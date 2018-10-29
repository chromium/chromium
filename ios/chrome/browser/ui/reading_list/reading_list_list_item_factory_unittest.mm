// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"

#include "base/time/time.h"
#include "components/reading_list/core/reading_list_entry.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ReadingListListItemFactoryTest : public PlatformTest {
 public:
  ReadingListListItemFactoryTest()
      : PlatformTest(),
        entry_(GURL("https://www.google.com"), "Google", base::Time::Now()) {}

 protected:
  const ReadingListEntry entry_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadingListListItemFactoryTest);
};

// Tests that the accessibility delegate is properly passed to the generated
// ListItems.
TEST_F(ReadingListListItemFactoryTest, SetA11yDelegate) {
  id<ReadingListListItemAccessibilityDelegate> mockDelegate =
      OCMProtocolMock(@protocol(ReadingListListItemAccessibilityDelegate));
  ReadingListListItemFactory* factory =
      [[ReadingListListItemFactory alloc] init];
  factory.accessibilityDelegate = mockDelegate;
  id<ReadingListListItem> item = [factory cellItemForReadingListEntry:&entry_];
  EXPECT_EQ(item.customActionFactory.accessibilityDelegate, mockDelegate);
}
