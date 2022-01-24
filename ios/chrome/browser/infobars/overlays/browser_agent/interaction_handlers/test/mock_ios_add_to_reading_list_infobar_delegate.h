// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_

#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"

class MockIOSAddToReadingListInfobarDelegate
    : public IOSAddToReadingListInfobarDelegate {
 public:
  MockIOSAddToReadingListInfobarDelegate(ReadingListModel* model,
                                         web::WebState* web_state);
  ~MockIOSAddToReadingListInfobarDelegate() override;

  MOCK_METHOD0(Accept, bool());
  MOCK_METHOD0(NeverShow, void());
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_IOS_ADD_TO_READING_LIST_INFOBAR_DELEGATE_H_
