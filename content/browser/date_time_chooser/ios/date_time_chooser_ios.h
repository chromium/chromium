// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_IOS_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_IOS_H_

#include "content/browser/date_time_chooser/date_time_chooser.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-forward.h"

@class DateTimeChooserCoordinator;

namespace content {

// IOS implementation for DateTimeChooser dialogs.
class CONTENT_EXPORT DateTimeChooserIOS : public DateTimeChooser {
 public:
  explicit DateTimeChooserIOS(WebContents* web_contents);
  ~DateTimeChooserIOS() override;

  DateTimeChooserIOS(const DateTimeChooserIOS&) = delete;
  DateTimeChooserIOS& operator=(const DateTimeChooserIOS&) = delete;

  // Called when a dialog is closed by a user action.
  void OnDialogClosed(bool success, double value);

 protected:
  // DateTimeChooser:
  void OpenPlatformDialog(blink::mojom::DateTimeDialogValuePtr value,
                          OpenDateTimeDialogCallback callback) override;
  void ClosePlatformDialog() override;

 private:
  DateTimeChooserCoordinator* __strong coordinator_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_IOS_H_
