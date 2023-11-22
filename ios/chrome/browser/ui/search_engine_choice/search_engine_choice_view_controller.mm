// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Accessibility Identifier.
NSString* const kSearchEngineChoiceTitleAccessibilityIdentifier =
    @"SearchEngineChoiceTitleAccessibilityIdentifier";
// Parameters for the fake omnibox.
constexpr CGFloat kFakeOmniboxWidth = 226.;
constexpr CGFloat kFakeOmniboxHeight = 48.;
constexpr CGFloat kFakeOmniboxCornerRadius = 99.;
// Line width for the fake omnibox and the bottom separator.
constexpr CGFloat kLineWidth = 1.;
// Parameters for empty field in the fake omnibox.
constexpr CGFloat kFakeOmniboxFieldWidth = 102.;
constexpr CGFloat kFakeOmniboxFieldHeight = 12.;
constexpr CGFloat kFakeOmniboxFieldCornerRadius = 12.;
constexpr CGFloat kFakeOmniboxFieldLeadingInset = 52.;
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
// Magnifying glass size.
constexpr CGFloat kMagnifyingGlassSize = 20.;
constexpr CGFloat kMagnifyingGlassFrameSize = 24.;
constexpr CGFloat kMagnifyingGlassLeadingInset = 16.;
constexpr CGFloat kMagnifyingGlassTopInset = 12.;
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

// Helper method that creates a fake empty omnibox to diplay between the title
// and the subtitle.
UIView* CreateFakeEmptyOmnibox() {
  UIView* fake_omnibox = [[UIView alloc] init];

  fake_omnibox.bounds = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);

  // Create the dashed border line.
  CAShapeLayer* fake_omnibox_border = [CAShapeLayer layer];
  fake_omnibox_border.strokeColor = [UIColor colorNamed:kGrey300Color].CGColor;
  fake_omnibox_border.fillColor = nil;
  fake_omnibox_border.lineDashPattern = @[ @2, @1 ];
  fake_omnibox_border.frame = fake_omnibox.bounds;
  fake_omnibox_border.lineWidth = kLineWidth;
  fake_omnibox_border.path =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_border];

  // Add the empty grey field inside.
  CAShapeLayer* fake_omnibox_field = [CAShapeLayer layer];
  fake_omnibox_field.fillColor = [UIColor colorNamed:kGrey100Color].CGColor;
  BOOL isLeftToRightLayout =
      UIApplication.sharedApplication.userInterfaceLayoutDirection ==
      UIUserInterfaceLayoutDirectionLeftToRight;
  if (isLeftToRightLayout) {
    fake_omnibox_field.frame =
        CGRectMake(kFakeOmniboxFieldLeadingInset,
                   (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                   kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
  } else {
    fake_omnibox_field.frame =
        CGRectMake(kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset -
                       kFakeOmniboxFieldWidth,
                   (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                   kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
  }
  fake_omnibox_field.path =
      [UIBezierPath
          bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxFieldWidth,
                                               kFakeOmniboxFieldHeight)
                       cornerRadius:kFakeOmniboxFieldCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_field];

  // Add the search icon to the side.
  UIImageView* searchSymbolIcon = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kMagnifyingglassSymbol,
                                               kMagnifyingGlassSize)];

  [fake_omnibox addSubview:searchSymbolIcon];
  if (isLeftToRightLayout) {
    searchSymbolIcon.frame =
        CGRectMake(kMagnifyingGlassLeadingInset, kMagnifyingGlassTopInset,
                   kMagnifyingGlassFrameSize, kMagnifyingGlassFrameSize);
  } else {
    searchSymbolIcon.frame =
        CGRectMake(kFakeOmniboxWidth - kMagnifyingGlassLeadingInset -
                       kMagnifyingGlassFrameSize,
                   kMagnifyingGlassTopInset, kMagnifyingGlassFrameSize,
                   kMagnifyingGlassFrameSize);
  }
  fake_omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  return fake_omnibox;
}

// Helper method that creates a fake omnibox with the given incon and search
// engine name to diplay between the title and the subtitle.
UIView* CreateFakeOmnibox(UIImageView* icon, NSString* searchEngineName) {
  UIView* fake_omnibox = [[UIView alloc] init];

  fake_omnibox.bounds = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);

  // Add the shadow around the omnibox.
  CAShapeLayer* fake_omnibox_shadow = [CAShapeLayer layer];
  fake_omnibox_shadow.frame = fake_omnibox.bounds;
  fake_omnibox_shadow.shadowColor = [UIColor colorNamed:kGrey300Color].CGColor;
  fake_omnibox_shadow.shadowOpacity = 1;
  fake_omnibox_shadow.shadowRadius = 16;
  fake_omnibox_shadow.shadowOffset = CGSizeMake(0, 4);
  fake_omnibox_shadow.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_shadow];

  // Create the pill-shaped field.
  CAShapeLayer* fake_omnibox_pill = [CAShapeLayer layer];
  fake_omnibox_pill.fillColor = [UIColor colorNamed:kBackgroundColor].CGColor;
  fake_omnibox_pill.frame = fake_omnibox.bounds;
  fake_omnibox_pill.path =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_pill];
  BOOL isLeftToRightLayout =
      UIApplication.sharedApplication.userInterfaceLayoutDirection ==
      UIUserInterfaceLayoutDirectionLeftToRight;
  // Add the search engine Label.
  UILabel* searchWithLabel = [[UILabel alloc] init];
  if (isLeftToRightLayout) {
    searchWithLabel.frame = CGRectMake(
        kFakeOmniboxFieldLeadingInset, 0.,
        kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset, kFakeOmniboxHeight);
  } else {
    searchWithLabel.frame =
        CGRectMake(0., 0., kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset,
                   kFakeOmniboxHeight);
  }

  searchWithLabel.text =
      l10n_util::GetNSStringF(IDS_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_TEXT,
                              base::SysNSStringToUTF16(searchEngineName));
  searchWithLabel.font = [UIFont systemFontOfSize:13];
  searchWithLabel.numberOfLines = 0;
  [fake_omnibox addSubview:searchWithLabel];

  // Add the favicon on the side.
  [fake_omnibox addSubview:icon];
  if (isLeftToRightLayout) {
    icon.frame =
        CGRectMake(kMagnifyingGlassLeadingInset, kMagnifyingGlassTopInset,
                   kMagnifyingGlassFrameSize, kMagnifyingGlassFrameSize);

  } else {
    icon.frame = CGRectMake(kFakeOmniboxWidth - kMagnifyingGlassLeadingInset -
                                kMagnifyingGlassFrameSize,
                            kMagnifyingGlassTopInset, kMagnifyingGlassFrameSize,
                            kMagnifyingGlassFrameSize);
  }

  fake_omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  return fake_omnibox;
}

