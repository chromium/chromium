// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/fake_omnibox/fake_omnibox_view.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Accessibility Identifier.
NSString* const kSearchEngineChoiceTitleAccessibilityIdentifier =
    @"SearchEngineChoiceTitleAccessibilityIdentifier";
// Line width for the bottom separator.
constexpr CGFloat kLineWidth = 1.;
// The horizontal space between the safe area edges and the view elements.
constexpr CGFloat kHorizontalInsets = -48.;
// Space between the Chrome logo and the top of the screen.
constexpr CGFloat kTopSpacing = 40.;
// Spacing between the elements of the top stack view.
constexpr CGFloat kTopStackViewSpacing = 16.;
// Space above and below the primary button.
constexpr CGFloat kPrimaryButtonPadding = 14.;
// Primary button height.
constexpr CGFloat kPrimaryButtonHeight = 50.;
// Logo dimensions.
constexpr CGFloat kLogoSize = 50.;
// The minimum height of the search engines table.
// TODO(b/280753739): Figure out a way to make this the height of five rows.
constexpr CGFloat kMinimumTableHeight = 300.;
// Specifications for the fake omnibox animation. Durations are in seconds.
constexpr CGFloat kEntranceAnimationDuration = 0.6;
constexpr CGFloat kExitAnimationDuration = 0.3;
constexpr CGFloat kSpringDamping = 0.6;
// Angle in radians of the fake omnibox rotation.
constexpr CGFloat kRotationAngle = (5.0 / 180.0) * M_PI;
// Vertical distance, in pixels, that the fake omnibox travels.
constexpr CGFloat kTravelDistance = 40;

// URL for the "Learn more" link.
const char* const kLearnMoreURL = "internal://choice-screen-learn-more";

}  // namespace

@implementation SearchEngineChoiceViewController {
  // The screen's title
  NSString* _titleString;
  // The table containing the list of search engine choices.
  SearchEngineChoiceTableViewController* _searchEngineTableViewController;
  // Button to confirm the default search engine selection.
  UIButton* _primaryButton;
  // View that contains all the UI elements above the search engine table.
  UIStackView* _topZoneStackView;
  // A fake empty omnibox illustration, shown before the user has made any
  // selection.
  FakeOmniboxView* _fakeEmptyOmniboxView;
  // A fake empty omnibox illustration, with the user's selection.
  FakeOmniboxView* _fakeOmniboxView;
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
}

- (instancetype)initWithSearchEngineTableViewController:
    (SearchEngineChoiceTableViewController*)tableViewController {
  DCHECK(tableViewController);

  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _searchEngineTableViewController = tableViewController;
  }
  return self;
}

- (void)updatePrimaryActionButton {
  UpdatePrimaryButton(_primaryButton,
                      _searchEngineTableViewController.didReachBottom,
                      self.didUserSelectARow);
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self addChildViewController:_searchEngineTableViewController];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [_searchEngineTableViewController didMoveToParentViewController:self];

  _scrollContentView = [[UIView alloc] init];
  _scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;

  _topZoneStackView = [[UIStackView alloc] init];
  [_scrollContentView addSubview:_topZoneStackView];
  _topZoneStackView.axis = UILayoutConstraintAxisVertical;
  _topZoneStackView.spacing = kTopStackViewSpacing;
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

  _fakeEmptyOmniboxView =
      [[FakeOmniboxView alloc] initWithSearchEngineName:nil faviconImage:nil];
  [_topZoneStackView addArrangedSubview:_fakeEmptyOmniboxView];
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    _fakeEmptyOmniboxView.hidden = YES;
  }
  _fakeEmptyOmniboxView.translatesAutoresizingMaskIntoConstraints = NO;

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

  UIView* searchEngineTableView = _searchEngineTableViewController.view;
  [_scrollContentView addSubview:searchEngineTableView];
  searchEngineTableView.translatesAutoresizingMaskIntoConstraints = NO;

  _scrollView = [[UIScrollView alloc] init];
  [_scrollView addSubview:_scrollContentView];
  [self.view addSubview:_scrollView];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  _separatorView = [[UIView alloc] init];
  [self.view addSubview:_separatorView];
  _separatorView.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [self.view bringSubviewToFront:_separatorView];
  _separatorView.translatesAutoresizingMaskIntoConstraints = NO;

  if (_searchEngineTableViewController.didReachBottom) {
    _primaryButton = CreateDisabledPrimaryButton();
  } else {
    _primaryButton = CreateMorePrimaryButton();
  }

  [self.view addSubview:_primaryButton];
  [_primaryButton addTarget:self
                     action:@selector(primaryButtonAction)
           forControlEvents:UIControlEventTouchUpInside];

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

    [_fakeEmptyOmniboxView.widthAnchor
        constraintEqualToConstant:kFakeOmniboxWidth],
    [_fakeEmptyOmniboxView.heightAnchor
        constraintEqualToConstant:kFakeOmniboxHeight],

    [_logoView.widthAnchor constraintEqualToConstant:kLogoSize],
    [_logoView.heightAnchor constraintEqualToConstant:kLogoSize],

    [_primaryButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kPrimaryButtonPadding],
    [_primaryButton.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                               constant:kHorizontalInsets],
    [_primaryButton.heightAnchor
        constraintEqualToConstant:kPrimaryButtonHeight],

    [_separatorView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [_separatorView.heightAnchor constraintEqualToConstant:kLineWidth],
    [_separatorView.bottomAnchor
        constraintEqualToAnchor:_primaryButton.topAnchor
                       constant:-kPrimaryButtonPadding],

    [searchEngineTableView.widthAnchor
        constraintEqualToAnchor:_scrollContentView.widthAnchor],
    [searchEngineTableView.topAnchor
        constraintEqualToAnchor:_subtitleTextView.bottomAnchor],
    [searchEngineTableView.bottomAnchor
        constraintEqualToAnchor:_scrollContentView.bottomAnchor],
    [searchEngineTableView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinimumTableHeight],

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
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Reset the title font to make sure that it is
  // properly scaled.
  _titleLabel.font = GetTitleFontWithTraitCollection(self.traitCollection);
}

