// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_table_view_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_constants.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
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

@implementation ReadingListTableViewItem {
  UIImage* _distillationBadgeImage;
}

@synthesize title = _title;
@synthesize entryURL = _entryURL;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize distillationState = _distillationState;
@synthesize distillationDateText = _distillationDateText;
@synthesize showCloudSlashIcon = _showCloudSlashIcon;
@synthesize customActionFactory = _customActionFactory;
@synthesize attributes = _attributes;

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

#pragma mark - Accessors

- (void)setDistillationState:
    (ReadingListUIDistillationStatus)distillationState {
  if (_distillationState == distillationState) {
    return;
  }
  _distillationState = distillationState;
  switch (_distillationState) {
    case ReadingListUIDistillationStatusFailure:
      _distillationBadgeImage = SymbolWithPalette(
          DefaultSymbolTemplateWithPointSize(kErrorCircleFillSymbol,
                                             kSymbolBadgeImagePointSize),
          @[ [UIColor colorNamed:kGrey600Color] ]);
      break;
    case ReadingListUIDistillationStatusSuccess:
      _distillationBadgeImage = SymbolWithPalette(
          DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol,
                                             kSymbolBadgeImagePointSize),
          @[ [UIColor colorNamed:kGreen500Color] ]);
      break;
    case ReadingListUIDistillationStatusPending:
      _distillationBadgeImage = nil;
      break;
  }
}

#pragma mark - ListItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = [self titleLabelText];
  configuration.subtitle = [self URLLabelText];

  FaviconContentConfiguration* faviconConfiguration =
      [[FaviconContentConfiguration alloc] init];
  faviconConfiguration.faviconAttributes = self.attributes;

  faviconConfiguration.badgeImage = _distillationBadgeImage;
  faviconConfiguration.badgeAccessibilityID = kReadingListItemBadgeID;

  configuration.leadingConfiguration = faviconConfiguration;

  if (self.showCloudSlashIcon) {
    ImageContentConfiguration* imageConfiguration =
        [[ImageContentConfiguration alloc] init];
    imageConfiguration.image =
        SymbolWithPalette(CustomSymbolWithPointSize(kCloudSlashSymbol,
                                                    kCloudSlashSymbolPointSize),
                          @[ CloudSlashTintColor() ]);
    imageConfiguration.accessibilityID = kReadingListLocalImageID;

    configuration.trailingConfiguration = imageConfiguration;
  }

  cell.contentConfiguration = configuration;

  cell.accessibilityIdentifier = [self titleLabelText];

  cell.isAccessibilityElement = YES;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  cell.accessibilityLabel = GetReadingListCellAccessibilityLabel(
      self.title, [self hostname], self.distillationState,
      self.showCloudSlashIcon);
  cell.accessibilityCustomActions =
      [self.customActionFactory customActionsForItem:self];
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
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

// Returns the text to use when configuring a title.
- (NSString*)titleLabelText {
  return self.title.length ? self.title : self.hostname;
}

// Returns the text to use when configuring a URL.
- (NSString*)URLLabelText {
  // If there's no title text, the URL is used as the cell title.  Simply
  // display the distillation date in the URL label when this occurs.
  if (!self.title.length) {
    return self.distillationDateText;
  }

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
