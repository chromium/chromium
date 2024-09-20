// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mutator.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/snippet_search_engine_button.h"
#import "ios/chrome/browser/ui/search_engine_choice/snippet_search_engine_element.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Space between the Chrome logo and the top of the screen.
constexpr CGFloat kLogoTopMargin = 24.;
// Logo dimensions.
constexpr CGFloat kLogoSize = 40.;
// Margin between the logo and the title.
constexpr CGFloat kLogoTitleMargin = 16.;
// Margin between the title and the subtitle.
constexpr CGFloat kTitleSubtitleMargin = 8.;
// Margin between the subtitle and search engine stack view.
constexpr CGFloat kSubtitleSearchEngineStackMargin = 20.;
// Margin above "Set as Default" button.
// This margin needs to be used for inline and floating buttons, to make sure
// both containers have the same size. Having the same size is required to have
// a smooth transition from inline to floating SetAsDefault button.
constexpr CGFloat kSetAsDefaultButtonTopMargin = 16.;
// Corner radius for the "More" pill button.
constexpr CGFloat kMorePillButtonCornerRadius = 25.;
// Horizontal padding for the "More" pill button.
constexpr CGFloat kMorePillButtonHorizontalPadding = 15.;
// Vertical padding for the "More" pill button.
constexpr CGFloat kMorePillButtonVerticalPadding = 17.;
// The margin between the text and the arrow on the "More" pill button.
constexpr CGFloat kMoreArrowMargin = 4.;
// Animation duration when the floating SetAsDefault button appears.
constexpr NSTimeInterval kFloatingSetAsDefaultAnimationDuration = .3;
// Height of the separator shown in the floating container.
constexpr CGFloat kFloatingContainerSeparatorHeight = 1.;
// Animation duration for the more pill button to move away from the bottom of
// the screen.
constexpr CGFloat kMorePillButtonAnimationDuration = .1;
// URL for the "Learn more" link.
const char* const kLearnMoreURL = "internal://choice-screen-learn-more";

SnippetSearchEngineButton* CreateSnippetSearchEngineButtonWithElement(
    SnippetSearchEngineElement* element) {
  CHECK(element.keyword);
  SnippetSearchEngineButton* button = [[SnippetSearchEngineButton alloc] init];
  button.faviconImage = element.faviconImage;
  button.searchEngineName = element.name;
  button.snippetText = element.snippetDescription;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.searchEngineKeyword = element.keyword;
  return button;
}

// Set the tile for a pill button, with its down arrow.
void SetPillButtonTitle(UIButton* pill_button, int string_id) {
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  NSDictionary* textAttributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(string_id)
              attributes:textAttributes];
  // Use `ceilf()` when calculating the icon's bounds to ensure the
  // button's content height does not shrink by fractional points, as the
  // attributed string's actual height is slightly smaller than the
  // assigned height.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [[UIImage imageNamed:@"read_more_arrow"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  CGFloat height = ceilf(attributedString.size.height);
  CGFloat capHeight = ceilf(font.capHeight);
  CGFloat horizontalOffset =
      base::i18n::IsRTL() ? -1.f * kMoreArrowMargin : kMoreArrowMargin;
  CGFloat verticalOffset = (capHeight - height) / 2.f;
  attachment.bounds =
      CGRectMake(horizontalOffset, verticalOffset, height, height);
  [attributedString
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];

  UIButtonConfiguration* buttonConfiguration = pill_button.configuration;
  buttonConfiguration.attributedTitle = attributedString;
  pill_button.configuration = buttonConfiguration;
}

// Configures a "Set as Default" button to be enabled or disabled.
void EnableSetAsDefaultButton(UIButton* button, BOOL is_enabled) {
  UIButtonConfiguration* button_configuration = button.configuration;
  if (is_enabled) {
    button_configuration.background.backgroundColor =
        [UIColor colorNamed:kBlueColor];
    button_configuration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
    button.accessibilityHint = nil;
  } else {
    button_configuration.background.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    button_configuration.baseForegroundColor =
        [UIColor colorNamed:kDisabledTintColor];
    button.accessibilityHint =
        l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_DEFAULT_HINT);
  }
  button.configuration = button_configuration;
  button.enabled = is_enabled;
}

// Creates a "Set as Default" button. The button is returned as disabled.
UIButton* CreateSetAsDefaultButton() {
  UIButton* button = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  SetConfigurationTitle(
      button, l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE));
  button.translatesAutoresizingMaskIntoConstraints = NO;
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the SetAsDefault button, using VoiceOver.
  button.accessibilityContainerType = UIAccessibilityContainerTypeSemanticGroup;
  EnableSetAsDefaultButton(button, /*is_enabled=*/NO);
  return button;
}

// Create a more pill button.
UIButton* CreateMorePillButton() {
  UIButton* morePillButton =
      PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  morePillButton.layer.cornerRadius = kMorePillButtonCornerRadius;
  morePillButton.layer.masksToBounds = YES;
  UIButtonConfiguration* configuration = morePillButton.configuration;
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kMorePillButtonHorizontalPadding, kMorePillButtonVerticalPadding,
      kMorePillButtonHorizontalPadding, kMorePillButtonVerticalPadding);
  morePillButton.configuration = configuration;
  SetPillButtonTitle(morePillButton, IDS_SEARCH_ENGINE_CHOICE_MORE_BUTTON);
  morePillButton.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  return morePillButton;
}

