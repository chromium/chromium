// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_search_engine_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Favicon container size (vertical and horizontal).
constexpr CGFloat kFaviconContainerSize = 32.;
// Favicon container corner radius.
constexpr CGFloat kFaviconContainerCornerRadius = 7.;
// Favicon container border width.
constexpr CGFloat kFaviconContainerBorderWidth = 1.5;

}  // namespace

@implementation SettingsSearchEngineItem {
  raw_ptr<const TemplateURL> _templateURL;
}

@synthesize enabled = _enabled;
@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = SettingsSearchEngineCell.class;
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  SettingsSearchEngineCell* cell =
      base::apple::ObjCCastStrict<SettingsSearchEngineCell>(tableCell);
  cell.accessibilityIdentifier = [NSString
      stringWithFormat:@"%@%@", kSettingsSearchEngineCellIdentifierPrefix,
                       self.text];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  [cell.faviconView configureWithAttributes:self.faviconAttributes];
  if (self.enabled) {
    cell.contentView.alpha = 1.0;
    cell.userInteractionEnabled = YES;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    cell.contentView.alpha = 0.4;
    cell.userInteractionEnabled = NO;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  }
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

- (void)setTemplateURL:(const TemplateURL*)templateURL {
  _templateURL = templateURL;
}

- (const TemplateURL*)templateURL {
  return _templateURL;
}

@end

@implementation SettingsSearchEngineCell {
  UIView* _faviconContainer;
}

@synthesize faviconView = _faviconView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;
    // Add favicon container.
    _faviconContainer = [[UIView alloc] init];
    _faviconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconContainer.layer.borderWidth = kFaviconContainerBorderWidth;
    _faviconContainer.layer.cornerRadius = kFaviconContainerCornerRadius;
    _faviconContainer.layer.masksToBounds = YES;
    _faviconContainer.layer.borderColor =
        [UIColor colorNamed:kSeparatorColor].CGColor;
    [contentView addSubview:_faviconContainer];
    // Add favicon view.
    _faviconView = [[FaviconView alloc] init];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    [_faviconView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
    [_faviconView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_faviconContainer addSubview:_faviconView];
    // Stack.
    UIStackView* textStackView = [[UIStackView alloc] init];
    textStackView.axis = UILayoutConstraintAxisVertical;
    textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:textStackView];
    // Add text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.numberOfLines = 0;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [textStackView addArrangedSubview:_textLabel];
    // Add detail text label.
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    [textStackView addArrangedSubview:_detailTextLabel];
    AddSameCenterConstraints(_faviconContainer, _faviconView);
    AddSameCenterYConstraint(_faviconContainer, contentView);
    AddSameCenterYConstraint(textStackView, contentView);
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.widthAnchor
          constraintEqualToConstant:kFaviconImageViewSize],
      [_faviconView.heightAnchor
          constraintEqualToConstant:kFaviconImageViewSize],
      [_faviconContainer.widthAnchor
          constraintEqualToConstant:kFaviconContainerSize],
      [_faviconContainer.heightAnchor
          constraintEqualToConstant:kFaviconContainerSize],
      [_faviconContainer.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainer.topAnchor
          constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                      constant:kTableViewVerticalSpacing],
      [_faviconContainer.bottomAnchor
          constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                   constant:-kTableViewVerticalSpacing],
      [_faviconContainer.trailingAnchor
          constraintEqualToAnchor:textStackView.leadingAnchor
                         constant:-kTableViewSubViewHorizontalSpacing],
      [textStackView.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kTableViewTwoLabelsCellVerticalSpacing],
      [textStackView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewTwoLabelsCellVerticalSpacing],
      [textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
    ]];
    [self resetColors];

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:TraitCollectionSetForTraits(nil)
                         withAction:@selector(resetColors)];
    }
  }
  return self;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _textLabel.text = nil;
  _detailTextLabel.text = nil;
  [_faviconView configureWithAttributes:nil];
}

#pragma mark - UITraitEnvironment

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self resetColors];
}
#endif

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  CHECK_GT(self.textLabel.text.length, 0ul);
  if (self.detailTextLabel.text.length == 0) {
    return self.textLabel.text;
  }
  return l10n_util::GetNSStringF(
      IDS_IOS_SEARCH_ENGINE_SETTINS_CELL_VOICE_OVER,
      base::SysNSStringToUTF16(self.textLabel.text),
      base::SysNSStringToUTF16(self.detailTextLabel.text));
}

#pragma mark - Private

// Updates the colors in the cell.
- (void)resetColors {
  _faviconContainer.layer.borderColor =
      [UIColor colorNamed:kSeparatorColor].CGColor;
}

@end
