// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

namespace {

constexpr CGFloat kFaviconContainterViewSize = 30;

// Returns the size that the favicon should have.
CGFloat GetFaviconSize() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kFaviconContainterViewSize
                                             : gfx::kFaviconSize;
}

}  // namespace

@interface ManualFillPlusAddressItem ()

// The plus address for this item.
@property(nonatomic, strong, readonly)
    ManualFillPlusAddress* manualFillPlusAddress;

// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The part of the cell's accessibility label that is used to indicate the index
// at which the plus address represented by this item is positioned in the list
// of plus addresses to show.
@property(nonatomic, strong) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillPlusAddressItem

- (instancetype)initWithPlusAddress:(ManualFillPlusAddress*)plusAddress
                    contentInjector:
                        (id<ManualFillContentInjector>)contentInjector
                        menuActions:(NSArray<UIAction*>*)menuActions
        cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _manualFillPlusAddress = plusAddress;
    _contentInjector = contentInjector;
    _menuActions = menuActions;
    _cellIndexAccessibilityLabel = cellIndexAccessibilityLabel;
    self.cellClass = [ManualFillPlusAddressCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPlusAddressCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithPlusAddress:self.manualFillPlusAddress
                  contentInjector:self.contentInjector
                      menuActions:self.menuActions
      cellIndexAccessibilityLabel:_cellIndexAccessibilityLabel];
}

- (const GURL&)faviconURL {
  return self.manualFillPlusAddress.URL;
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.manualFillPlusAddress.URL.spec());
}

- (NSString*)plusAddress {
  return self.manualFillPlusAddress.plusAddress;
}

@end

@interface ManualFillPlusAddressCell ()

// The plus address this cell is showing.
@property(nonatomic, strong) ManualFillPlusAddress* plusAddress;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The constraints for the visible favicon.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* faviconContraints;

// The view displayed at the top the cell containing the favicon, the site name
// and an overflow button.
@property(nonatomic, strong) UIView* headerView;

// The favicon for the plus address. Of type FaviconView when the Keyboard
// Accessory Upgrade is disabled, and FaviconContainerView when enabled.
@property(nonatomic, strong) UIView* faviconView;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// The menu button displayed in the cell's header.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// A button showing the plus address.
@property(nonatomic, strong) UIButton* plusAddressButton;

// Separator line. When the Keyboard Accessory Upgrade feature is enbaled, used
// to delimit the header from the rest of the cell. When disabled, used when
// needed to delimit cells.
@property(nonatomic, strong) UIView* grayLine;

// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

@end

@implementation ManualFillPlusAddressCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";
  [self configureFaviconWithAttributes:nil];

  [self.plusAddressButton setTitle:@"" forState:UIControlStateNormal];
  self.plusAddressButton.enabled = YES;
  [self.plusAddressButton setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
                               forState:UIControlStateNormal];

  self.plusAddress = nil;

  self.grayLine.hidden = NO;
}

- (void)setUpWithPlusAddress:(ManualFillPlusAddress*)plusAddress
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;
  self.plusAddress = plusAddress;

  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;

  // Header.
  NSAttributedString* attributedText =
      CreateSiteNameLabelAttributedText(plusAddress, /*should_show_host=*/YES);
  self.siteNameLabel.attributedText = attributedText;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.siteNameLabel.numberOfLines = 0;
    self.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", cellIndexAccessibilityLabel,
                                   attributedText.string];
  }
  self.siteNameLabel.hidden = NO;
  self.faviconView.hidden = NO;
  AddViewToVerticalLeadViews(self.headerView,
                             ManualFillCellView::ElementType::kOther,
                             verticalLeadViews);
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    AddViewToVerticalLeadViews(
        self.grayLine, ManualFillCellView::ElementType::kHeaderSeparator,
        verticalLeadViews);
  }
  if (menuActions && menuActions.count) {
    self.overflowMenuButton.menu = [UIMenu menuWithChildren:menuActions];
    self.overflowMenuButton.hidden = NO;
  } else {
    self.overflowMenuButton.hidden = YES;
  }

  // Holds the chip buttons related to the plus address that are vertical leads.
  NSMutableArray<UIView*>* plusAddressGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Plus Address chip button.
  [self.plusAddressButton setTitle:plusAddress.plusAddress
                          forState:UIControlStateNormal];
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.plusAddressButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(plusAddress.plusAddress));
  }
  [plusAddressGroupVerticalLeadChips addObject:self.plusAddressButton];

  AddChipGroupsToVerticalLeadViews(@[ plusAddressGroupVerticalLeadChips ],
                                   verticalLeadViews);

  // Set and activate constraints.
  self.dynamicConstraints = [[NSMutableArray alloc] init];
  AppendVerticalConstraintsSpacingForViews(self.dynamicConstraints,
                                           verticalLeadViews, self.layoutGuide,
                                           /*offset=*/0);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.plusAddress.URL.spec());
}

- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes {
  if (attributes.faviconImage) {
    self.faviconView.hidden = NO;
    [NSLayoutConstraint activateConstraints:self.faviconContraints];
    [self configureFaviconWithAttributes:attributes];
    return;
  }
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.layoutGuide =
      AddLayoutGuideToContentView(self.contentView, /*cell_has_header=*/YES);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.faviconView = IsKeyboardAccessoryUpgradeEnabled()
                         ? [[FaviconContainerView alloc] init]
                         : [[FaviconView alloc] init];
  self.faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconView.clipsToBounds = YES;
  self.faviconView.hidden = YES;
  self.faviconContraints = @[
    [self.faviconView.widthAnchor constraintEqualToConstant:GetFaviconSize()],
    [self.faviconView.heightAnchor
        constraintEqualToAnchor:self.faviconView.widthAnchor],
  ];

  self.siteNameLabel = CreateLabel();
  self.overflowMenuButton = CreateOverflowMenuButton();
  // In the tests, the overflow menu of the chips for the other data types, can
  // have the same accessibility identifier, therefore, override for the plus
  // address ones to distinguish them.
  self.overflowMenuButton.accessibilityIdentifier =
      manual_fill::kExpandedManualFillPlusAddressOverflowMenuID;

  self.headerView = CreateHeaderView(self.faviconView, self.siteNameLabel,
                                     self.overflowMenuButton);
  [self.contentView addSubview:self.headerView];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.headerView ],
                                      self.layoutGuide);

  self.grayLine = CreateGraySeparatorForContainer(self.contentView);

  self.plusAddressButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapPlusAddressButton:), self);
  [self.contentView addSubview:self.plusAddressButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.plusAddressButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapPlusAddressButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_PlusAddress_SelectPlusAddress"));
  [self.contentInjector userDidPickContent:self.plusAddress.plusAddress
                             passwordField:NO
                             requiresHTTPS:NO];
}

// Configure the favicon with the given `attributes`.
- (void)configureFaviconWithAttributes:(FaviconAttributes*)attributes {
  FaviconView* favicon;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    FaviconContainerView* faviconContainerView =
        static_cast<FaviconContainerView*>(self.faviconView);
    favicon = faviconContainerView.faviconView;
  } else {
    favicon = static_cast<FaviconView*>(self.faviconView);
  }
  [favicon configureWithAttributes:attributes];
}

@end
