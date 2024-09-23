// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

// Time to wait before closing the dialog and cancelling the connection.
constexpr base::TimeDelta kDialogTimeout = base::Minutes(1);

class It2MeConfirmationDialogLinux : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogLinux();

  It2MeConfirmationDialogLinux(const It2MeConfirmationDialogLinux&) = delete;
  It2MeConfirmationDialogLinux& operator=(const It2MeConfirmationDialogLinux&) =
      delete;

  ~It2MeConfirmationDialogLinux() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  // Creates a dialog window and makes it visible.
  void CreateWindow(const std::string& remote_user_email);

  // Destroys the dialog window (if created) and stop |dialog_timer_|.
  void Hide();

  // Handles user input from the dialog.
  void OnResponse(GtkDialog* dialog, int response_id);

  raw_ptr<GtkWidget> confirmation_window_ = nullptr;

  ResultCallback result_callback_;

  base::OneShotTimer dialog_timer_;

  ScopedGSignal signal_;
};

It2MeConfirmationDialogLinux::It2MeConfirmationDialogLinux() {}

It2MeConfirmationDialogLinux::~It2MeConfirmationDialogLinux() {
  Hide();
}

void It2MeConfirmationDialogLinux::Show(const std::string& remote_user_email,
                                        ResultCallback callback) {
  DCHECK(!remote_user_email.empty());
  DCHECK(callback);
  DCHECK(!result_callback_);

  result_callback_ = std::move(callback);

  CreateWindow(remote_user_email);

  dialog_timer_.Start(FROM_HERE, kDialogTimeout,
                      base::BindOnce(&It2MeConfirmationDialogLinux::OnResponse,
                                     base::Unretained(this),
                                     /*dialog=*/nullptr, GTK_RESPONSE_NONE));
}

void It2MeConfirmationDialogLinux::Hide() {
  dialog_timer_.Stop();

  if (confirmation_window_) {
    gtk_widget_destroy(confirmation_window_);
    confirmation_window_ = nullptr;
  }
}

void It2MeConfirmationDialogLinux::CreateWindow(
    const std::string& remote_user_email) {
  DCHECK(!confirmation_window_);

  confirmation_window_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str(),
      /*parent=*/nullptr, static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
      l10n_util::GetStringUTF8(IDS_SHARE_CONFIRM_DIALOG_DECLINE).c_str(),
      GTK_RESPONSE_CANCEL,
      l10n_util::GetStringUTF8(IDS_SHARE_CONFIRM_DIALOG_CONFIRM).c_str(),
      GTK_RESPONSE_OK,
      /*next_button=*/nullptr);

  gtk_dialog_set_default_response(GTK_DIALOG(confirmation_window_.get()),
                                  GTK_RESPONSE_CANCEL);

  gtk_window_set_resizable(GTK_WINDOW(confirmation_window_.get()), false);

  gtk_window_set_keep_above(GTK_WINDOW(confirmation_window_.get()), true);

  signal_ = ScopedGSignal(
      GTK_DIALOG(confirmation_window_.get()), "response",
      base::BindRepeating(&It2MeConfirmationDialogLinux::OnResponse,
                          base::Unretained(this)));

  GtkWidget* content_area =
      gtk_dialog_get_content_area(GTK_DIALOG(confirmation_window_.get()));

  std::u16string dialog_text =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME),
          remote_user_email);
  GtkWidget* text_label = gtk_label_new(base::UTF16ToUTF8(dialog_text).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(text_label), true);
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

  gtk_window_set_urgency_hint(GTK_WINDOW(confirmation_window_.get()), true);
  gtk_window_present(GTK_WINDOW(confirmation_window_.get()));
}

void It2MeConfirmationDialogLinux::OnResponse(GtkDialog* dialog,
                                              int response_id) {
  DCHECK(result_callback_);

  Hide();
  std::move(result_callback_)
      .Run((response_id == GTK_RESPONSE_OK) ? Result::OK : Result::CANCEL);
}

}  // namespace

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogLinux>();
}

}  // namespace remoting
