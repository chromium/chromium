// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"

TestInfoBarDelegate::TestInfoBarDelegate(NSString* infobar_message)
    : infobar_message_(infobar_message) {}

bool TestInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);
  return !!infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(this)));
}

TestInfoBarDelegate::InfoBarIdentifier TestInfoBarDelegate::GetIdentifier()
    const {
  return TEST_INFOBAR;
}

std::u16string TestInfoBarDelegate::GetMessageText() const {
  return base::SysNSStringToUTF16(infobar_message_);
}

int TestInfoBarDelegate::GetButtons() const {
  return ConfirmInfoBarDelegate::BUTTON_OK;
}