UIFont* GetTitleFontWithTraitCollection(UITraitCollection* trait_collection) {
  BOOL dynamic_type_enabled = UIContentSizeCategoryIsAccessibilityCategory(
      trait_collection.preferredContentSizeCategory);

  UIFontTextStyle text_style;
  if (!dynamic_type_enabled) {
    if (IsRegularXRegularSizeClass(trait_collection)) {
      text_style = UIFontTextStyleTitle1;
    } else if (!IsSmallDevice()) {
      text_style = UIFontTextStyleLargeTitle;
    }
  } else {
    text_style = UIFontTextStyleTitle2;
  }

  UIFontDescriptor* descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:text_style];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* font_metrics = [UIFontMetrics metricsForTextStyle:text_style];
  return [font_metrics scaledFontForFont:font];
}

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
  UIView* _fakeEmptyOmniboxView;
  // A fake empty omnibox illustration, with the user's selection.
  UIView* _fakeOmniboxView;
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

- (void)enablePrimaryButton {
  UIButtonConfiguration* buttonConfiguration = _primaryButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  _primaryButton.configuration = buttonConfiguration;
  _primaryButton.enabled = YES;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self addChildViewController:_searchEngineTableViewController];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
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

  _fakeEmptyOmniboxView = CreateFakeEmptyOmnibox();
  [_topZoneStackView addArrangedSubview:_fakeEmptyOmniboxView];
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    _fakeEmptyOmniboxView.hidden = YES;
  }

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
  _separatorView.backgroundColor = [UIColor colorNamed:kGrey300Color];
  [self.view bringSubviewToFront:_separatorView];
  _separatorView.translatesAutoresizingMaskIntoConstraints = NO;

  _primaryButton = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  [self.view addSubview:_primaryButton];
  SetConfigurationTitle(
      _primaryButton,
      l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE));
  SetConfigurationFont(
      _primaryButton,
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]);
  _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_primaryButton addTarget:self
                     action:@selector(primaryButtonAction)
           forControlEvents:UIControlEventTouchUpInside];

  UIButtonConfiguration* buttonConfiguration = _primaryButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kDisabledTintColor];
  buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  _primaryButton.configuration = buttonConfiguration;
  _primaryButton.enabled = NO;

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

#pragma mark - SearchEngineChoiceConsumer

- (void)updateFakeOmniboxWithFavicon:(UIImageView*)icon
                    SearchEngineName:(NSString*)name {
  CGRect startingFrame = _fakeEmptyOmniboxView.frame;
  startingFrame.origin.y += kTravelDistance;
  CGRect endFrame = _fakeEmptyOmniboxView.frame;
  UIView* existingFakeOmniboxView = _fakeOmniboxView;
  if (existingFakeOmniboxView) {
    [UIView animateWithDuration:kExitAnimationDuration
        delay:0
        usingSpringWithDamping:1
        initialSpringVelocity:0
        options:UIViewAnimationCurveEaseIn
        animations:^{
          existingFakeOmniboxView.alpha = 0;
          existingFakeOmniboxView.transform =
              CGAffineTransformMakeRotation(kRotationAngle);
          existingFakeOmniboxView.frame = startingFrame;
        }
        completion:^(BOOL finished) {
          [existingFakeOmniboxView removeFromSuperview];
        }];
  }
  // No need to add a new fake omnibox when it is hidden.
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    return;
  }

  UIView* newFakeOmniboxView = CreateFakeOmnibox(icon, name);
  [_topZoneStackView addSubview:newFakeOmniboxView];
  newFakeOmniboxView.frame = startingFrame;
  newFakeOmniboxView.transform = CGAffineTransformMakeRotation(kRotationAngle);

  [UIView animateWithDuration:kEntranceAnimationDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0
      options:UIViewAnimationCurveEaseOut
      animations:^{
        newFakeOmniboxView.transform = CGAffineTransformIdentity;
        newFakeOmniboxView.frame = endFrame;
      }
      completion:^(BOOL finished) {
        self->_fakeOmniboxView = newFakeOmniboxView;
      }];
}

#pragma mark - Private

- (void)primaryButtonAction {
  [self.actionDelegate didTapPrimaryButton];
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
