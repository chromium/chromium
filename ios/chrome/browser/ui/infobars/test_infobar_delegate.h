// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_DELEGATE_H_

#include <CoreFoundation/CoreFoundation.h>

#include "components/infobars/core/confirm_infobar_delegate.h"

// An infobar that displays `infobar_message` and one button.
class TestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit TestInfoBarDelegate(NSString* infobar_message);

  bool Create(infobars::InfoBarManager* infobar_manager);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  int GetButtons() const override;

 private:
  NSString* infobar_message_;
};

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_DELEGATE_H_