// Returns the `y` value from the `localReference` in the coordinator of
// `mainView`.
CGFloat ConvertVerticalCoordonateWithMainViewReference(UIView* mainView,
                                                       UIView* referenceView,
                                                       CGFloat y) {
  CGPoint point = CGPointMake(0, y);
  CGPoint pointWithMainViewReference = [mainView convertPoint:point
                                                     fromView:referenceView];
  return pointWithMainViewReference.y;
}

}  // namespace

@interface SearchEngineChoiceViewController () <UITextViewDelegate>
@end

@implementation SearchEngineChoiceViewController {
  // The view title.
  UILabel* _titleLabel;
  // Scroll view that contains the logo, the title, the subtitle,
  // the search engine list, and the inline SetAsDefault button.
  UIScrollView* _scrollView;
  // Contains the list of search engine buttons.
  UIStackView* _searchEngineStackView;
  // Button floating on top of the scroll view to scroll down to the bottom.
  // If the user already scroll onces to the button, the button will be hidden.
  // By default the title is "More". As soon as the user selects a search engine
  // the title is changed to "Continue" (the button action is the same).
  UIButton* _moreOrContinueButton;
  // Container to display the "Set as Default" button in the scroll view.
  // Related to `_inlineSetAsDefaultButton`. This container is used in
  // the animation to transition to `_floatingSetAsDefaultButtonContainer`.
  // This container needs to have the same size than
  // `_floatingSetAsDefaultButtonContainer`, to have a smooth transition to the
  // floating SetAsDefault button.
  UIView* _inlineSetAsDefaultButtonContainer;
  // Button to confirm the default search engine selection. This button is
  // visually identical to `_floatingSetAsDefaultButton` but it is part of
  // `_inlineSetAsDefaultButtonContainer`.
  UIButton* _inlineSetAsDefaultButton;
  // Container to display the "Set as Default" button on top of the scroll view.
  // Related to `_floatingSetAsDefaultButton`.
  // This container needs to have the same size than
  // `_inlineSetAsDefaultButtonContainer`, to have a smooth transition from the
  // inline SetAsDefault button.
  UIView* _floatingSetAsDefaultButtonContainer;
  // Horizontal separator at the top of `_floatingSetAsDefaultButtonContainer`.
  // It should be visible only when `_floatingSetAsDefaultButtonContainer` is
  // covering `_searchEngineStackView`.
  UIView* _floatingContainerSeparator;
  // Button to confirm the default search engine selection. This button is
  // visually identical to `_inlineSetAsDefaultButton` but it is inside
  // `_floatingSetAsDefaultButtonContainer`.
  UIButton* _floatingSetAsDefaultButton;
  // Whether the choice screen is being displayed for the FRE.
  BOOL _isForFRE;
  // YES, when showing the floating button and hidding the inline button.
  // NO, when showing the inline button and hidding the floating button.
  BOOL _showFloatingSetAsDefaultButton;
  // Contains the selected search engine button.
  SnippetSearchEngineButton* _selectedSearchEngineButton;
  // Whether `-[SearchEngineChoiceViewController viewIsAppearing:]` was called.
  BOOL _viewIsAppearingCalled;
  // Whether the search engine buttons have been loaded in the stack view.
  BOOL _searchEnginesLoaded;
}

@synthesize searchEngines = _searchEngines;

