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
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Chrome logo with 40pt size.
NSString* const kChromeIcon40pt = @"chrome_icon_40";
// Line width for the bottom separator.
constexpr CGFloat kLineWidth = 1.;
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
// Margin above and below the button.
constexpr CGFloat kButtonMargin = 16.;
// Width margin (wide or narrow, depending on the desired layout).
constexpr CGFloat kWidthMarginWide = 54.;
constexpr CGFloat kWidthMarginNarrow = 24.;

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
  button.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 element.name];
  return button;
}

}  // namespace

@interface SearchEngineChoiceViewController () <UITextViewDelegate>
@end

@implementation SearchEngineChoiceViewController {
  // Button to confirm the default search engine selection.
  UIButton* _primaryButton;
  // The view title.
  UILabel* _titleLabel;
  // Scrollable content containing everything above the primary button.
  UIScrollView* _scrollView;
  // Whether the choice screen is being displayed for the FRE.
  BOOL _isForFRE;
  // The horizontal margin.
  CGFloat _marginWidth;
  // Whether the scroll view reached the bottom at least once.
  BOOL _didReachBottom;
  // Contains the list of search engine buttons.
  UIStackView* _searchEngineStackView;
  // Contains the selected search engine button.
  SnippetSearchEngineButton* _selectedSearchEngineButton;
  // Whether `-[SearchEngineChoiceViewController viewIsAppearing:]` was called.
  BOOL _viewIsAppearingCalled;
  // Whether the search engine buttons have been loaded in the stack view.
  BOOL _searchEnginesLoaded;
}

@synthesize searchEngines = _searchEngines;

