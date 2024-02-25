// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_IOS_H_

#include <memory>

#include "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"

#include "base/strings/utf_string_conversions.h"

class FakeInfobarDelegate;

// Fake version of InfoBarIOS set up with fake delegates to use in tests.
class FakeInfobarIOS : public InfoBarIOS {
 public:
  // Creates a FakeInfobarIOS with `type` that has a delegate that uses
  // `message_text` as its message.
  FakeInfobarIOS(InfobarType type = InfobarType::kInfobarTypeConfirm,
                 std::u16string message_text = u"FakeInfobar");
  // Creates a FakeInfobarIOS with `fake_delegate`. Uses
  // InfobarType::kInfobarTypeConfirm as a default type value.}
  FakeInfobarIOS(std::unique_ptr<FakeInfobarDelegate> fake_delegate);
  ~FakeInfobarIOS() override;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_TEST_FAKE_INFOBAR_IOS_H_
