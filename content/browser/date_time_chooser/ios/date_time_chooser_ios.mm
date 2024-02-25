// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_ios.h"

#import <UIKit/UIKit.h>

#import "content/browser/date_time_chooser/ios/date_time_chooser_coordinator.h"
#import "ui/gfx/native_widget_types.h"

namespace content {

// DateTimeChooserIOS implementation
DateTimeChooserIOS::DateTimeChooserIOS(WebContents* web_contents)
    : DateTimeChooser(web_contents) {}

DateTimeChooserIOS::~DateTimeChooserIOS() = default;

void DateTimeChooserIOS::OnDialogClosed(bool success, double value) {
  if (open_date_time_response_callback_) {
    std::move(open_date_time_response_callback_).Run(success, value);
  }
  ClosePlatformDialog();
}

void DateTimeChooserIOS::OpenPlatformDialog(
    blink::mojom::DateTimeDialogValuePtr value,
    OpenDateTimeDialogCallback callback) {
  open_date_time_response_callback_ = std::move(callback);
  gfx::NativeWindow gfx_window = GetWebContents().GetTopLevelNativeWindow();
  coordinator_ = [[DateTimeChooserCoordinator alloc]
      initWithDateTimeChooser:this
                      configs:std::move(value)
           baseViewController:gfx_window.Get().rootViewController];
}

void DateTimeChooserIOS::ClosePlatformDialog() {
  coordinator_ = nil;
}

// static
void DateTimeChooser::CreateDateTimeChooser(WebContents* web_contents) {
  web_contents->SetUserData(UserDataKey(),
                            std::make_unique<DateTimeChooserIOS>(web_contents));
}

}  // namespace content
