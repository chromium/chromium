// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "ui/strings/grit/ui_strings.h"
#import "url/gurl.h"

namespace {

// The size of the symbol badge image.
constexpr CGFloat kSymbolBadgeImagePointSize = 13;

// The string format used to append the distillation date to the URL host.
NSString* const kURLAndDistillationDateFormat = @"%@ • %@";

}  // namespace

@interface ReadingListTableViewItem ()

// The image to supply as to the TableViewURLCell's `faviconBadgeView`.
@property(nonatomic, strong) UIImage* distillationBadgeImage;

// The color to supply as to the TableViewURLCell's `tintColor`.
@property(nonatomic, strong) UIColor* distillationBadgeTintColor;

@end

@implementation ReadingListTableViewItem
@synthesize title = _title;
@synthesize entryURL = _entryURL;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize distillationState = _distillationState;
@synthesize distillationDateText = _distillationDateText;
@synthesize estimatedReadTimeText = _estimatedReadTimeText;
@synthesize showCloudSlashIcon = _showCloudSlashIcon;
@synthesize customActionFactory = _customActionFactory;
@synthesize attributes = _attributes;

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
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
      self.distillationBadgeImage = DefaultSymbolTemplateWithPointSize(
          kErrorCircleFillSymbol, kSymbolBadgeImagePointSize);
      self.distillationBadgeTintColor = [UIColor colorNamed:kGrey600Color];
      break;
    case ReadingListUIDistillationStatusSuccess:
      self.distillationBadgeImage = DefaultSymbolTemplateWithPointSize(
          kCheckmarkCircleFillSymbol, kSymbolBadgeImagePointSize);
      self.distillationBadgeTintColor = [UIColor colorNamed:kGreen500Color];
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
  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  URLCell.titleLabel.text = [self titleLabelText];
  URLCell.URLLabel.text = [self URLLabelText];
  URLCell.cellUniqueIdentifier = base::SysUTF8ToNSString(self.entryURL.host());
  URLCell.accessibilityTraits |= UIAccessibilityTraitButton;
  URLCell.metadataImage.image =
      self.showCloudSlashIcon
          ? CustomSymbolWithPointSize(kCloudSlashSymbol,
                                      kCloudSlashSymbolPointSize)
          : nil;
  URLCell.metadataImage.tintColor = CloudSlashTintColor();
  if (styler.cellTitleColor)
    URLCell.titleLabel.textColor = styler.cellTitleColor;
  [URLCell.faviconView configureWithAttributes:self.attributes];
  URLCell.faviconBadgeView.image = self.distillationBadgeImage;
  URLCell.faviconBadgeView.tintColor = self.distillationBadgeTintColor;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = GetReadingListCellAccessibilityLabel(
      self.title, [self hostname], self.distillationState,
      self.showCloudSlashIcon);
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
