// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <gtk/gtk.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

class ContinueWindowGtk : public ContinueWindow {
 public:
  ContinueWindowGtk();

  ContinueWindowGtk(const ContinueWindowGtk&) = delete;
  ContinueWindowGtk& operator=(const ContinueWindowGtk&) = delete;

  ~ContinueWindowGtk() override;

 protected:
  // ContinueWindow overrides.
  void ShowUi() override;
  void HideUi() override;

 private:
  void CreateWindow();

  void OnResponse(GtkDialog*, int);

  raw_ptr<GtkWidget> continue_window_;

  ScopedGSignal signal_;
};

ContinueWindowGtk::ContinueWindowGtk() : continue_window_(nullptr) {}

ContinueWindowGtk::~ContinueWindowGtk() {
  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = nullptr;
  }
}

void ContinueWindowGtk::ShowUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!continue_window_);

  CreateWindow();
  gtk_window_set_urgency_hint(GTK_WINDOW(continue_window_.get()), TRUE);
  gtk_window_present(GTK_WINDOW(continue_window_.get()));
}

void ContinueWindowGtk::HideUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = nullptr;
  }
}

void ContinueWindowGtk::CreateWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!continue_window_);

  continue_window_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str(), nullptr,
      GTK_DIALOG_MODAL,
      l10n_util::GetStringUTF8(IDS_STOP_SHARING_BUTTON).c_str(),
      GTK_RESPONSE_CANCEL,
      l10n_util::GetStringUTF8(IDS_CONTINUE_BUTTON).c_str(), GTK_RESPONSE_OK,
      nullptr);

  gtk_dialog_set_default_response(GTK_DIALOG(continue_window_.get()),
                                  GTK_RESPONSE_OK);
  gtk_window_set_resizable(GTK_WINDOW(continue_window_.get()), FALSE);

  // Set always-on-top, otherwise this window tends to be obscured by the
  // DisconnectWindow.
  gtk_window_set_keep_above(GTK_WINDOW(continue_window_.get()), TRUE);

  signal_ = ScopedGSignal(GTK_DIALOG(continue_window_.get()), "response",
                          base::BindRepeating(&ContinueWindowGtk::OnResponse,
                                              base::Unretained(this)));

  GtkWidget* content_area =
      gtk_dialog_get_content_area(GTK_DIALOG(continue_window_.get()));

  GtkWidget* text_label =
      gtk_label_new(l10n_util::GetStringUTF8(IDS_CONTINUE_PROMPT).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
  // TODO(lambroslambrou): Fix magic numbers, as in disconnect_window_gtk.cc.
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_margin_start(GTK_WIDGET(text_label), 12);
  gtk_widget_set_margin_end(GTK_WIDGET(text_label), 12);
  gtk_widget_set_margin_top(GTK_WIDGET(text_label), 12);
  gtk_widget_set_margin_bottom(GTK_WIDGET(text_label), 12);
#else
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_misc_set_padding(GTK_MISC(text_label), 12, 12);
  G_GNUC_END_IGNORE_DEPRECATIONS;
#endif
  gtk_container_add(GTK_CONTAINER(content_area), text_label);

#if !GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_show_all(content_area);
#endif
}

void ContinueWindowGtk::OnResponse(GtkDialog* dialog, int response_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response_id == GTK_RESPONSE_OK) {
    ContinueSession();
  } else {
    DisconnectSession();
  }

  HideUi();
}

// static
std::unique_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return std::make_unique<ContinueWindowGtk>();
}

}  // namespace remoting