- (instancetype)initWithFirstRunMode:(BOOL)isForFRE {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _isForFRE = isForFRE;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Add main scroll view with its content view.
  UIView* scrollContentView = [[UIView alloc] init];
  scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView = [[UIScrollView alloc] init];
  [view addSubview:_scrollView];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.accessibilityIdentifier = kSearchEngineChoiceScrollViewIdentifier;
  _scrollView.delegate = self;
  _scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  [_scrollView addSubview:scrollContentView];

  // Add logo image.
  // Need to use a regular png instead of custom symbol to have a better control
  // on the size and the margin of the logo.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logoImage = [UIImage imageNamed:kChromeSearchEngineChoiceIcon];
#else
  UIImage* logoImage = [UIImage imageNamed:kChromiumSearchEngineChoiceIcon];
#endif
  UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logoImage];
  [scrollContentView addSubview:logoImageView];
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add view title.
  _titleLabel = [[UILabel alloc] init];
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the SetAsDefault button, using VoiceOver.
  _titleLabel.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  [scrollContentView addSubview:_titleLabel];
  [_titleLabel
      setText:l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE)];
  [_titleLabel setTextColor:[UIColor colorNamed:kSolidBlackColor]];
  UIFontTextStyle textStyle = GetTitleLabelFontTextStyle(self);
  _titleLabel.font = GetFRETitleFont(textStyle);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  [_titleLabel setTextAlignment:NSTextAlignmentCenter];
  [_titleLabel setNumberOfLines:0];
  [_titleLabel setAccessibilityIdentifier:
                   kSearchEngineChoiceTitleAccessibilityIdentifier];
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  // Add view subtitle.
  NSMutableAttributedString* subtitleText = [[NSMutableAttributedString alloc]
      initWithString:[l10n_util::GetNSString(
                         IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE)
                         stringByAppendingString:@" "]
          attributes:@{
            NSForegroundColorAttributeName : [UIColor colorNamed:kGrey800Color]
          }];
  NSAttributedString* learnMoreAttributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK)
              attributes:@{
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kBlueColor],
                NSLinkAttributeName : net::NSURLWithGURL(GURL(kLearnMoreURL)),
              }];
  learnMoreAttributedString.accessibilityLabel = l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK_A11Y_LABEL);
  [subtitleText appendAttributedString:learnMoreAttributedString];
  UITextView* subtitleTextView = [[UITextView alloc] init];
  [scrollContentView addSubview:subtitleTextView];
  [subtitleTextView setAttributedText:subtitleText];
  [subtitleTextView
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
  subtitleTextView.backgroundColor = nil;
  subtitleTextView.adjustsFontForContentSizeCategory = YES;
  [subtitleTextView setTextAlignment:NSTextAlignmentCenter];
  subtitleTextView.delegate = self;
  // Disable and hide scrollbar.
  subtitleTextView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
  subtitleTextView.scrollEnabled = NO;
  subtitleTextView.showsVerticalScrollIndicator = NO;
  subtitleTextView.showsHorizontalScrollIndicator = NO;
  subtitleTextView.editable = NO;
  subtitleTextView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add stack view for the search engine buttons.
  _searchEngineStackView = [[UIStackView alloc] init];
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the SetAsDefault button, using VoiceOver.
  _searchEngineStackView.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  _searchEngineStackView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  _searchEngineStackView.layer.cornerRadius = 12.;
  _searchEngineStackView.layer.masksToBounds = YES;
  _searchEngineStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _searchEngineStackView.axis = UILayoutConstraintAxisVertical;
  [scrollContentView addSubview:_searchEngineStackView];

  // Add inline "Set as Default" button container.
  _inlineSetAsDefaultButtonContainer = [[UIView alloc] init];
  _inlineSetAsDefaultButtonContainer.translatesAutoresizingMaskIntoConstraints =
      NO;
  [scrollContentView addSubview:_inlineSetAsDefaultButtonContainer];

  // Add inline "Set as Default" button.
  _inlineSetAsDefaultButton = CreateSetAsDefaultButton();
  [_inlineSetAsDefaultButtonContainer addSubview:_inlineSetAsDefaultButton];
  [_inlineSetAsDefaultButton addTarget:self
                                action:@selector(setAsDefaultButtonAction)
                      forControlEvents:UIControlEventTouchUpInside];

  // Add floating "Set as Default" button container.
  _floatingSetAsDefaultButtonContainer = [[UIView alloc] init];
  _floatingSetAsDefaultButtonContainer
      .translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:_floatingSetAsDefaultButtonContainer];
  _floatingSetAsDefaultButtonContainer.hidden = YES;
  _floatingSetAsDefaultButtonContainer.backgroundColor = view.backgroundColor;

  // Add separator at the top of the floating container.
  _floatingContainerSeparator = [[UIView alloc] init];
  _floatingContainerSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  [_floatingSetAsDefaultButtonContainer addSubview:_floatingContainerSeparator];
  _floatingContainerSeparator.backgroundColor =
      [UIColor colorNamed:kSeparatorColor];

  // Add floating "Set as Default" button.
  _floatingSetAsDefaultButton = CreateSetAsDefaultButton();
  _floatingSetAsDefaultButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_floatingSetAsDefaultButtonContainer addSubview:_floatingSetAsDefaultButton];
  _floatingSetAsDefaultButton.accessibilityIdentifier =
      kSetAsDefaultSearchEngineIdentifier;
  _floatingSetAsDefaultButtonContainer.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  [_floatingSetAsDefaultButton addTarget:self
                                  action:@selector(setAsDefaultButtonAction)
                        forControlEvents:UIControlEventTouchUpInside];

  // Add "More" pill button.
  // Needs to be the last element added to the view, so it is always above all
  // other elements.
  _moreOrContinueButton = CreateMorePillButton();
  _moreOrContinueButton.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:_moreOrContinueButton];
  _moreOrContinueButton.accessibilityIdentifier =
      kSearchEngineMoreButtonIdentifier;
  [_moreOrContinueButton addTarget:self
                            action:@selector(moreButtonAction)
                  forControlEvents:UIControlEventTouchUpInside];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = AddPromoStyleWidthLayoutGuide(view);
  // This is the layout guide to compute the bottom margin of the "Set as
  // Default" button.
  UILayoutGuide* buttonBottomMargin = [[UILayoutGuide alloc] init];
  [view addLayoutGuide:buttonBottomMargin];
  // This layout guide is to map `buttonBottomMargin` height into the inline
  // "Set as Default" button container.
  UILayoutGuide* inlineContainerButtonBottomMargin =
      [[UILayoutGuide alloc] init];
  [_inlineSetAsDefaultButtonContainer
      addLayoutGuide:inlineContainerButtonBottomMargin];

  [NSLayoutConstraint activateConstraints:@[
    // Scroll view constraints. It needs to be the full size of the view,
    // so the content is visible in the safe area too.
    [_scrollView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_scrollView.widthAnchor constraintEqualToAnchor:view.widthAnchor],
    [_scrollView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
    [_scrollView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],

    // Scroll content view constraints.
    [scrollContentView.topAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.topAnchor],
    [scrollContentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.bottomAnchor],
    [scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_scrollView.safeAreaLayoutGuide
                                                 .heightAnchor],
    [scrollContentView.centerXAnchor
        constraintEqualToAnchor:_scrollView.centerXAnchor],
    [scrollContentView.widthAnchor
        constraintEqualToAnchor:widthLayoutGuide.widthAnchor],

    // Logo constraints.
    [logoImageView.topAnchor
        constraintEqualToAnchor:scrollContentView.safeAreaLayoutGuide.topAnchor
                       constant:kLogoTopMargin],
    [logoImageView.heightAnchor constraintEqualToConstant:kLogoSize],
    [logoImageView.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],
    [logoImageView.widthAnchor constraintEqualToConstant:kLogoSize],

    // Title constraints.
    [_titleLabel.topAnchor constraintEqualToAnchor:logoImageView.bottomAnchor
                                          constant:kLogoTitleMargin],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_searchEngineStackView.leadingAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_searchEngineStackView.trailingAnchor],

    // SubtitleTextView constraints.
    [subtitleTextView.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                               constant:kTitleSubtitleMargin],
    [subtitleTextView.leadingAnchor
        constraintEqualToAnchor:_searchEngineStackView.leadingAnchor],
    [subtitleTextView.trailingAnchor
        constraintEqualToAnchor:_searchEngineStackView.trailingAnchor],

    // Search engine stack view constraints.
    [_searchEngineStackView.topAnchor
        constraintEqualToAnchor:subtitleTextView.bottomAnchor
                       constant:kSubtitleSearchEngineStackMargin],
    [_searchEngineStackView.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor],
    [_searchEngineStackView.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor],

    // Button bottom margin constraints.
    [buttonBottomMargin.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [buttonBottomMargin.topAnchor
        constraintLessThanOrEqualToAnchor:view.safeAreaLayoutGuide.bottomAnchor
                                 constant:-kActionsBottomMarginWithSafeArea],
    [buttonBottomMargin.topAnchor
        constraintLessThanOrEqualToAnchor:view.bottomAnchor
                                 constant:-kActionsBottomMarginWithoutSafeArea],

    // _inlineSetAsDefaultButtonContainer constraints.
    [_inlineSetAsDefaultButtonContainer.topAnchor
        constraintGreaterThanOrEqualToAnchor:_searchEngineStackView
                                                 .bottomAnchor],
    [_inlineSetAsDefaultButtonContainer.leadingAnchor
        constraintEqualToAnchor:_searchEngineStackView.leadingAnchor],
    [_inlineSetAsDefaultButtonContainer.trailingAnchor
        constraintEqualToAnchor:_searchEngineStackView.trailingAnchor],
    [_inlineSetAsDefaultButtonContainer.bottomAnchor
        constraintEqualToAnchor:scrollContentView.bottomAnchor],

    // inlineContainerButtonBottomMargin constraints.
    [inlineContainerButtonBottomMargin.bottomAnchor
        constraintEqualToAnchor:_inlineSetAsDefaultButtonContainer
                                    .bottomAnchor],
    [inlineContainerButtonBottomMargin.heightAnchor
        constraintEqualToAnchor:buttonBottomMargin.heightAnchor],

    // _inlineSetAsDefaultButton constraints.
    [_inlineSetAsDefaultButton.topAnchor
        constraintEqualToAnchor:_inlineSetAsDefaultButtonContainer.topAnchor
                       constant:kSetAsDefaultButtonTopMargin],
    [_inlineSetAsDefaultButton.bottomAnchor
        constraintEqualToAnchor:inlineContainerButtonBottomMargin.topAnchor],
    [_inlineSetAsDefaultButton.bottomAnchor
        constraintLessThanOrEqualToAnchor:_inlineSetAsDefaultButtonContainer
                                              .bottomAnchor],
    [_inlineSetAsDefaultButton.widthAnchor
        constraintEqualToAnchor:_searchEngineStackView.widthAnchor],
    [_inlineSetAsDefaultButton.centerXAnchor
        constraintEqualToAnchor:_searchEngineStackView.centerXAnchor],

    // More pill button constraints.
    [_moreOrContinueButton.bottomAnchor
        constraintEqualToAnchor:buttonBottomMargin.topAnchor],
    [_moreOrContinueButton.centerXAnchor
        constraintEqualToAnchor:view.centerXAnchor],

    // _floatingSetAsDefaultButtonContainer constraints.
    [_floatingSetAsDefaultButtonContainer.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor],
    // It needs to be as large as the screen so the separator can be as large
    // as the screen.
    [_floatingSetAsDefaultButtonContainer.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor],
    [_floatingSetAsDefaultButtonContainer.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],

    // _floatingContainerSeparator constraints.
    [_floatingContainerSeparator.topAnchor
        constraintEqualToAnchor:_floatingSetAsDefaultButtonContainer.topAnchor],
    [_floatingContainerSeparator.leadingAnchor
        constraintEqualToAnchor:_floatingSetAsDefaultButtonContainer
                                    .leadingAnchor],
    [_floatingContainerSeparator.trailingAnchor
        constraintEqualToAnchor:_floatingSetAsDefaultButtonContainer
                                    .trailingAnchor],
    [_floatingContainerSeparator.heightAnchor
        constraintEqualToConstant:kFloatingContainerSeparatorHeight],

    // _floatingSetAsDefaultButton constraints.
    [_floatingSetAsDefaultButton.topAnchor
        constraintEqualToAnchor:_floatingSetAsDefaultButtonContainer.topAnchor
                       constant:kSetAsDefaultButtonTopMargin],
    [_floatingSetAsDefaultButton.bottomAnchor
        constraintEqualToAnchor:buttonBottomMargin.topAnchor],
    [_floatingSetAsDefaultButton.widthAnchor
        constraintEqualToAnchor:_searchEngineStackView.widthAnchor],
    [_floatingSetAsDefaultButton.centerXAnchor
        constraintEqualToAnchor:_searchEngineStackView.centerXAnchor],
  ]];
  // No need to update the more and SetAsDefault buttons. They will be updated
  // when the view will be appearing.
  [self loadSearchEngineButtons];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(accessibilityElementFocusedNotification:)
             name:UIAccessibilityElementFocusedNotification
           object:nil];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
      UITraitPreferredContentSizeCategory.self, UITraitVerticalSizeClass.self,
      UITraitHorizontalSizeClass.self
    ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateUIOnTraitChange)];
  }
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  // Using -[UIViewController viewWillAppear:] is too early. There is an issue
  // on iPhone, the safe area is not visible yet.
  // Using -[UIViewController viewDidAppear:] is too late. There is an issue on
  // iPad, the More button appears and then disappears.
  [self.view layoutIfNeeded];
  // After the last layout before appearing, now, the views can be updated.
  _viewIsAppearingCalled = YES;
  [self updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:NO];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:YES];
}

