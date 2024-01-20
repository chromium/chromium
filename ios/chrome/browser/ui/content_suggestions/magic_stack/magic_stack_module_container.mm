// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_contents_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/most_visited_tiles_config.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

// The max height of the modules.
const int kModuleMaxHeight = 150;

const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@interface MagicStackModuleContainer () <UIContextMenuInteractionDelegate>

// Redefined as ReadWrite.
@property(nonatomic, assign, readwrite) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleContainer {
  UILabel* _title;
  UILabel* _subtitle;
  BOOL _isPlaceholder;
  UIButton* _seeMoreButton;
  UIView* _contentView;
  UIView* _separator;
  UIStackView* _stackView;
  UIImageView* _placeholderImage;
  UIStackView* _titleStackView;
    MagicStackModuleContentsFactory* _magicStackModuleContentsFactory;
    NSLayoutConstraint* _containerHeightAnchor;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _magicStackModuleContentsFactory = [[MagicStackModuleContentsFactory alloc] init];

    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;

    _titleStackView = [[UIStackView alloc] init];
    _titleStackView.alignment = UIStackViewAlignmentTop;
    _titleStackView.axis = UILayoutConstraintAxisHorizontal;
    _titleStackView.distribution = UIStackViewDistributionFill;
    // Resist Vertical expansion so all titles are the same height, allowing
    // content view to fill the rest of the module space.
    [_titleStackView setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                       forAxis:UILayoutConstraintAxisVertical];

    _title = [[UILabel alloc] init];
    _title.font = [MagicStackModuleContainer fontForTitle];
    _title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _title.numberOfLines = 1;
    _title.lineBreakMode = NSLineBreakByWordWrapping;
    _title.accessibilityTraits |= UIAccessibilityTraitHeader;
    [_title setContentHuggingPriority:UILayoutPriorityDefaultLow
                              forAxis:UILayoutConstraintAxisHorizontal];
    [_title
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_title
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    [_titleStackView addArrangedSubview:_title];
    // `setContentHuggingPriority:` does not guarantee that _titleStackView
    // completely resists vertical expansion since UIStackViews do not have
    // intrinsic contentSize. Constraining the title label to the StackView will
    // ensure contentView expands.
    [NSLayoutConstraint activateConstraints:@[
      [_title.topAnchor constraintEqualToAnchor:_titleStackView.topAnchor]
    ]];

    _seeMoreButton = [[UIButton alloc] init];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
    buttonConfiguration.titleLineBreakMode = NSLineBreakByWordWrapping;
    NSDictionary* attributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
    };
    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_SEE_MORE)
            attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;
    _seeMoreButton.configuration = buttonConfiguration;
    [_seeMoreButton setTitleColor:[UIColor colorNamed:kBlueColor]
                         forState:UIControlStateNormal];
    _seeMoreButton.titleLabel.numberOfLines = 2;
    _seeMoreButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _seeMoreButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentTrailing;
    [_seeMoreButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_seeMoreButton addTarget:self
                       action:@selector(seeMoreButtonWasTapped:)
             forControlEvents:UIControlEventTouchUpInside];
    [_seeMoreButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_titleStackView addArrangedSubview:_seeMoreButton];
    _seeMoreButton.accessibilityIdentifier = _seeMoreButton.titleLabel.text;
    _seeMoreButton.hidden = YES;

    _subtitle = [[UILabel alloc] init];
    _subtitle.font = [MagicStackModuleContainer fontForSubtitle];
    _subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitle.numberOfLines = 0;
    _subtitle.lineBreakMode = NSLineBreakByWordWrapping;
    _subtitle.accessibilityTraits |= UIAccessibilityTraitHeader;
    [_subtitle setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
    [_subtitle
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _subtitle.textAlignment =
        UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;
    [_titleStackView addArrangedSubview:_subtitle];

    _stackView = [[UIStackView alloc] init];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.alignment = UIStackViewAlignmentFill;
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.spacing = kContentVerticalSpacing;
    _stackView.distribution = UIStackViewDistributionFill;

    [_stackView addArrangedSubview:_titleStackView];

    _separator = [[UIView alloc] init];
    [_separator setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisVertical];
    _separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    [_stackView addArrangedSubview:_separator];
    [NSLayoutConstraint activateConstraints:@[
      [_separator.heightAnchor
          constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
      [_separator.leadingAnchor
          constraintEqualToAnchor:_stackView.leadingAnchor],
      [_separator.trailingAnchor
          constraintEqualToAnchor:_stackView.trailingAnchor],
    ]];

    _containerHeightAnchor =
        [self.heightAnchor constraintEqualToConstant:kModuleMaxHeight];
    [NSLayoutConstraint activateConstraints:@[ _containerHeightAnchor ]];

    [self addSubview:_stackView];
    AddSameConstraintsWithInsets(_stackView, self, [self contentMargins]);
  }
  return self;
}

- (instancetype)initWithContentView:(UIView*)contentView
                               type:(ContentSuggestionsModuleType)type
                           delegate:
                               (id<MagicStackModuleContainerDelegate>)delegate {
  if (self = [self initWithFrame:CGRectZero]) {
    _type = type;
    _delegate = delegate;
    if ([self allowsLongPress]) {
      [self addInteraction:[[UIContextMenuInteraction alloc]
                               initWithDelegate:self]];
    }

    _title.text = [MagicStackModuleContainer titleStringForModule:_type];
    _title.accessibilityIdentifier =
        [MagicStackModuleContainer accessibilityIdentifierForModule:_type];

    _seeMoreButton.hidden = ![self shouldShowSeeMore];

    if ([self shouldShowSubtitle]) {
      // TODO(crbug.com/1474992): Update MagicStackModuleContainer to take an id
      // config in its initializer so the container can build itself from a
      // passed config/state object.
      NSString* subtitle = [_delegate subtitleStringForModule:_type];
      _subtitle.text = subtitle;
      _subtitle.accessibilityIdentifier = subtitle;
    }

    _title.hidden = [_title.text length] == 0;

    _separator.hidden = ![self shouldShowSeparator];

    _contentView = contentView;
    [_stackView addArrangedSubview:_contentView];

    // Configures `contentView` to be the view willing to expand if needed to
    // fill extra vertical space in the container.
    [_contentView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];

    NSMutableArray* accessibilityElements =
        [[NSMutableArray alloc] initWithObjects:_title, nil];
    if ([self shouldShowSeeMore]) {
      [accessibilityElements addObject:_seeMoreButton];
    } else if ([self shouldShowSubtitle]) {
      [accessibilityElements addObject:_subtitle];
    }
    [accessibilityElements addObject:_contentView];
    self.accessibilityElements = accessibilityElements;
  }
  return self;
}

- (void)configureWithConfig:(MagicStackModule*)config {
  // Ensures that the modules conforms to a height of kModuleMaxHeight. For
  // the MVT when it lives outside of the Magic Stack to stay as close to its
  // intrinsic size as possible, the constraint is configured to be less than
  // or equal to.
  if (config.type == ContentSuggestionsModuleType::kMostVisited &&
      !ShouldPutMostVisitedSitesInMagicStack()) {
    _containerHeightAnchor.active = NO;
    _containerHeightAnchor = [self.heightAnchor
        constraintLessThanOrEqualToConstant:kModuleMaxHeight];
    [NSLayoutConstraint activateConstraints:@[ _containerHeightAnchor ]];
  }

  if (config.type == ContentSuggestionsModuleType::kPlaceholder) {
    _isPlaceholder = YES;
    _placeholderImage = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"magic_stack_placeholder_module"]];
    _placeholderImage.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_placeholderImage];
    AddSameConstraints(_placeholderImage, self);
    [self bringSubviewToFront:_placeholderImage];
    _separator.hidden = YES;
    return;
  }
  _type = config.type;
  if ([self allowsLongPress]) {
    [self addInteraction:[[UIContextMenuInteraction alloc]
                             initWithDelegate:self]];
  }

  _title.text = [MagicStackModuleContainer titleStringForModule:_type];
  _title.accessibilityIdentifier =
      [MagicStackModuleContainer accessibilityIdentifierForModule:_type];

  _seeMoreButton.hidden = ![self shouldShowSeeMore];

  if ([self shouldShowSubtitle]) {
    // TODO(crbug.com/1474992): Update MagicStackModuleContainer to take an id
    // config in its initializer so the container can build itself from a
    // passed config/state object.
    NSString* subtitle = [_delegate subtitleStringForModule:_type];
    _subtitle.text = subtitle;
    _subtitle.accessibilityIdentifier = subtitle;
  }

  if ([_title.text length] == 0) {
    [_titleStackView removeFromSuperview];
  }

  _separator.hidden = ![self shouldShowSeparator];

  _contentView = [_magicStackModuleContentsFactory contentViewForConfig:config traitCollection:self.traitCollection];
  [_stackView addArrangedSubview:_contentView];

  // Configures `contentView` to be the view willing to expand if needed to
  // fill extra vertical space in the container.
  [_contentView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  NSMutableArray* accessibilityElements =
      [[NSMutableArray alloc] initWithObjects:_title, nil];
  if ([self shouldShowSeeMore]) {
    [accessibilityElements addObject:_seeMoreButton];
  } else if ([self shouldShowSubtitle]) {
    [accessibilityElements addObject:_subtitle];
  }
  [accessibilityElements addObject:_contentView];
  self.accessibilityElements = accessibilityElements;
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
    case ContentSuggestionsModuleType::kSetUpListContentNotification:
      return content_suggestions::SetUpListTitleString();
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

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    _title.font = [MagicStackModuleContainer fontForTitle];
  }
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
    case ContentSuggestionsModuleType::kSetUpListContentNotification:
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
    case ContentSuggestionsModuleType::kSetUpListContentNotification:
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
    case ContentSuggestionsModuleType::kSetUpListContentNotification:
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
    case ContentSuggestionsModuleType::kSetUpListContentNotification:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      return l10n_util::GetNSStringF(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_DESCRIPTION,
          l10n_util::GetStringUTF16(
              content_suggestions::SetUpListTitleStringID()));
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

@end
