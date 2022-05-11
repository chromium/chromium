// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"

#include "base/i18n/time_formatting.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the symbol badge image.
NSInteger kSymbolBadgeImagePointSize = 13;

// The string format used to append the distillation date to the URL host.
NSString* const kURLAndDistillationDateFormat = @"%@ • %@";

}  // namespace

@interface ReadingListTableViewItem ()

// The image to supply as to the TableViewURLCell's |faviconBadgeView|.
@property(nonatomic, strong) UIImage* distillationBadgeImage;

@end

@implementation ReadingListTableViewItem
@synthesize title = _title;
@synthesize entryURL = _entryURL;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize distillationState = _distillationState;
@synthesize distillationSizeText = _distillationSizeText;
@synthesize distillationDateText = _distillationDateText;
@synthesize estimatedReadTimeText = _estimatedReadTimeText;
@synthesize customActionFactory = _customActionFactory;
@synthesize attributes = _attributes;
@synthesize distillationBadgeImage = _distillationBadgeImage;

- (instancetype)initWithType:(NSInteger)type {
  if (self = [super initWithType:type]) {
    self.cellClass = [TableViewURLCell class];
  }
  return self;
}

#pragma mark - Accessors

- (void)setDistillationState:
    (ReadingListUIDistillationStatus)distillationState {
  if (_distillationState == distillationState)
    return;
  _distillationState = distillationState;
  switch (_distillationState) {
    case ReadingListUIDistillationStatusFailure:
      self.distillationBadgeImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(kFailMarkCircleFillSymbol,
                                                   kSymbolBadgeImagePointSize)
              : [UIImage imageNamed:@"distillation_fail_new"];
      break;
    case ReadingListUIDistillationStatusSuccess:
      self.distillationBadgeImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(kCheckMarkCircleFillSymbol,
                                                   kSymbolBadgeImagePointSize)
              : [UIImage imageNamed:@"table_view_cell_check_mark"];
      break;
    case ReadingListUIDistillationStatusPending:
      self.distillationBadgeImage = nil;
      break;
  }
}

#pragma mark - ListItem

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
  URLCell.titleLabel.text = [self titleLabelText];
  URLCell.URLLabel.text = [self URLLabelText];
  URLCell.metadataLabel.text = self.distillationSizeText;
  URLCell.cellUniqueIdentifier = base::SysUTF8ToNSString(self.entryURL.host());
  URLCell.accessibilityTraits |= UIAccessibilityTraitButton;

  if (styler.cellTitleColor)
    URLCell.titleLabel.textColor = styler.cellTitleColor;
  [URLCell.faviconView configureWithAttributes:self.attributes];
  URLCell.faviconBadgeView.image = self.distillationBadgeImage;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = GetReadingListCellAccessibilityLabel(
      self.title, [self hostname], self.distillationState);
  cell.accessibilityCustomActions =
      [self.customActionFactory customActionsForItem:self];
  [URLCell configureUILayout];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"Reading List item \"%@\" for url %@",
                                    self.title, [self hostname]];
}

- (BOOL)isEqual:(id)other {
  return AreReadingListListItemsEqual(self, other);
}

#pragma mark Private

// Returns the text to use when configuring a TableViewURLCell's title label.
- (NSString*)titleLabelText {
  return self.title.length ? self.title : self.hostname;
}

// Returns the text to use when configuring a TableViewURLCell's URL label.
- (NSString*)URLLabelText {
  // If there's no title text, the URL is used as the cell title.  Simply
  // display the distillation date in the URL label when this occurs.
  if (!self.title.length)
    return self.distillationDateText;

  // Append the hostname with the distillation date if it exists.
  if (self.distillationDateText.length) {
    return
        [NSString stringWithFormat:kURLAndDistillationDateFormat,
                                   [self hostname], self.distillationDateText];
  } else {
    return [self hostname];
  }
}

- (NSString*)hostname {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              self.entryURL));
}

@end
