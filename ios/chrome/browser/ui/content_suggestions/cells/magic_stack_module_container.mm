// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The horizontal inset for the content within this container.
const CGFloat kContentHorizontalInset = 20.0f;

// The top inset for the content within this container.
const CGFloat kContentTopInset = 16.0f;

// The bottom inset for the content within this container.
const CGFloat kContentBottomInset = 24.0f;
const CGFloat kReducedContentBottomInset = 10.0f;

// Vertical spacing between the content views.
const CGFloat kContentVerticalSpacing = 16.0f;

// The corner radius of this container.
const float kCornerRadius = 24;

// The width of the modules.
const int kModuleWidthCompact = 343;
const int kModuleWidthRegular = 382;
// The max height of the modules.
const int kModuleMaxHeight = 150;

const CGFloat kSeparatorHeight = 0.5;

// The horizontal trailing spacing between the top horizontal StackView
// (containing the title and any subtitle/See More buttons) and the module's
// overall vertical container StackView when there is none between the overall
// vertical StackView and this container .
const CGFloat kTitleStackViewTrailingMargin = 16.0f;

}  // namespace

@interface MagicStackModuleContainer () <UIContextMenuInteractionDelegate>

// Redefined as ReadWrite.
@property(nonatomic, assign, readwrite) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleContainer {
  NSLayoutConstraint* _contentViewWidthAnchor;
  id<MagicStackModuleContainerDelegate> _delegate;
  UILabel* _title;
  UILabel* _subtitle;
  BOOL _isPlaceholder;
  UIButton* _seeMoreButton;
}

- (instancetype)initWithType:(ContentSuggestionsModuleType)type {
  self = [super initWithFrame:CGRectZero];
  if (self) {
  }
  return self;
}

- (instancetype)initAsPlaceholder {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _isPlaceholder = YES;
    self.layer.cornerRadius = kCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    UIImageView* placeholderImage = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"magic_stack_placeholder_module"]];
    placeholderImage.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:placeholderImage];
    AddSameConstraints(placeholderImage, self);
  }
  return self;
}

