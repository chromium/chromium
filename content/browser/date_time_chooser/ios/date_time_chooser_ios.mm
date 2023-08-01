// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_ios.h"

namespace content {

// DateTimeChooserIOS implementation
DateTimeChooserIOS::DateTimeChooserIOS(WebContents* web_contents)
    : DateTimeChooser(web_contents) {}

DateTimeChooserIOS::~DateTimeChooserIOS() = default;

void DateTimeChooserIOS::OpenPlatformDialog(
    blink::mojom::DateTimeDialogValuePtr value,
    OpenDateTimeDialogCallback callback) {
  // TODO(crbug.com/1461947): Create a coordninator to handle UI components.
  std::move(callback).Run(false, 0);
}

void DateTimeChooserIOS::ClosePlatformDialog() {
  // TODO(crbug.com/1461947): Close a dialog and clean it up.
}

// static
void DateTimeChooser::CreateDateTimeChooser(WebContents* web_contents) {
  web_contents->SetUserData(UserDataKey(),
                            std::make_unique<DateTimeChooserIOS>(web_contents));
}

}  // namespace content
