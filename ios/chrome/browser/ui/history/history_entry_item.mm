// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_entry_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/history/history_entry_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - HistoryEntryItem

@interface HistoryEntryItem ()
// Delegate to perform custom accessibility actions.
@property(nonatomic, weak) id<HistoryEntryItemDelegate> accessibilityDelegate;

// Custom accessibility actions for the history entry cell.
- (NSArray*)accessibilityActions;
@end

@implementation HistoryEntryItem
@synthesize accessibilityDelegate = _accessibilityDelegate;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize timeText = _timeText;
@synthesize URL = _URL;
@synthesize timestamp = _timestamp;

- (instancetype)initWithType:(NSInteger)type
       accessibilityDelegate:(id<HistoryEntryItemDelegate>)delegate {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewURLCell class];
    _accessibilityDelegate = delegate;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewURLCell* cell =
      base::mac::ObjCCastStrict<TableViewURLCell>(tableCell);
  cell.cellUniqueIdentifier = self.uniqueIdentifier;
  cell.titleLabel.text = self.text;
  cell.URLLabel.text = self.detailText;
  cell.metadataLabel.text = self.timeText;
  cell.isAccessibilityElement = YES;
  cell.accessibilityCustomActions = self.accessibilityActions;
  [cell configureUILayout];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.URL.host());
}

#pragma mark - Accessibility

- (NSArray*)accessibilityActions {
  UIAccessibilityCustomAction* deleteAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_HISTORY_ENTRY_ACCESSIBILITY_DELETE)
                target:self
              selector:@selector(deleteHistoryEntry)];
  UIAccessibilityCustomAction* openInNewTabAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                target:self
              selector:@selector(openInNewTab)];
  UIAccessibilityCustomAction* openInNewIncognitoTabAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                target:self
              selector:@selector(openInNewIncognitoTab)];
  UIAccessibilityCustomAction* copyURLAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY)
                target:self
              selector:@selector(copyURL)];
  return @[
    deleteAction, openInNewTabAction, openInNewIncognitoTabAction, copyURLAction
  ];
}

- (void)deleteHistoryEntry {
  [self.accessibilityDelegate historyEntryItemDidRequestDelete:self];
}

- (void)openInNewTab {
  [self.accessibilityDelegate historyEntryItemDidRequestOpenInNewTab:self];
}

- (void)openInNewIncognitoTab {
  [self.accessibilityDelegate
      historyEntryItemDidRequestOpenInNewIncognitoTab:self];
}

- (void)copyURL {
  [self.accessibilityDelegate historyEntryItemDidRequestCopy:self];
}

#pragma mark - NSObject

- (BOOL)isEqualToHistoryEntryItem:(HistoryEntryItem*)item {
  return item && item.URL == _URL && item.timestamp == _timestamp;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;

  if (![object isMemberOfClass:[HistoryEntryItem class]])
    return NO;

  return [self isEqualToHistoryEntryItem:object];
}

- (NSUInteger)hash {
  return [base::SysUTF8ToNSString(self.URL.spec()) hash] ^
         self.timestamp.since_origin().InMicroseconds();
}

@end