- (instancetype)initWithContentView:(UIView*)contentView
                               type:(ContentSuggestionsModuleType)type
                           delegate:
                               (id<MagicStackModuleContainerDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _type = type;
    _delegate = delegate;
    self.layer.cornerRadius = kCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    if ([self allowsLongPress]) {
      [self addInteraction:[[UIContextMenuInteraction alloc]
                               initWithDelegate:self]];
    }

    UIStackView* titleStackView = [[UIStackView alloc] init];
    titleStackView.alignment = UIStackViewAlignmentCenter;
    titleStackView.axis = UILayoutConstraintAxisHorizontal;
    titleStackView.distribution = UIStackViewDistributionFill;
    // Resist Vertical expansion so all titles are the same height, allowing
    // content view to fill the rest of the module space.
    [titleStackView setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

    _title = [[UILabel alloc] init];
    _title.text = [MagicStackModuleContainer titleStringForModule:type];
    _title.font = [MagicStackModuleContainer fontForTitle];
    _title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _title.numberOfLines = 0;
    _title.lineBreakMode = NSLineBreakByWordWrapping;
    _title.accessibilityTraits |= UIAccessibilityTraitHeader;
    _title.accessibilityIdentifier =
        [MagicStackModuleContainer accessibilityIdentifierForModule:type];
    [_title setContentHuggingPriority:UILayoutPriorityDefaultLow
                              forAxis:UILayoutConstraintAxisHorizontal];
    [_title
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_title
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    [titleStackView addArrangedSubview:_title];
    // `setContentHuggingPriority:` does not guarantee that titleStackView
    // completely resists vertical expansion since UIStackViews do not have
    // intrinsic contentSize. Constraining the title label to the StackView will
    // ensure contentView expands.
    [NSLayoutConstraint activateConstraints:@[
      [_title.bottomAnchor constraintEqualToAnchor:titleStackView.bottomAnchor]
    ]];

    if ([self shouldShowSeeMore]) {
      UIButton* showMoreButton = [[UIButton alloc] init];
      [showMoreButton
          setTitle:l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_SEE_MORE)
          forState:UIControlStateNormal];
      [showMoreButton setTitleColor:[UIColor colorNamed:kBlueColor]
                           forState:UIControlStateNormal];
      [showMoreButton.titleLabel
          setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]];
      showMoreButton.titleLabel.numberOfLines = 2;
      showMoreButton.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
      showMoreButton.titleLabel.adjustsFontForContentSizeCategory = YES;
      showMoreButton.contentHorizontalAlignment =
          UIControlContentHorizontalAlignmentTrailing;
      [showMoreButton
          setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      [showMoreButton addTarget:self
                         action:@selector(seeMoreButtonWasTapped:)
               forControlEvents:UIControlEventTouchUpInside];
      [showMoreButton
          setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
      [titleStackView addArrangedSubview:showMoreButton];
      showMoreButton.accessibilityIdentifier = showMoreButton.titleLabel.text;
      _seeMoreButton = showMoreButton;
    } else if ([self shouldShowSubtitle]) {
      // TODO(crbug.com/1474992): Update MagicStackModuleContainer to take an id
      // config in its initializer so the container can build itself from a
      // passed config/state object.
      NSString* subtitle = [delegate subtitleStringForModule:type];

      _subtitle = [[UILabel alloc] init];
      _subtitle.text = subtitle;
      _subtitle.font = [MagicStackModuleContainer fontForSubtitle];
      _subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
      _subtitle.numberOfLines = 0;
      _subtitle.lineBreakMode = NSLineBreakByWordWrapping;
      _subtitle.accessibilityTraits |= UIAccessibilityTraitHeader;
      _subtitle.accessibilityIdentifier = subtitle;
      [_subtitle setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];
      [_subtitle
          setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      _subtitle.textAlignment =
          UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;

      [titleStackView addArrangedSubview:_subtitle];
    }

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.alignment = UIStackViewAlignmentLeading;
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.spacing = kContentVerticalSpacing;
    stackView.distribution = UIStackViewDistributionFill;
    [stackView addSubview:contentView];
    if ([_title.text length] > 0) {
      [stackView addArrangedSubview:titleStackView];
      // Ensure that there is horizontal trailing spacing between the title
      // stackview content and the module. The overall StackView has no trailing
      // spacing for kCompactedSetUpList.
      CGFloat trailingSpacing =
          _type == ContentSuggestionsModuleType::kCompactedSetUpList
              ? -kTitleStackViewTrailingMargin
              : 0;
      [NSLayoutConstraint activateConstraints:@[
        [titleStackView.trailingAnchor
            constraintEqualToAnchor:stackView.trailingAnchor
                           constant:trailingSpacing],
      ]];
    }
    if ([self shouldShowSeparator]) {
      UIView* separator = [[UIView alloc] init];
      [separator setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                   forAxis:UILayoutConstraintAxisVertical];
      separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
      [stackView addArrangedSubview:separator];
      [NSLayoutConstraint activateConstraints:@[
        [separator.heightAnchor
            constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
        [separator.leadingAnchor
            constraintEqualToAnchor:stackView.leadingAnchor],
        [separator.trailingAnchor
            constraintEqualToAnchor:stackView.trailingAnchor],
      ]];
    }
    [stackView addArrangedSubview:contentView];

    NSMutableArray* accessibilityElements =
        [[NSMutableArray alloc] initWithObjects:_title, nil];
    if ([self shouldShowSeeMore]) {
      [accessibilityElements addObject:_seeMoreButton];
    }
    [accessibilityElements addObject:contentView];
    if ([self shouldShowSubtitle]) {
      [accessibilityElements addObject:_subtitle];
    }
    self.accessibilityElements = accessibilityElements;

    _contentViewWidthAnchor = [contentView.widthAnchor
        constraintEqualToConstant:[self contentViewWidth]];
    [NSLayoutConstraint activateConstraints:@[ _contentViewWidthAnchor ]];
    // Configures `contentView` to be the view willing to expand if needed to
    // fill extra vertical space in the container.
    [contentView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    // Ensures that the modules conforms to a height of kModuleMaxHeight. For
    // the MVT when it lives outside of the Magic Stack to stay as close to its
    // intrinsic size as possible, the constraint is configured to be less than
    // or equal to.
    if (_type == ContentSuggestionsModuleType::kMostVisited &&
        !ShouldPutMostVisitedSitesInMagicStack()) {
      [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintLessThanOrEqualToConstant:kModuleMaxHeight]
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintEqualToConstant:kModuleMaxHeight]
      ]];
    }

    [self addSubview:stackView];
    AddSameConstraintsWithInsets(stackView, self, [self contentMargins]);
  }
  return self;
}

// Returns the module width (CGFloat) given `traitCollection`.
+ (CGFloat)moduleWidthForHorizontalTraitCollection:
    (UITraitCollection*)traitCollection {
  return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular
             ? kModuleWidthRegular
             : kModuleWidthCompact;
}

// Returns the module's title, if any, given the Magic Stack module `type`.
+ (NSString*)titleStringForModule:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kShortcuts:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_SHORTCUTS_MODULE_TITLE);
    case ContentSuggestionsModuleType::kMostVisited:
      if (ShouldPutMostVisitedSitesInMagicStack()) {
        return l10n_util::GetNSString(
            IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE);
      }
      return @"";
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_TITLE);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE);
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE);
    default:
      NOTREACHED();
      return @"";
  }
}

