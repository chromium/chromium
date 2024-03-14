// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"

#import "base/check.h"
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
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Line width for the bottom separator.
constexpr CGFloat kLineWidth = 1.;
// The horizontal space between the safe area edges and the view elements.
constexpr CGFloat kHorizontalInsets = -48.;
// Space between the Chrome logo and the top of the screen.
constexpr CGFloat kTopSpacing = 40.;
// Space between the elements of the top stack view and around the primary
// button.
constexpr CGFloat kDefaultMargin = 16.;
// Logo dimensions.
constexpr CGFloat kLogoSize = 50.;
// Stack view margin.
constexpr CGFloat kStackViewMargin = 24.;

// URL for the "Learn more" link.
const char* const kLearnMoreURL = "internal://choice-screen-learn-more";

SnippetSearchEngineButton* CreateSnippetSearchEngineButtonWithElement(
    SnippetSearchEngineElement* element) {
  CHECK(element.keyword);
  SnippetSearchEngineButton* button = [[SnippetSearchEngineButton alloc] init];
  button.faviconImage = element.faviconImage;
  button.nameLabel.text = element.name;
  button.snippetText = element.snippetDescription;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.searchEngineKeyword = element.keyword;
  button.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 element.name];
  return button;
}

}  // namespace

@interface SearchEngineChoiceViewController () <UITextViewDelegate>
@end

@implementation SearchEngineChoiceViewController {
  // The screen's title
  NSString* _titleString;
  // Button to confirm the default search engine selection.
  UIButton* _primaryButton;
  // View that contains all the UI elements above the search engine table.
  UIStackView* _topZoneStackView;
  // The chrome logo.
  UIImageView* _logoView;
  // The view title.
  UILabel* _titleLabel;
  // Some informational text above the search engines table.
  UITextView* _subtitleTextView;
  // Separator between the search engines table and the primary button.
  UIView* _separatorView;
  // Scrollable content containing everything above the primary button.
  UIScrollView* _scrollView;
  UIView* _scrollContentView;
  // Whether the choice screen is being displayed for the FRE.
  BOOL _isForFRE;
  BOOL _didReachBottom;
  // Search engine element chosen by the user.
  SnippetSearchEngineElement* _chosenSearchEngineElement;
  UIStackView* _searchEngineStackView;
  SnippetSearchEngineButton* _selectedSearchEngineButton;
  // Wether `-[SearchEngineChoiceViewController viewWillAppear]` was called.
  BOOL _viewWillAppearCalled;
}

@synthesize searchEngines = _searchEngines;

- (instancetype)initWithFirstRunMode:(BOOL)isForFRE {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _isForFRE = isForFRE;
  }
  return self;
}

- (void)updatePrimaryActionButton {
  UpdatePrimaryButton(_primaryButton, _didReachBottom,
                      _selectedSearchEngineButton != nil);
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  _scrollContentView = [[UIView alloc] init];
  _scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;

  _topZoneStackView = [[UIStackView alloc] init];
  [_scrollContentView addSubview:_topZoneStackView];
  _topZoneStackView.axis = UILayoutConstraintAxisVertical;
  _topZoneStackView.spacing = kDefaultMargin;
  _topZoneStackView.distribution = UIStackViewDistributionEqualSpacing;
  _topZoneStackView.alignment = UIStackViewAlignmentCenter;
  _topZoneStackView.translatesAutoresizingMaskIntoConstraints = NO;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  _logoView = [[UIImageView alloc]
      initWithImage:MakeSymbolMulticolor(CustomSymbolWithPointSize(
                        kMulticolorChromeballSymbol, kLogoSize))];
#else
  _logoView = [[UIImageView alloc]
      initWithImage:CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize)];
