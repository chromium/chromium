// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"

// Fake version of InfoBarDelegate.
class FakeInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  FakeInfobarDelegate();
  FakeInfobarDelegate(std::u16string message_text);
  FakeInfobarDelegate(std::u16string title_text, std::u16string message_text);
  FakeInfobarDelegate(infobars::InfoBarDelegate::InfoBarIdentifier identifier);
  ~FakeInfobarDelegate() override;

  // Returns |identifier_|, set during construction.
  InfoBarIdentifier GetIdentifier() const override;

  // Returns the message string to be displayed for the Infobar.
  std::u16string GetTitleText() const override;

  // Returns the message string to be displayed for the Infobar.
  std::u16string GetMessageText() const override;

 private:
  FakeInfobarDelegate(infobars::InfoBarDelegate::InfoBarIdentifier identifier,
                      std::u16string title_text,
                      std::u16string message_text);
  infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
  std::u16string title_text_;
  std::u16string message_text_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_