// Returns the accessibility identifier given the Magic Stack module `type`.
+ (NSString*)accessibilityIdentifierForModule:
    (ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier;

    default:
      // TODO(crbug.com/1506038): the code should use constants for
      // accessibility identifiers, and not localized strings.
      return [self titleStringForModule:type];
  }
}

// Returns the font for the module title string.
+ (UIFont*)fontForTitle {
  return CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold);
}

// Returns the font for the module subtitle string.
+ (UIFont*)fontForSubtitle {
  return CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
}

// Returns the content insets.
- (NSDirectionalEdgeInsets)contentMargins {
  NSDirectionalEdgeInsets contentMargins =
      NSDirectionalEdgeInsetsMake(kContentTopInset, kContentHorizontalInset,
                                  kContentBottomInset, kContentHorizontalInset);
  switch (_type) {
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      contentMargins.trailing = 0;
      break;
    case ContentSuggestionsModuleType::kMostVisited:
    case ContentSuggestionsModuleType::kShortcuts:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      contentMargins.bottom = kReducedContentBottomInset;
      break;
    default:
      break;
  }
  return contentMargins;
}

// Returns the intrinsic content size.
- (CGSize)intrinsicContentSize {
  // When the Most Visited Tiles module is not in the Magic Stack or if a module
  // is the only module in the Magic Stack in a wider screen, the module should
  // be wider to match the wider Magic Stack ScrollView.
  BOOL MVTModuleShouldUseWideWidth =
      (!_isPlaceholder && _type == ContentSuggestionsModuleType::kMostVisited &&
       !ShouldPutMostVisitedSitesInMagicStack() &&
       content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                           self.window));
  BOOL moduleShouldUseWideWidth =
      content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                          self.window) &&
      [_delegate doesMagicStackShowOnlyOneModule:_type];
  if (MVTModuleShouldUseWideWidth || moduleShouldUseWideWidth) {
    return CGSizeMake(kMagicStackWideWidth, UIViewNoIntrinsicMetric);
  }
  return CGSizeMake(
      [MagicStackModuleContainer
          moduleWidthForHorizontalTraitCollection:self.traitCollection],
      UIViewNoIntrinsicMetric);
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    _title.font = [MagicStackModuleContainer fontForTitle];
  }
  _contentViewWidthAnchor.constant = [self contentViewWidth];
  // Trigger relayout so intrinsic contentsize is recalculated.
  [self invalidateIntrinsicContentSize];
  [self sizeToFit];
  [self layoutIfNeeded];
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  CHECK([self allowsLongPress]);
  __weak MagicStackModuleContainer* weakSelf = self;
  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    UIAction* hideAction = [UIAction
        actionWithTitle:[self contextMenuHideDescription]
                  image:DefaultSymbolWithPointSize(kHideActionSymbol, 18)
             identifier:nil
                handler:^(UIAction* action) {
                  MagicStackModuleContainer* strongSelf = weakSelf;
                  [strongSelf->_delegate neverShowModuleType:strongSelf->_type];
                }];
    hideAction.attributes = UIMenuElementAttributesDestructive;
    return [UIMenu menuWithTitle:[self contextMenuTitle]
                        children:@[ hideAction ]];
  };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Helpers

- (void)seeMoreButtonWasTapped:(UIButton*)button {
  [_delegate seeMoreWasTappedForModuleType:_type];
}

// YES if this container should show a context menu when the user performs a
// long-press gesture.
- (BOOL)allowsLongPress {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      return YES;
    default:
      return NO;
  }
}

// Based on ContentSuggestionsModuleType, returns YES if the module should show
// a subtitle.
- (BOOL)shouldShowSubtitle {
  switch (_type) {
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
      return YES;
    default:
      return NO;
  }
}

// Based on ContentSuggestionsModuleType, returns YES if a "See More" button
// should be displayed in the module.
- (BOOL)shouldShowSeeMore {
  switch (_type) {
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      return YES;
    default:
      return NO;
  }
}

// Based on ContentSuggestionsModuleType, returns YES if a separator should be
// shown between the module title/subtitle row, and the remaining bottom-half of
// the module.
- (BOOL)shouldShowSeparator {
  switch (_type) {
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kTabResumption:
      return YES;
    default:
      return NO;
  }
}

// Title string for the context menu of this container.
- (NSString*)contextMenuTitle {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_TITLE);
    default:
      NOTREACHED_NORETURN();
  }
}

// Descriptor string for hide action of the context menu of this container.
- (NSString*)contextMenuHideDescription {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      return l10n_util::GetNSStringF(
          IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_DESCRIPTION,
          base::SysNSStringToUTF16(l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)));
    default:
      NOTREACHED_NORETURN();
  }
}

// Returns the expected width of the contentView subview.
- (CGFloat)contentViewWidth {
  NSDirectionalEdgeInsets insets = [self contentMargins];
  return [self intrinsicContentSize].width - insets.leading - insets.trailing;
}

@end
