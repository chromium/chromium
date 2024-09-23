// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/time_format.h"

@interface ReadingListListItemFactory ()

// The factory supplying custom accessibility actions to the items.
@property(nonatomic, readonly, strong)
    ReadingListListItemCustomActionFactory* customActionFactory;

@end

@implementation ReadingListListItemFactory
@synthesize customActionFactory = _customActionFactory;

- (instancetype)init {
  if ((self = [super init])) {
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

- (void)setDelegate:(id<ReadingListListItemFactoryDelegate>)delegate {
  self.customActionFactory.incognitoDelegate = delegate;
}

- (id<ReadingListListItemFactoryDelegate>)delegate {
  return self.customActionFactory.incognitoDelegate;
}

#pragma mark Public

- (ListItem<ReadingListListItem>*)
    cellItemForReadingListEntry:(const ReadingListEntry*)entry
            needsExplicitUpload:(BOOL)needsExplicitUpload {
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
      entry->DistillationTime() != 0;
  int64_t distillationDate =
      hasDistillationDetails ? entry->DistillationTime() : 0;
  item.distillationDateText =
      GetReadingListCellDistillationDateText(distillationDate);
  item.showCloudSlashIcon = needsExplicitUpload;
  item.customActionFactory = self.customActionFactory;
  return item;
}

@end
