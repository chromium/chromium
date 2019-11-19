// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a dialog allowing the user to pick between installed
// X session types. It finds sessions by looking for .desktop files in
// /etc/X11/sessions and in the xsessions folder (if any) in each XDG system
// data directory. (By default, this will be /usr/local/share/xsessions and
// /usr/share/xsessions.) Once the user selects a session, it will be launched
// via /etc/X11/Xsession. There will additionally be an "Xsession" will will
// invoke Xsession without arguments to launch a "default" session based on the
// system's configuration. If no session .desktop files are found, this will be
// the only option present.

#include <gtk/gtk.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/logging.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/base/l10n/l10n_util.h"

#include "remoting/host/xsession_chooser_ui.inc"

namespace remoting {

namespace {

const char XSESSION_SCRIPT[] = "/etc/X11/Xsession";

struct XSession {
  std::string name;
  std::string comment;
  std::vector<std::string> desktop_names;
  std::string exec;
};

class SessionDialog {
 public:
  SessionDialog(std::vector<XSession> choices,
                base::OnceCallback<void(XSession)> callback,
                base::OnceClosure cancel_callback)
      : choices_(std::move(choices)),
        callback_(std::move(callback)),
        cancel_callback_(std::move(cancel_callback)),
        ui_(gtk_builder_new_from_string(UI, -1)) {
    gtk_label_set_text(
        GTK_LABEL(gtk_builder_get_object(ui_, "message")),
        l10n_util::GetStringUTF8(IDS_SESSION_DIALOG_MESSAGE).c_str());
    gtk_tree_view_column_set_title(
        GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(ui_, "name_column")),
        l10n_util::GetStringUTF8(IDS_SESSION_DIALOG_NAME_COLUMN).c_str());
    gtk_tree_view_column_set_title(
        GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(ui_, "comment_column")),
        l10n_util::GetStringUTF8(IDS_SESSION_DIALOG_COMMENT_COLUMN).c_str());

    GtkListStore* session_store =
        GTK_LIST_STORE(gtk_builder_get_object(ui_, "session_store"));
    for (std::size_t i = 0; i < choices_.size(); ++i) {
      GtkTreeIter iter;
      gtk_list_store_append(session_store, &iter);
      // gtk_list_store_set makes its own internal copy of the strings.
      gtk_list_store_set(session_store, &iter,
                         INDEX_COLUMN, static_cast<guint>(i),
                         NAME_COLUMN, choices_[i].name.c_str(),
                         COMMENT_COLUMN, choices_[i].comment.c_str(),
                         -1);
    }

    g_signal_connect(gtk_builder_get_object(ui_, "session_list"),
                     "row-activated", G_CALLBACK(OnRowActivatedThunk), this);
    g_signal_connect(gtk_builder_get_object(ui_, "ok_button"), "clicked",
                     G_CALLBACK(OnOkClickedThunk), this);
    g_signal_connect(gtk_builder_get_object(ui_, "dialog"), "delete-event",
                     G_CALLBACK(OnCloseThunk), this);
  }

  void Show() {
    gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(ui_, "dialog")));
  }

 private:
  void ActivateChoice(std::size_t index) {
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(ui_, "dialog")));
    if (callback_) {
      std::move(callback_).Run(std::move(choices_.at(index)));
    }
  }

  CHROMEG_CALLBACK_2(SessionDialog,
                     void,
                     OnRowActivated,
                     GtkTreeView*,
                     GtkTreePath*,
                     GtkTreeViewColumn*);
  CHROMEG_CALLBACK_0(SessionDialog, void, OnOkClicked, GtkButton*);
  CHROMEG_CALLBACK_1(SessionDialog, gboolean, OnClose, GtkWidget*, GdkEvent*);

  enum Columns { INDEX_COLUMN, NAME_COLUMN, COMMENT_COLUMN, NUM_COLUMNS };
  std::vector<XSession> choices_;
  base::OnceCallback<void(XSession)> callback_;
  base::OnceClosure cancel_callback_;
  ScopedGObject<GtkBuilder> ui_;

  SessionDialog(const SessionDialog&) = delete;
  SessionDialog& operator=(const SessionDialog&) = delete;
};