#pragma mark - SearchEngineChoiceTableConsumer

- (void)setSearchEngines:(NSArray<SnippetSearchEngineElement*>*)searchEngines {
  _searchEngines = searchEngines;
  [self loadSearchEngineButtons];
}

#pragma mark - UITraitEnvironment

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange];
}
#endif

#pragma mark - Private

// Called when the tap on a SnippetSearchEngineButton.
- (void)searchEngineTapAction:(SnippetSearchEngineButton*)button {
  BOOL wasSelectedYet = _selectedSearchEngineButton != nil;
  [self.mutator selectSearchEnginewWithKeyword:button.searchEngineKeyword];
  _selectedSearchEngineButton.checked = NO;
  _selectedSearchEngineButton = button;
  _selectedSearchEngineButton.checked = YES;
  if (wasSelectedYet) {
    return;
  }
  EnableSetAsDefaultButton(_inlineSetAsDefaultButton, /*is_enabled=*/YES);
  EnableSetAsDefaultButton(_floatingSetAsDefaultButton, /*is_enabled=*/YES);
  if (!_moreOrContinueButton) {
    // If the more pill button is not visible, the user already saw the last
    // search engine, and since they selected one, then the "Set as Default"
    // button can appear now.
    [self animateFloatingSetAsDefaultContainer];
  } else {
    // After selecting a search engine, needs to scroll down to see all
    // search engines before tapping on the "Set as Default" button.
    SetPillButtonTitle(_moreOrContinueButton,
                       IDS_SEARCH_ENGINE_CHOICE_CONTINUE_BUTTON);
    _moreOrContinueButton.accessibilityIdentifier =
        kSearchEngineContinueButtonIdentifier;
  }
}

