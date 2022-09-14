// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"

#import "base/time/time.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ReadingListListItemFactoryTest : public PlatformTest {
 public:
  ReadingListListItemFactoryTest()
      : PlatformTest(),
        entry_(GURL("https://www.google.com"), "Google", base::Time::Now()) {}

  ReadingListListItemFactoryTest(const ReadingListListItemFactoryTest&) =
      delete;
  ReadingListListItemFactoryTest& operator=(
      const ReadingListListItemFactoryTest&) = delete;

 protected:
  const ReadingListEntry entry_;
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