#pragma mark - SearchEngineChoiceConsumer

- (void)updateFakeOmniboxWithFaviconImage:(UIImage*)icon
                         searchEngineName:(NSString*)name {
  UIView* exitingFakeOmniboxView = _fakeOmniboxView;
  _fakeOmniboxView = [[FakeOmniboxView alloc] initWithSearchEngineName:name
                                                          faviconImage:icon];
  _fakeOmniboxView.translatesAutoresizingMaskIntoConstraints = NO;
  [_topZoneStackView addSubview:_fakeOmniboxView];
  AddSameConstraints(_fakeOmniboxView, _fakeEmptyOmniboxView);
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    // If the vertical size is compact, the new fake omnibox should be added but
    // hidden (just in case the user rotate the device in portrait mode).
    // And the previous fake omnibox should be removed.
    [exitingFakeOmniboxView removeFromSuperview];
    _fakeOmniboxView.hidden = YES;
    return;
  }
  if (exitingFakeOmniboxView) {
    // Animate the exiting fake omnibox view.
    [UIView animateWithDuration:kExitAnimationDuration
        delay:0
        usingSpringWithDamping:1
        initialSpringVelocity:0
        options:UIViewAnimationCurveEaseIn
        animations:^{
          exitingFakeOmniboxView.alpha = 0;
          CGAffineTransform rotate =
              CGAffineTransformMakeRotation(kRotationAngle);
          CGAffineTransform translate =
              CGAffineTransformMakeTranslation(0, kTravelDistance);
          exitingFakeOmniboxView.transform =
              CGAffineTransformConcat(rotate, translate);
        }
        completion:^(BOOL finished) {
          [exitingFakeOmniboxView removeFromSuperview];
        }];
  }
  // Animate the entering fake omnibox view.
  CGAffineTransform rotate = CGAffineTransformMakeRotation(kRotationAngle);
  CGAffineTransform translate =
      CGAffineTransformMakeTranslation(0, kTravelDistance);
  _fakeOmniboxView.transform = CGAffineTransformConcat(rotate, translate);
  FakeOmniboxView* enteringFakeOmniboxView = _fakeOmniboxView;
  [UIView animateWithDuration:kEntranceAnimationDuration
                        delay:0
       usingSpringWithDamping:kSpringDamping
        initialSpringVelocity:0
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     enteringFakeOmniboxView.transform =
                         CGAffineTransformIdentity;
                   }
                   completion:nil];
}

#pragma mark - SearchEngineChoiceFaviconUpdateConsumer

- (void)updateFaviconImageForItem:(SnippetSearchEngineItem*)item {
  _fakeOmniboxView.faviconImage = item.faviconImage;
}

#pragma mark - Private

- (void)primaryButtonAction {
  if (_searchEngineTableViewController.didReachBottom) {
    [self.actionDelegate didTapPrimaryButton];
  } else {
    [_searchEngineTableViewController scrollToBottom];
    CGPoint bottomOffset = CGPointMake(0, _scrollView.contentSize.height -
                                              _scrollView.bounds.size.height +
                                              _scrollView.contentInset.bottom);
    [_scrollView setContentOffset:bottomOffset animated:YES];
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
      _fakeEmptyOmniboxView.alpha = 1;
      _fakeEmptyOmniboxView.hidden = NO;
      _fakeOmniboxView.alpha = 1;
      _fakeOmniboxView.hidden = NO;
      break;

    case UIUserInterfaceSizeClassCompact:
      _logoView.alpha = 0;
      _logoView.hidden = YES;
      _fakeEmptyOmniboxView.alpha = 0;
      _fakeEmptyOmniboxView.hidden = YES;
      _fakeOmniboxView.alpha = 0;
      _fakeOmniboxView.hidden = YES;

      break;

    default:
      break;
  };
}

@end
