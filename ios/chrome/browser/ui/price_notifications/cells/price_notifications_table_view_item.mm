// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_image_container_view.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_menu_button.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_cell_delegate.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

const CGFloat kCellContentHeight = 64.0;
const CGFloat kCellContentSpacing = 14;
const CGFloat kTableViewColumnSpacing = 8;
// Notification icon's point size.
const CGFloat kNotificationIconPointSize = 20;
// The space in between elements in the vertical UIStackView element.
const CGFloat kVerticalStackViewElementSpacing = 7;
// The following properties define the dimensions of the various placeholder
// elements that compose the loading screen.
const CGFloat kTitlePlaceholderHeight = 16;
const CGFloat kTitlePlaceholderWidth = 120;
const CGFloat kURLPlaceholderWidth = 94;
const CGFloat kPriceChipPlaceholderHeight = 24;
const CGFloat kPriceChipPlaceholderWidth = 50;
const CGFloat kTrackButtonPlaceholderHeight = 28;
const CGFloat kTrackButtonPlaceholderWidth = 70;
// Identifier for the stop price tracking action item.
NSString* kActionMenuIdentifier = @"priceTrackingActionMenu";

// A container for the UIView elements that will be added to the UIStackView.
struct TableViewItemStackContent {
  UIView* title;
  UIView* url;
  UIView* price_chip;
  UIView* track_button;
  UIView* menu_button;
};

// Creates an action menu for stopping a product's subscription to price
// tracking events.
UIMenu* CreateOptionMenu(void (^completion_handler)(UIAction* action)) {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kNotificationIconPointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];

  UIImage* icon = DefaultSymbolWithConfiguration(kBellSymbol, configuration);

  UIAction* stop_tracking = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_MENU_ITEM_STOP_TRACKING)
                image:icon
           identifier:kActionMenuIdentifier
              handler:completion_handler];

  // Create Action Menu
  NSArray<UIMenuElement*>* menu_elements = @[ stop_tracking ];

  return [UIMenu menuWithChildren:menu_elements];
}

// Creates an empty rectangular UIView that represents the
// placeholder for an element while the data loads.
UIView* CreatePlaceholderElement() {
  UIView* placeholder = [[UIView alloc] initWithFrame:CGRectZero];
  placeholder.backgroundColor = [UIColor colorNamed:kGrey100Color];
  placeholder.translatesAutoresizingMaskIntoConstraints = NO;
  return placeholder;
}

// A function that creates the horizontal UIStackView that contains and formats
// the bulk of the item's UI elements.
UIStackView* CreateHorizontalStack(TableViewItemStackContent content) {
  // Use stack views to layout the subviews except for the Price Notification
  // Image.
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    content.title, content.url, content.price_chip
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionEqualSpacing;
  verticalStack.alignment = UIStackViewAlignmentLeading;
  verticalStack.spacing = kVerticalStackViewElementSpacing;

  UIStackView* horizontalStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        verticalStack, content.menu_button, content.track_button
      ]];
  horizontalStack.axis = UILayoutConstraintAxisHorizontal;
  horizontalStack.spacing = kTableViewColumnSpacing;
  horizontalStack.distribution = UIStackViewDistributionFill;
  horizontalStack.alignment = UIStackViewAlignmentCenter;
  horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;

  return horizontalStack;
}

// Creates the entire loading screen composed of multiple placeholder
// UIViews.
UIStackView* CreateLoadingScreen(UIView* track_button, UIView* menu_button) {
  TableViewItemStackContent content = {
      CreatePlaceholderElement(), CreatePlaceholderElement(),
      CreatePlaceholderElement(), track_button, menu_button};
  content.title.layer.cornerRadius = 2;
  content.url.layer.cornerRadius = 2;
  content.price_chip.layer.cornerRadius = kPriceChipPlaceholderHeight / 2;
  content.menu_button.layer.cornerRadius = kPriceChipPlaceholderHeight / 2;
  content.track_button.layer.cornerRadius = kTrackButtonPlaceholderHeight / 2;

  UIStackView* horizontalStack = CreateHorizontalStack(content);

  // Set the heights and widths of the placeholders
  [NSLayoutConstraint activateConstraints:@[
    [content.title.heightAnchor
        constraintEqualToConstant:kTitlePlaceholderHeight],
    [content.title.widthAnchor
        constraintEqualToConstant:kTitlePlaceholderWidth],
    [content.url.heightAnchor
        constraintEqualToConstant:kTitlePlaceholderHeight],
    [content.url.widthAnchor constraintEqualToConstant:kURLPlaceholderWidth],
    [content.price_chip.heightAnchor
        constraintEqualToConstant:kPriceChipPlaceholderHeight],
    [content.price_chip.widthAnchor
        constraintEqualToConstant:kPriceChipPlaceholderWidth],
    [content.track_button.heightAnchor
        constraintEqualToConstant:kTrackButtonPlaceholderHeight],
    [content.track_button.widthAnchor
        constraintEqualToConstant:kTrackButtonPlaceholderWidth],
    [content.menu_button.heightAnchor
        constraintEqualToConstant:kPriceChipPlaceholderHeight],
    [content.menu_button.widthAnchor
        constraintEqualToAnchor:content.menu_button.heightAnchor]
  ]];

  return horizontalStack;
}

}  // namespace

