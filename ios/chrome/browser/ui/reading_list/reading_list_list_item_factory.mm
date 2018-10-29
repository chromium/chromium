// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReadingListListItemFactory ()

// The factory supplying custom accessibility actions to the items.
@property(nonatomic, readonly, strong)
    ReadingListListItemCustomActionFactory* customActionFactory;

@end

@implementation ReadingListListItemFactory
@synthesize customActionFactory = _customActionFactory;

- (instancetype)init {
  if (self = [super init]) {
    _customActionFactory =
        [[ReadingListListItemCustomActionFactory alloc] init];
  }
  return self;
}

#pragma mark Accessors

- (void)setAccessibilityDelegate:
    (id<ReadingListListItemAccessibilityDelegate>)accessibilityDelegate {
  self.customActionFactory.accessibilityDelegate = accessibilityDelegate;
}

- (id<ReadingListListItemAccessibilityDelegate>)accessibilityDelegate {
  return self.customActionFactory.accessibilityDelegate;
}

#pragma mark Public

- (ListItem<ReadingListListItem>*)cellItemForReadingListEntry:
    (const ReadingListEntry*)entry {
  ListItem<ReadingListListItem>* item =
      [[ReadingListTableViewItem alloc] initWithType:0];
  item.title = base::SysUTF8ToNSString(entry->Title());
  const GURL& URL = entry->URL();
  item.entryURL = URL;
  item.faviconPageURL =
      entry->DistilledURL().is_valid() ? entry->DistilledURL() : URL;
  item.distillationState =
      reading_list::UIStatusFromModelStatus(entry->DistilledState());
  BOOL hasDistillationDetails =
      entry->DistilledState() == ReadingListEntry::PROCESSED &&
      entry->DistillationSize() != 0 && entry->DistillationTime() != 0;
  int64_t distillationDate =
      hasDistillationDetails ? entry->DistillationTime() : 0;
  item.distillationDateText =
      GetReadingListCellDistillationDateText(distillationDate);
  int64_t distillationSize =
      hasDistillationDetails ? entry->DistillationSize() : 0;
  item.distillationSizeText =
      GetReadingListCellDistillationSizeText(distillationSize);
  item.customActionFactory = self.customActionFactory;
  return item;
}

@end
