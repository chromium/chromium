// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/date_time_chooser/date_time_chooser.h"

#include <string_view>

namespace content {

DateTimeChooser::DateTimeChooser(WebContents* web_contents)
    : WebContentsUserData<DateTimeChooser>(*web_contents) {}

DateTimeChooser::~DateTimeChooser() = default;

void DateTimeChooser::OnDateTimeChooserReceiver(
    mojo::PendingReceiver<blink::mojom::DateTimeChooser> receiver) {
  // Disconnect the previous picker first.
  date_time_chooser_receiver_.reset();
  date_time_chooser_receiver_.Bind(std::move(receiver));
  date_time_chooser_receiver_.set_disconnect_handler(
      base::BindOnce(&DateTimeChooser::OnDateTimeChooserReceiverConnectionError,
                     base::Unretained(this)));
}

void DateTimeChooser::OnDateTimeChooserReceiverConnectionError() {
  // Close a dialog and reset the Mojo receiver and the callback.
  CloseDateTimeDialog();
  open_date_time_response_callback_.Reset();
  date_time_chooser_receiver_.reset();
}

void DateTimeChooser::OpenDateTimeDialog(
    blink::mojom::DateTimeDialogValuePtr value,
    OpenDateTimeDialogCallback callback) {
  OpenPlatformDialog(std::move(value), std::move(callback));
}

void DateTimeChooser::CloseDateTimeDialog() {
  ClosePlatformDialog();
}

void DateTimeChooser::ReportBadMessage(std::string_view error) {
  date_time_chooser_receiver_.ReportBadMessage(error);
}

// static
DateTimeChooser* DateTimeChooser::GetDateTimeChooser(
    WebContents* web_contents) {
  return DateTimeChooser::FromWebContents(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DateTimeChooser);

}  // namespace content