// Animates the floating SetAsDefault button to:
//  1- Fades from grey to blue, to become enabled.
//  2- Appears on the screen by moving from the bottom (if the floating
//     SetAsDefault is not visible yet).
//  3- Scrolls up the scrollview to avoid covering the selected search engine.
- (void)animateFloatingSetAsDefaultContainer {
  CHECK(!_moreOrContinueButton, base::NotFatalUntil::M127);

  // 1- Fades grey color to blue color to have better animation.
  UIButton* fakeButtonForGreyToBlueFading = nil;
  CGPoint inlineButtonOriginInMainView =
      [self.view convertPoint:_inlineSetAsDefaultButton.bounds.origin
                     fromView:_inlineSetAsDefaultButton];
  if (inlineButtonOriginInMainView.y < self.view.bounds.size.height ||
      !_floatingSetAsDefaultButtonContainer.hidden) {
    // When the inline button is visible, a fake Set as Default button is added
    // in the floating container. The fake button is as the same color than
    // the inline button.
    // The fake button is faded out at the same time than the floating container
    // is moved up.
    fakeButtonForGreyToBlueFading = CreateSetAsDefaultButton();
    fakeButtonForGreyToBlueFading.translatesAutoresizingMaskIntoConstraints =
        YES;
    fakeButtonForGreyToBlueFading.frame = _floatingSetAsDefaultButton.frame;
    [_floatingSetAsDefaultButtonContainer
        addSubview:fakeButtonForGreyToBlueFading];
  }
  // Hide the inline SetAsDefault button. It is replace by
  // `fakeButtonForGreyToBlueFading` during the animation.
  _inlineSetAsDefaultButtonContainer.hidden = YES;

  // 2- Sets the starting point of the floating SetAsDefault container to move
  //    up. The starting point should be the position of the inline SetAsDefault
  //    container.
  //    If the floating SetAsDefault container is already visible, there is
  //    nothing to do.
  //    If the inline SetAsDefault container is not visible, the starting point
  //    is the bottom of the view.
  // Rect of the floating container at the end of the animation.
  CGRect animationEndFrame = _floatingSetAsDefaultButtonContainer.frame;
  // Computes and sets the origin of the animation based on the inline
  // container.
  // Rect of the floating container at the beginning of the animation.
  CGRect animationStartFrame = animationEndFrame;
  if (_floatingSetAsDefaultButtonContainer.hidden) {
    // The origin point for the animation should be the origin of the inline
    // container.
    CGPoint animationStartOriginPoint =
        [self.view convertPoint:_inlineSetAsDefaultButtonContainer.bounds.origin
                       fromView:_inlineSetAsDefaultButtonContainer];
    if (animationStartOriginPoint.y > self.view.bounds.size.height) {
      // If the inline container is below the bottom of the view, then
      // the floating container should start at the bottom of the view.
      animationStartOriginPoint.y = self.view.frame.size.height;
    }
    animationStartFrame.origin.y = animationStartOriginPoint.y;
    if (UIAccessibilityPrefersCrossFadeTransitions()) {
      // `_floatingSetAsDefaultButtonContainer` should not appear with a
      // transition, but with a fade in.
      // `animationStartFrame` is unchanged to be able to compute
      // `heightToScrollUp` value.
      _floatingSetAsDefaultButtonContainer.hidden = NO;
      _floatingSetAsDefaultButtonContainer.alpha = 0.;
    } else {
      _floatingSetAsDefaultButtonContainer.frame = animationStartFrame;
    }
    [self makeFloatingSetAsDefaultButtonContainerVisible];
  }

  // 3- At the end of the animation, if the floating SetAsDefault container
  //    will cover the selected search engine, the scroll view needs to move up
  //    as much as the floating SetAsDefault container will move up.
  CGRect selectedButtonRect = _selectedSearchEngineButton.bounds;
  selectedButtonRect = [self.view convertRect:selectedButtonRect
                                     fromView:_selectedSearchEngineButton];
  CGFloat heightToScrollUp = 0.;
  // Tests if the floating SetAsDefault button will cover the selected search
  // engine button, after the animation.
  // If this is true, then scroll view needs to scroll up to compensate
  // the floating SetAsDefault button animation. This value is
  // the begining position height minus the end position height.
  // So the scrollview will move exactly at the same time than the button.
  if (selectedButtonRect.origin.y + selectedButtonRect.size.height >
      animationEndFrame.origin.y) {
    heightToScrollUp =
        animationEndFrame.origin.y - animationStartFrame.origin.y;
  }

  // Animates everything.
  UIView* floatingSetAsDefaultButtonContainer =
      _floatingSetAsDefaultButtonContainer;
  UIScrollView* scrollView = _scrollView;
  [UIView animateWithDuration:kFloatingSetAsDefaultAnimationDuration
      animations:^{
        // 1- Fades in.
        fakeButtonForGreyToBlueFading.alpha = 0;
        // 2- Moves from the bottom or fade in.
        floatingSetAsDefaultButtonContainer.frame = animationEndFrame;
        floatingSetAsDefaultButtonContainer.alpha = 1.;
        // 3- Scrolls up, if needed.
        if (heightToScrollUp) {
          CGPoint contentOffset = scrollView.contentOffset;
          contentOffset.y -= heightToScrollUp;
          scrollView.contentOffset = contentOffset;
        }
      }
      completion:^(BOOL) {
        [fakeButtonForGreyToBlueFading removeFromSuperview];
      }];
}