void SessionDialog::OnRowActivated(GtkTreeView* session_list,
                                   GtkTreePath* path,
                                   GtkTreeViewColumn*) {
  GtkTreeModel* model = gtk_tree_view_get_model(session_list);
  GtkTreeIter iter;
  guint index;
  if (!gtk_tree_model_get_iter(model, &iter, path)) {
    // Strange, but the user should still be able to click OK to progress.
    return;
  }
  gtk_tree_model_get(model, &iter, INDEX_COLUMN, &index, -1);
  ActivateChoice(index);
}

void SessionDialog::OnOkClicked(GtkButton*) {
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(gtk_builder_get_object(ui_, "session_list")));
  GtkTreeModel* model;
  GtkTreeIter iter;
  guint index;
  if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
    // Nothing selected, so do nothing. Note that the selection mode is set to
    // "browse", which should, under most circumstances, ensure that exactly one
    // item is selected, preventing this from being reached. However, it does
    // not completely guarantee that it can never happen.
    return;
  }
  gtk_tree_model_get(model, &iter, INDEX_COLUMN, &index, -1);
  ActivateChoice(index);
}

gboolean SessionDialog::OnClose(GtkWidget* dialog, GdkEvent*) {
  gtk_widget_hide(dialog);
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
  return true;
}

base::Optional<XSession> TryLoadSession(base::FilePath path) {
  std::unique_ptr<GKeyFile, void (*)(GKeyFile*)> key_file(g_key_file_new(),
                                                          &g_key_file_free);
  GError* error;

  if (!g_key_file_load_from_file(key_file.get(), path.value().c_str(),
                                 G_KEY_FILE_NONE, &error)) {
    LOG(WARNING) << "Failed to load " << path << ": " << error->message;
    g_error_free(error);
    return base::nullopt;
  }

  // Files without a "Desktop Entry" group can be ignored. (An empty file can be
  // put in a higher-priority directory to hide entries from a lower-priority
  // directory.)
  if (!g_key_file_has_group(key_file.get(), G_KEY_FILE_DESKTOP_GROUP)) {
    return base::nullopt;
  }

  // Files with "NoDisplay" or "Hidden" set should be ignored.
  for (const char* key :
       {G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, G_KEY_FILE_DESKTOP_KEY_HIDDEN}) {
    if (g_key_file_get_boolean(key_file.get(), G_KEY_FILE_DESKTOP_GROUP, key,
                               nullptr)) {
      return base::nullopt;
    }
  }

  // If there's a "TryExec" key, we need to check if the specified path is
  // executable and ignore the entry if not. (However, we should not try to
  // actually execute the specified path.)
  if (gchar* try_exec =
          g_key_file_get_string(key_file.get(), G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, nullptr)) {
    base::FilePath try_exec_path(
        base::TrimWhitespaceASCII(try_exec, base::TRIM_ALL));
    g_free(try_exec);

    if (try_exec_path.IsAbsolute()
            ? access(try_exec_path.value().c_str(), X_OK) != 0
            : !base::ExecutableExistsInPath(base::Environment::Create().get(),
                                            try_exec_path.value())) {
      LOG(INFO) << "Rejecting " << path << " due to TryExec=" << try_exec_path;
      return base::nullopt;
    }
  }

  XSession session;
  // Required fields.
  if (gchar* localized_name = g_key_file_get_locale_string(
          key_file.get(), G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME,
          nullptr, nullptr)) {
    session.name = localized_name;
    g_free(localized_name);
  } else {
    LOG(WARNING) << "Failed to load value of " << G_KEY_FILE_DESKTOP_KEY_NAME
                 << " from " << path;
    return base::nullopt;
  }

  if (gchar* exec =
          g_key_file_get_string(key_file.get(), G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_EXEC, nullptr)) {
    session.exec = exec;
    g_free(exec);
  } else {
    LOG(WARNING) << "Failed to load value of " << G_KEY_FILE_DESKTOP_KEY_EXEC
                 << " from " << path;
    return base::nullopt;
  }

  // Optional fields.
  if (gchar* localized_comment = g_key_file_get_locale_string(
          key_file.get(), G_KEY_FILE_DESKTOP_GROUP,
          G_KEY_FILE_DESKTOP_KEY_COMMENT, nullptr, nullptr)) {
    session.comment = localized_comment;
    g_free(localized_comment);
  }

  // DesktopNames does not yet have a constant in glib.
  if (gchar** desktop_names =
          g_key_file_get_string_list(key_file.get(), G_KEY_FILE_DESKTOP_GROUP,
                                     "DesktopNames", nullptr, nullptr)) {
    for (std::size_t i = 0; desktop_names[i]; ++i) {
      session.desktop_names.push_back(desktop_names[i]);
    }
    g_strfreev(desktop_names);
  }

  return session;
}