#endif
  [_topZoneStackView addArrangedSubview:_logoView];
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    _logoView.hidden = YES;
  }
  _logoView.translatesAutoresizingMaskIntoConstraints = NO;

  _titleLabel = [[UILabel alloc] init];
  // Add semantic group to have a coherent behaviour with the table view and
  // the primary button, this is related to VoiceOver.
  _titleLabel.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  [_topZoneStackView addArrangedSubview:_titleLabel];
  [_titleLabel
      setText:l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE)];
  [_titleLabel setTextColor:[UIColor colorNamed:kSolidBlackColor]];
  _titleLabel.font = GetTitleFontWithTraitCollection(self.traitCollection);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  [_titleLabel setTextAlignment:NSTextAlignmentCenter];
  [_titleLabel setNumberOfLines:0];
  [_titleLabel setAccessibilityIdentifier:
                   kSearchEngineChoiceTitleAccessibilityIdentifier];
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

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

  _subtitleTextView = [[UITextView alloc] init];
  [_topZoneStackView addArrangedSubview:_subtitleTextView];
  [_subtitleTextView setAttributedText:subtitleText];
  [_subtitleTextView
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
  _subtitleTextView.backgroundColor = nil;
  _subtitleTextView.adjustsFontForContentSizeCategory = YES;
  [_subtitleTextView setTextAlignment:NSTextAlignmentCenter];
  _subtitleTextView.delegate = self;
  _subtitleTextView.scrollEnabled = NO;
  _subtitleTextView.editable = NO;
  _subtitleTextView.translatesAutoresizingMaskIntoConstraints = NO;

  _searchEngineStackView = [[UIStackView alloc] init];
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the primary button, using VoiceOver.
  _searchEngineStackView.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;
  _searchEngineStackView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _searchEngineStackView.layer.cornerRadius = 12.;
  _searchEngineStackView.layer.masksToBounds = YES;
  _searchEngineStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _searchEngineStackView.axis = UILayoutConstraintAxisVertical;
  [_scrollContentView addSubview:_searchEngineStackView];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.accessibilityIdentifier = kSearchEngineChoiceScrollViewIdentifier;
  _scrollView.delegate = self;
  [_scrollView addSubview:_scrollContentView];
  [self.view addSubview:_scrollView];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  _separatorView = [[UIView alloc] init];
  [self.view addSubview:_separatorView];
  _separatorView.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [self.view bringSubviewToFront:_separatorView];
  _separatorView.translatesAutoresizingMaskIntoConstraints = NO;

  _primaryButton = CreateMorePrimaryButton();

  [self.view addSubview:_primaryButton];
  [_primaryButton addTarget:self
                     action:@selector(primaryButtonAction)
           forControlEvents:UIControlEventTouchUpInside];
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the primary button, using VoiceOver.
  _primaryButton.accessibilityContainerType =
      UIAccessibilityContainerTypeSemanticGroup;

  [NSLayoutConstraint activateConstraints:@[
    // Scroll view constraints.
    [_scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_scrollView.widthAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.widthAnchor],
    [_scrollView.bottomAnchor
        constraintEqualToAnchor:_separatorView.bottomAnchor],

    // Scroll content view constraints.
    [_scrollContentView.topAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.topAnchor],
    [_scrollContentView.widthAnchor
        constraintEqualToAnchor:_scrollView.widthAnchor],
    [_scrollContentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.bottomAnchor],
    [_scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor],

    [_topZoneStackView.topAnchor
        constraintEqualToAnchor:_scrollContentView.topAnchor
                       constant:kTopSpacing],
    [_topZoneStackView.widthAnchor
        constraintEqualToAnchor:_scrollContentView.widthAnchor
                       constant:kHorizontalInsets],

    [_logoView.widthAnchor constraintEqualToConstant:kLogoSize],
    [_logoView.heightAnchor constraintEqualToConstant:kLogoSize],

    [_primaryButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kDefaultMargin],
    [_primaryButton.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                               constant:kHorizontalInsets],

    [_separatorView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [_separatorView.heightAnchor constraintEqualToConstant:kLineWidth],
    [_separatorView.bottomAnchor
        constraintEqualToAnchor:_primaryButton.topAnchor
                       constant:-kDefaultMargin],

    [_searchEngineStackView.topAnchor
        constraintEqualToAnchor:_subtitleTextView.bottomAnchor],
    [_searchEngineStackView.bottomAnchor
        constraintEqualToAnchor:_scrollContentView.bottomAnchor
                       constant:-kStackViewMargin],
    [_searchEngineStackView.leadingAnchor
        constraintEqualToAnchor:_scrollContentView.leadingAnchor
                       constant:kStackViewMargin],
    [_searchEngineStackView.trailingAnchor
        constraintEqualToAnchor:_scrollContentView.trailingAnchor
                       constant:-kStackViewMargin],

    [self.view.centerXAnchor
        constraintEqualToAnchor:_primaryButton.centerXAnchor],
    [self.view.centerXAnchor
        constraintEqualToAnchor:_topZoneStackView.centerXAnchor],
    [self.view.centerXAnchor
        constraintEqualToAnchor:_separatorView.centerXAnchor],
    [self.view.centerXAnchor constraintEqualToAnchor:_scrollView.centerXAnchor],
    [_scrollView.centerXAnchor
        constraintEqualToAnchor:_scrollContentView.centerXAnchor],
  ]];
  [self updatePrimaryActionButton];
  [self loadSearchEngineButtons];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  _viewWillAppearCalled = YES;
  // Update all views sizes before checking if the scroll view is at the bottom.
  [self.view layoutIfNeeded];
  [self updateDidReachBottomFlag];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateDidReachBottomFlag];
}