// Called when the user taps on the SetAsDefault button.
- (void)setAsDefaultButtonAction {
  [self.actionDelegate didTapPrimaryButton];
}

// Called when the user taps on the more/continue pill button.
- (void)moreButtonAction {
  [self animateMorePillButtonAway];
  // Adding 1 to the content offset to make sure the scroll view will reach
  // the bottom of view to trigger the floating SetAsDefault container when
  // `updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:` will be
  // called.
  // See crbug.com/332719699.
  CGPoint bottomOffset = CGPointMake(
      0, _scrollView.contentSize.height - _scrollView.bounds.size.height +
             _scrollView.adjustedContentInset.bottom + 1);
  [_scrollView setContentOffset:bottomOffset animated:YES];
}

// Loads the search engine buttons from `_searchEngines`.
- (void)loadSearchEngineButtons {
  NSString* selectedSearchEngineKeyword =
      _selectedSearchEngineButton.searchEngineKeyword;
  _selectedSearchEngineButton = nil;
  // This set saves the list of search engines that are expanded to keep them
  // expanded after loading the search engine list.
  NSMutableSet<NSString*>* expandedSearchEngineKeyword = [NSMutableSet set];
  for (SnippetSearchEngineButton* oldSearchEngineButton in
           _searchEngineStackView.arrangedSubviews) {
    if (oldSearchEngineButton.snippetButtonState ==
        SnippetButtonState::kExpanded) {
      [expandedSearchEngineKeyword
          addObject:oldSearchEngineButton.searchEngineKeyword];
    }
    [_searchEngineStackView removeArrangedSubview:oldSearchEngineButton];
    [oldSearchEngineButton removeFromSuperview];
  }
  SnippetSearchEngineButton* button = nil;
  for (SnippetSearchEngineElement* element in _searchEngines) {
    button = CreateSnippetSearchEngineButtonWithElement(element);
    button.animatedLayoutView = _scrollView;
    if ([expandedSearchEngineKeyword containsObject:element.keyword]) {
      button.snippetButtonState = SnippetButtonState::kExpanded;
    }
    if ([selectedSearchEngineKeyword isEqualToString:element.keyword]) {
      button.checked = YES;
      _selectedSearchEngineButton = button;
    }
    [button addTarget:self
                  action:@selector(searchEngineTapAction:)
        forControlEvents:UIControlEventTouchUpInside];
    [_searchEngineStackView addArrangedSubview:button];
  }
  // Hide the horizontal seperator for the last button.
  button.horizontalSeparatorHidden = YES;
  _searchEnginesLoaded = YES;
  [self.view layoutIfNeeded];
  [self updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:YES];
}