@implementation PriceNotificationsTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PriceNotificationsTableViewCell class];
  }
  return self;
}

- (void)configureCell:(PriceNotificationsTableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  tableCell.titleLabel.text = self.title;
  tableCell.entryURL = self.entryURL;
  [tableCell setImage:self.productImage];
  [tableCell.priceNotificationsChip setPriceDrop:self.currentPrice
                                   previousPrice:self.previousPrice];
  tableCell.tracking = self.tracking;
  tableCell.accessibilityTraits |= UIAccessibilityTraitButton;
  tableCell.delegate = self.delegate;
  tableCell.loading = self.loading;
}

@end

#pragma mark - PriceNotificationsTableViewCell

@interface PriceNotificationsTableViewCell ()

// The imageview that is displayed on the leading edge of the cell.
@property(nonatomic, strong)
    PriceNotificationsImageContainerView* priceNotificationsImageContainerView;

@end

@implementation PriceNotificationsTableViewCell {
  // A blank rectangle that is displays as a placeholder for the title when the
  // data is loading.
  // UIView* _titlePlaceholder;
  UIStackView* _placeholderStackView;
  // This the UIStackView that contains the data that is returned from the
  // ShoppingService.
  UIStackView* _horizontalStack;
  // These two properties are placeholder elements. They need to be defined as
  // ivars because their visibility needs to be toggled depending on whether the
  // PriceNotificationTableViewCell's `tracking` property is true or false.
  UIView* _menuButtonPlaceholder;
  UIView* _trackButtonPlaceholder;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel = [[UILabel alloc] init];
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _trackButton = [[PriceNotificationsTrackButton alloc] init];
    _menuButton = [[PriceNotificationsMenuButton alloc] init];
    __weak PriceNotificationsTableViewCell* weakSelf = self;
    _menuButton.menu = CreateOptionMenu(^(UIAction* action) {
      [weakSelf willStopTrackingItem];
    });
    _menuButton.showsMenuAsPrimaryAction = YES;
    _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
    _priceNotificationsChip.translatesAutoresizingMaskIntoConstraints = NO;
    _priceNotificationsChip.isAccessibilityElement = YES;
    _priceNotificationsImageContainerView =
        [[PriceNotificationsImageContainerView alloc] init];
    _priceNotificationsImageContainerView
        .translatesAutoresizingMaskIntoConstraints = NO;
    [_trackButton addTarget:self
                     action:@selector(trackItem)
           forControlEvents:UIControlEventTouchUpInside];

    TableViewItemStackContent content = {_titleLabel, _URLLabel,
                                         _priceNotificationsChip, _trackButton,
                                         _menuButton};
    _horizontalStack = CreateHorizontalStack(content);

    _menuButtonPlaceholder = CreatePlaceholderElement();
    _trackButtonPlaceholder = CreatePlaceholderElement();
    _placeholderStackView =
        CreateLoadingScreen(_trackButtonPlaceholder, _menuButtonPlaceholder);

    [self.contentView addSubview:_priceNotificationsImageContainerView];
    [self.contentView addSubview:_horizontalStack];
    [self.contentView addSubview:_placeholderStackView];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellContentHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      [self.priceNotificationsImageContainerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [self.priceNotificationsImageContainerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [_horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.priceNotificationsImageContainerView
                                      .trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kCellContentSpacing],
      [_horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_horizontalStack.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.bottomAnchor
                                      constant:-kCellContentSpacing],
      heightConstraint,
    ]];

    AddSameConstraints(_horizontalStack, _placeholderStackView);
  }
  return self;
}

- (void)setImage:(UIImage*)productImage {
  [self.priceNotificationsImageContainerView setImage:productImage];
}

- (void)setTracking:(BOOL)tracking {
  if (tracking) {
    self.trackButton.hidden = YES;
    _trackButtonPlaceholder.hidden = YES;
    self.menuButton.hidden = NO;
    _menuButtonPlaceholder.hidden = NO;
    return;
  }

  self.trackButton.hidden = NO;
  _trackButtonPlaceholder.hidden = NO;
  self.menuButton.hidden = YES;
  _menuButtonPlaceholder.hidden = YES;
  _tracking = tracking;
}

- (void)setEntryURL:(GURL)URL {
  if (URL != _entryURL) {
    _entryURL = URL;
    _URLLabel.text = base::SysUTF16ToNSString(
        url_formatter::
            FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                _entryURL));
  }
}

- (void)setLoading:(BOOL)isLoading {
  _loading = isLoading;

  if (_loading) {
    _horizontalStack.hidden = YES;
    _placeholderStackView.hidden = NO;
    return;
  }

  _horizontalStack.hidden = NO;
  _placeholderStackView.hidden = YES;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
  self.loading = NO;
  [self.trackButton setUserInteractionEnabled:YES];
}

#pragma mark - Private

// Initiates the user's subscription to the product's price tracking events.
- (void)trackItem {
  [self.trackButton setUserInteractionEnabled:NO];
  [self.delegate trackItemForCell:self];
}

// Stops the user's subscription to the product's price tracking events.
- (void)willStopTrackingItem {
  [self.delegate stopTrackingItemForCell:self];
}

@end