- (instancetype)initWithFirstRunMode:(BOOL)isForFRE
                     wideMarginWidth:(BOOL)wideMarginWidth {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _isForFRE = isForFRE;
    _marginWidth = wideMarginWidth ? kWidthMarginWide : kWidthMarginNarrow;
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

  // Add main scroll view with its content view.
  UIView* scrollContentView = [[UIView alloc] init];
  scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView = [[UIScrollView alloc] init];
  [self.view addSubview:_scrollView];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.accessibilityIdentifier = kSearchEngineChoiceScrollViewIdentifier;
  _scrollView.delegate = self;
  [_scrollView addSubview:scrollContentView];

  // Need to use a regular png instead of custom symbol to have a better control
  // on the size and the margin of the logo.
  UIImage* logoImage = [UIImage imageNamed:kChromeIcon40pt];
  UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logoImage];
  [scrollContentView addSubview:logoImageView];
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;

  _titleLabel = [[UILabel alloc] init];
  // Add semantic group, so the user can skip all the search engine stack view,
  // and jump to the primary button, using VoiceOver.
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
  [scrollContentView addSubview:_searchEngineStackView];

  UIView* separatorView = [[UIView alloc] init];
  [self.view addSubview:separatorView];
  separatorView.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [self.view bringSubviewToFront:separatorView];
  separatorView.translatesAutoresizingMaskIntoConstraints = NO;

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
    [_scrollView.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],

    // Scroll content view constraints.
    [scrollContentView.topAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.topAnchor],
    [scrollContentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.bottomAnchor],
    [scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor],
    [scrollContentView.centerXAnchor
        constraintEqualToAnchor:_scrollView.centerXAnchor],
    [scrollContentView.widthAnchor
        constraintEqualToAnchor:_scrollView.widthAnchor],

    // Logo.
    [logoImageView.topAnchor constraintEqualToAnchor:scrollContentView.topAnchor
                                            constant:kLogoTopMargin],
    [logoImageView.heightAnchor constraintEqualToConstant:kLogoSize],
    [logoImageView.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],
    [logoImageView.widthAnchor constraintEqualToConstant:kLogoSize],

    // Title.
    [_titleLabel.topAnchor constraintEqualToAnchor:logoImageView.bottomAnchor
                                          constant:kLogoTitleMargin],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_searchEngineStackView.leadingAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_searchEngineStackView.trailingAnchor],

    // SubtitleTextView.
    [subtitleTextView.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                               constant:kTitleSubtitleMargin],
    [subtitleTextView.leadingAnchor
        constraintEqualToAnchor:_searchEngineStackView.leadingAnchor],
    [subtitleTextView.trailingAnchor
        constraintEqualToAnchor:_searchEngineStackView.trailingAnchor],

    // Search engine stack view.
    [_searchEngineStackView.topAnchor
        constraintEqualToAnchor:subtitleTextView.bottomAnchor
                       constant:kSubtitleSearchEngineStackMargin],
    [_searchEngineStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:scrollContentView.bottomAnchor
                                 constant:-_marginWidth],
    [_searchEngineStackView.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor
                       constant:_marginWidth],
    [_searchEngineStackView.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor
                       constant:-_marginWidth],
    [_searchEngineStackView.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],

    // Separator.
    [separatorView.topAnchor constraintEqualToAnchor:_scrollView.bottomAnchor],
    [separatorView.heightAnchor constraintEqualToConstant:kLineWidth],
    [separatorView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [separatorView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    // Primary button.
    [_primaryButton.topAnchor constraintEqualToAnchor:separatorView.bottomAnchor
                                             constant:kButtonMargin],
    [_primaryButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kButtonMargin],
    [_primaryButton.widthAnchor
        constraintEqualToAnchor:_searchEngineStackView.widthAnchor],
    [_primaryButton.centerXAnchor
        constraintEqualToAnchor:_searchEngineStackView.centerXAnchor],
  ]];
  [self updatePrimaryActionButton];
  [self loadSearchEngineButtons];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  _viewIsAppearingCalled = YES;
  // Using -[UIViewController viewWillAppear:] is too early. There is an issue
  // on iPhone, the safe area is not visible yet.
  // Using -[UIViewController viewDidAppear:] is too late. There is an issue on
  // iPad, the More button appears and then disappears.
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
  UIFontTextStyle textStyle = GetTitleLabelFontTextStyle(self);
  _titleLabel.font = GetFRETitleFont(textStyle);
  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf updateDidReachBottomFlag];
  });
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
    if (_selectedSearchEngineButton) {
      [self.actionDelegate didTapPrimaryButton];
    }
  } else {
    // Adding 1 to the content offset to make sure the scroll view will reach
    // the bottom of view to trigger the floating SetAsDefault container when
    // `updateViewsBasedOnScrollPosition` will be called.
    // See crbug.com/332719699.
    CGPoint bottomOffset = CGPointMake(
        0, _scrollView.contentSize.height - _scrollView.bounds.size.height +
               _scrollView.contentInset.bottom + 1);
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
  _searchEnginesLoaded = YES;
  [self.view layoutIfNeeded];
  [self updateDidReachBottomFlag];
}

// Tests if the scroll view reached the end of the last search engine button
// for the first time, and hides the more button accordingly.
- (void)updateDidReachBottomFlag {
  if (!_viewIsAppearingCalled || _didReachBottom ||
      !self.presentingViewController || !_searchEnginesLoaded) {
    // Don't update the value if the view is not ready to appear.
    // Don't update the value if the bottom was reached at least once.
    // Don't update the value if the view is not presented yet.
    // Don't update the value if the search engines have not been loaded yet.
    return;
  }
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;
  // The limit to remove the more button is when `_searchEngineStackView` is
  // fully visible.
  CGFloat scrollLimit = _searchEngineStackView.frame.origin.y +
                        _searchEngineStackView.frame.size.height +
                        _scrollView.contentInset.bottom;
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

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:nil
                      completion:^(
                          id<UIViewControllerTransitionCoordinatorContext>
                              unused) {
                        // Recompute if the user reached the bottom, once the
                        // animation is done.
                        [weakSelf updateDidReachBottomFlag];
                      }];
}

@end