// Updates views:
// 1- The horizontal separator in the floating SetAsDefault is visible only
//    when the floating SetAsDefault container is on top of the search engine
//    stack view
// 2- If the scroll view reaches the end of the last search engine button
//    for the first time, and hides the more button accordingly.
// 3- If the scroll view reaches the bottom, the inline SetAsDefault container
//    is hidden, and the floating SetAsDefault container is visible.
- (void)updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:
    (BOOL)morePillButtonAnimation {
  // 1- Tests if the stack view is covered by the floating SetAsDefault
  //    container, and makes `_floatingContainerSeparator` visible if it is
  //    the case.
  CGPoint stackViewBottomPoint =
      CGPointMake(_searchEngineStackView.frame.origin.x,
                  _searchEngineStackView.bounds.origin.y +
                      _searchEngineStackView.bounds.size.height);
  stackViewBottomPoint = [self.view convertPoint:stackViewBottomPoint
                                        fromView:_searchEngineStackView];
  _floatingContainerSeparator.hidden =
      stackViewBottomPoint.y <
      _floatingSetAsDefaultButtonContainer.frame.origin.y + 1;
  if (!_viewIsAppearingCalled || _showFloatingSetAsDefaultButton ||
      !self.presentingViewController || !_searchEnginesLoaded) {
    // Don't update the value if the view is not ready to appear.
    // Don't update the value if the bottom was reached at least once.
    // Don't update the value if the view is not presented yet.
    // Don't update the value if the search engines have not been loaded yet.
    return;
  }
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;

  // 2- Hides `_moreOrContinueButton` if the scroll view reaches the end of
  //    the stack view.
  // The limit to remove the more button is when `_searchEngineStackView` is
  // fully visible.
  CGFloat bottomStackViewLimit = _searchEngineStackView.frame.origin.y +
                                 _searchEngineStackView.frame.size.height;
  if (scrollPosition >= bottomStackViewLimit) {
    if (morePillButtonAnimation) {
      [self animateMorePillButtonAway];
    } else {
      [_moreOrContinueButton removeFromSuperview];
      _moreOrContinueButton = nil;
    }
  }

  // 3- Reveals the floating SetAsDefault container, and hides the inline
  //    SetAsDefault container, if the scroll view reaches the bottom.
  CGFloat scrollLimit =
      _scrollView.contentSize.height + _scrollView.adjustedContentInset.bottom;
  if (scrollPosition >= scrollLimit) {
    // Scroll reached the bottom, the inline SetAsDefault button needs to be
    // hidden, and the floating SetAsDefault button needs to be visible.
    _showFloatingSetAsDefaultButton = YES;
    _inlineSetAsDefaultButtonContainer.hidden = YES;
    [self makeFloatingSetAsDefaultButtonContainerVisible];
  }
}