#pragma mark - SearchEngineChoiceTableConsumer

- (void)setSearchEngines:(NSArray<SnippetSearchEngineElement*>*)searchEngines {
  _searchEngines = searchEngines;
  [self loadSearchEngineButtons];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Reset the title font to make sure that it is
  // properly scaled.
  _titleLabel.font = GetTitleFontWithTraitCollection(self.traitCollection);
}

#pragma mark - Private

// Called when the tap on a SnippetSearchEngineButton.
- (void)searchEngineTapAction:(SnippetSearchEngineButton*)button {
  [self.mutator selectSearchEnginewWithKeyword:button.searchEngineKeyword];
  _selectedSearchEngineButton.checked = NO;
  _selectedSearchEngineButton = button;
  _selectedSearchEngineButton.checked = YES;
  [self updatePrimaryActionButton];
}

// Called when the user tap on the primary button.
- (void)primaryButtonAction {
  if (_didReachBottom) {
    [self.actionDelegate didTapPrimaryButton];
  } else {
    CGPoint bottomOffset = CGPointMake(0, _scrollView.contentSize.height -
                                              _scrollView.bounds.size.height +
                                              _scrollView.contentInset.bottom);
    [_scrollView setContentOffset:bottomOffset animated:YES];
  }
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
  [self.view layoutSubviews];
  [self updateDidReachBottomFlag];
}

- (void)updateDidReachBottomFlag {
  if (!_viewWillAppearCalled || _didReachBottom ||
      !self.presentingViewController) {
    // Don't update the value if the view is not ready to appear.
    // Don't update the value if the bottom was reached at least once.
    // Don't update the value if the view is not presented yet.
    return;
  }
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;
  CGFloat scrollLimit =
      _scrollView.contentSize.height + _scrollView.contentInset.bottom;
  if (scrollPosition >= scrollLimit) {
    _didReachBottom = YES;
    [self updatePrimaryActionButton];
  }
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionDelegate showLearnMore];
  return NO;
}

#pragma mark - UIContentContainer

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  switch (newCollection.verticalSizeClass) {
      // `hidden` is not an animatable property so we use `alpha` to make the
      // transition smooth.
    case UIUserInterfaceSizeClassRegular:
      _logoView.alpha = 1;
      _logoView.hidden = NO;
      break;
    case UIUserInterfaceSizeClassCompact:
      _logoView.alpha = 0;
      _logoView.hidden = YES;
      break;
    default:
      break;
  };
}

@end