std::vector<XSession> CollectXSessions() {
  std::vector<base::FilePath> session_search_dirs;
  session_search_dirs.emplace_back("/etc/X11/sessions");

  // Returned list is owned by GLib and should not be modified or freed.
  const gchar* const* system_data_dirs = g_get_system_data_dirs();
  // List is null-terminated.
  for (std::size_t i = 0; system_data_dirs[i]; ++i) {
    session_search_dirs.push_back(
        base::FilePath(system_data_dirs[i]).Append("xsessions"));
  }

  std::map<base::FilePath, base::FilePath> session_files;

  for (const base::FilePath& search_dir : session_search_dirs) {
    base::FileEnumerator file_enumerator(search_dir, false /* recursive */,
                                         base::FileEnumerator::FILES,
                                         "*.desktop");
    base::FilePath session_path;
    while (!(session_path = file_enumerator.Next()).empty()) {
      base::FilePath basename = session_path.BaseName().RemoveFinalExtension();
      // Files in higher-priority directory should shadow those from lower-
      // priority directories. Emplace will only insert if an entry with the
      // same basename wasn't found in a previous directory.
      session_files.emplace(basename, session_path);
    }
  }

  std::vector<XSession> sessions;

  // Ensure there's always at least one session.
  sessions.push_back(
      {l10n_util::GetStringUTF8(IDS_SESSION_DIALOG_DEFAULT_SESSION_NAME),
       l10n_util::GetStringUTF8(IDS_SESSION_DIALOG_DEFAULT_SESSION_COMMENT),
       {},
       "default"});

  for (const auto& session : session_files) {
    base::Optional<XSession> loaded_session = TryLoadSession(session.second);
    if (loaded_session) {
      sessions.push_back(std::move(*loaded_session));
    }
  }

  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(err));
  if (U_SUCCESS(err)) {
    std::sort(sessions.begin() + 1, sessions.end(),
              [&](const XSession& first, const XSession& second) {
                UErrorCode err = U_ZERO_ERROR;
                UCollationResult result = collator->compare(
                    icu::UnicodeString::fromUTF8(first.name),
                    icu::UnicodeString::fromUTF8(second.name), err);
                // The icu documentation isn't clear under what circumstances
                // this can fail. base::i18n::CompareString16WithCollator just
                // does a DCHECK of the result, so do the same here for now.
                DCHECK(U_SUCCESS(err));
                return result == UCOL_LESS;
              });
  } else {
    LOG(WARNING) << "Error creating collator. Not sorting list. ("
                 << u_errorName(err) << ")";
  }

  return sessions;
}

void ExecXSession(base::OnceClosure quit_closure, XSession session) {
  LOG(INFO) << "Running " << XSESSION_SCRIPT << " " << session.exec;
  if (!session.desktop_names.empty()) {
    std::unique_ptr<base::Environment> environment =
        base::Environment::Create();
    environment->SetVar("XDG_CURRENT_DESKTOP",
                        base::JoinString(session.desktop_names, ":"));
  }
  execl(XSESSION_SCRIPT, XSESSION_SCRIPT, session.exec.c_str(), nullptr);
  PLOG(ERROR) << "Failed to exec XSession";
  std::move(quit_closure).Run();
}

}  // namespace

int XSessionChooserMain() {
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_init();
#else
  gtk_init(nullptr, nullptr);
#endif

  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;

  SessionDialog dialog(CollectXSessions(),
                       base::BindOnce(&ExecXSession, run_loop.QuitClosure()),
                       run_loop.QuitClosure());
  dialog.Show();

  run_loop.Run();

  // Control only gets to here if something went wrong.
  return 1;
}

}  // namespace remoting