// Makes the floating container visible.
- (void)makeFloatingSetAsDefaultButtonContainerVisible {
  _floatingSetAsDefaultButtonContainer.hidden = NO;
  [self adjustInsetVerticalScroller];
}

// Adjust the vertical scroller inset if the floating container is visible.
- (void)adjustInsetVerticalScroller {
  if (_floatingSetAsDefaultButtonContainer.hidden) {
    return;
  }
  // The bottom inset should not include the safe area height.
  CGFloat bottomInset = _floatingSetAsDefaultButtonContainer.frame.size.height -
                        _scrollView.adjustedContentInset.bottom;
  _scrollView.verticalScrollIndicatorInsets =
      UIEdgeInsetsMake(0, 0, bottomInset, 0);
}

// Animate the more pill button to disappear to the bottom of the screen.
- (void)animateMorePillButtonAway {
  if (!_moreOrContinueButton) {
    return;
  }
  UIButton* button = _moreOrContinueButton;
  _moreOrContinueButton = nil;
  CGAffineTransform transform = button.transform;
  CGFloat translateDistance =
      CGRectGetMaxY(self.view.bounds) - CGRectGetMinY(button.frame);
  transform = CGAffineTransformTranslate(transform, 0, translateDistance);
  [UIView animateWithDuration:kMorePillButtonAnimationDuration
      animations:^{
        if (UIAccessibilityPrefersCrossFadeTransitions()) {
          button.alpha = 0;
        } else {
          button.transform = transform;
        }
      }
      completion:^(BOOL finished) {
        [button removeFromSuperview];
      }];
}

// Scrolls automatically `_scrollView` to make sure the search engine button
// is always fully visible and not hidden by
// `_floatingSetAsDefaultButtonContainer`.
- (void)accessibilityElementFocusedNotification:(NSNotification*)notification {
  CHECK([notification.name
      isEqualToString:UIAccessibilityElementFocusedNotification])
      << base::SysNSStringToUTF8(notification.name);
  id focusedElement = notification.userInfo[UIAccessibilityFocusedElementKey];
  if (!focusedElement ||
      ![focusedElement isKindOfClass:SnippetSearchEngineButton.class] ||
      _floatingSetAsDefaultButtonContainer.hidden) {
    return;
  }
  SnippetSearchEngineButton* searchEngineButton =
      base::apple::ObjCCast<SnippetSearchEngineButton>(focusedElement);
  // Get the bottom of `searchEngineButton` in the reference of `self.view`.
  CGFloat searchEngineButtonBottom =
      ConvertVerticalCoordonateWithMainViewReference(
          self.view, searchEngineButton,
          CGRectGetMaxY(searchEngineButton.bounds));
  // Get the top of `floatingSetAsDefaultContainerTop` in the reference of
  // `self.view`.
  CGFloat floatingSetAsDefaultContainerTop =
      ConvertVerticalCoordonateWithMainViewReference(
          self.view, _floatingSetAsDefaultButtonContainer,
          CGRectGetMinY(_floatingSetAsDefaultButtonContainer.bounds));
  if (searchEngineButtonBottom <= floatingSetAsDefaultContainerTop) {
    // The bottom of `searchEngineButton` is visible, no need to scroll.
    return;
  }
  // `_scrollView` should go down to reveal the bottom of `searchEngineButton`.
  CGFloat distanceToScrollDown =
      searchEngineButtonBottom - floatingSetAsDefaultContainerTop;
  // Get the top of `searchEngineButton` in the reference of `self.view`.
  CGFloat searchEngineButtonTop =
      ConvertVerticalCoordonateWithMainViewReference(
          self.view, searchEngineButton,
          CGRectGetMinY(searchEngineButton.bounds));
  if (searchEngineButtonTop - distanceToScrollDown < 0) {
    // If the distance to scroll will hide the top of `searchEngineButton`,
    // the scroll distance should be reduced to make sure at the top is visible.
    distanceToScrollDown += searchEngineButtonTop - distanceToScrollDown;
  }
  // Update the scroll position.
  CGPoint contentOffset = _scrollView.contentOffset;
  contentOffset.y += distanceToScrollDown;
  _scrollView.contentOffset = contentOffset;
}

// Updates the title font and various scroll properties when the view
// controller's UITraits change.
- (void)updateUIOnTraitChange {
  // Reset the title font to make sure that it is
  // properly scaled.
  UIFontTextStyle textStyle = GetTitleLabelFontTextStyle(self);
  _titleLabel.font = GetFRETitleFont(textStyle);
  // Update the SetAsDefault button once the layout changes take effect to have
  // the right measurements to evaluate the scroll position.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:YES];
  });
  // Adjust the inset vertical scroller since floating container size might have
  // be updated.
  [self adjustInsetVerticalScroller];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionDelegate showLearnMore];
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

#pragma mark - UIContentContainer

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        // Recompute if the user reached the bottom, once the animation is done.
        // This needs be done at the beginning of the transition to have a
        // smooth transition.
        [weakSelf
            updateViewsBasedOnScrollPositionWithMorePillButtonAnimation:YES];
      }
                      completion:nil];
}

@end
