// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"

#import "base/ios/ns_range.h"
#import "components/content_settings/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

const CGFloat kStackViewHorizontalMargin = 20.0;
const CGFloat kStackViewMaxWidth = 416.0;
const CGFloat kStackViewDefaultSpacing = 20.0;
const CGFloat kStackViewImageSpacing = 22.0;
const CGFloat kLayoutGuideVerticalMargin = 8.0;
const CGFloat kLayoutGuideMinHeight = 12.0;

// The size of the incognito symbol image.
NSInteger kIncognitoSymbolImagePointSize = 72;

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the title of the incognito page.
UIFont* TitleFont() {
  return [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont boldSystemFontOfSize:26.0]];
}

// Returns the color to use for body text.
UIColor* BodyTextColor() {
  return [UIColor colorNamed:kTextSecondaryColor];
}

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the body text of the incognito page.
UIFont* BodyFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
}

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for bolded text in the body of the incognito page.
UIFont* BoldBodyFont() {
  UIFontDescriptor* baseDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* styleDescriptor = [baseDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  // Use a `size` of 0.0 to use the default size for the descriptor.
  return [UIFont fontWithDescriptor:styleDescriptor size:0.0];
}

// Takes an HTML string containing a bulleted list and formats it to display
// properly in a UILabel.  Removes the "<ul>" tag and replaces "<li>" with a
// bullet unicode character.
NSAttributedString* FormatHTMLListForUILabel(NSString* listString) {
  listString = [listString stringByReplacingOccurrencesOfString:@"<ul>"
                                                     withString:@""];
  listString = [listString stringByReplacingOccurrencesOfString:@"</ul>"
                                                     withString:@""];

  // Use a regular expression to find and remove all leading whitespace from the
  // lines which contain the "<li>" tag.  This un-indents the bulleted lines.
  listString = [listString
      stringByReplacingOccurrencesOfString:@"\n *<li>"
                                withString:@"\n\u2022  "
                                   options:NSRegularExpressionSearch
                                     range:NSMakeRange(0, [listString length])];
  listString = [listString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  const StringWithTag parsedString =
      ParseStringWithTag(listString, @"<em>", @"</em>");

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];
  [attributedText addAttribute:NSFontAttributeName
                         value:BodyFont()
                         range:NSMakeRange(0, attributedText.length)];
  if (parsedString.range != NSMakeRange(NSNotFound, 0)) {
    [attributedText addAttribute:NSFontAttributeName
                           value:BoldBodyFont()
                           range:parsedString.range];
  }
  return attributedText;
}

}  // namespace

@interface IncognitoView () <URLDropDelegate>

@end

@implementation IncognitoView {
  UIView* _containerView;
  UIStackView* _stackView;
  UILabel* _notSavedLabel;
  UILabel* _visibleDataLabel;

  // Layout Guide whose height is the height of the bottom unsafe area.
  UILayoutGuide* _bottomUnsafeAreaGuide;
  UILayoutGuide* _bottomUnsafeAreaGuideInSuperview;

  // Height constraint for adding margins for the bottom toolbar.
  NSLayoutConstraint* _bottomToolbarMarginHeight;

  // Constraint ensuring that `containerView` is at least as high as the
  // superview of the IncognitoNTPView, i.e. the Incognito panel.
  // This ensures that if the Incognito panel is higher than a compact
  // `containerView`, the `containerView`'s `topGuide` and `bottomGuide` are
  // forced to expand, centering the views in between them.
  NSArray<NSLayoutConstraint*>* _superViewConstraints;

  // Handles drop interactions for this view.
  URLDragDropHandler* _dragDropHandler;
}

- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame
      showTopIncognitoImageAndTitle:YES
          stackViewHorizontalMargin:kStackViewHorizontalMargin
                  stackViewMaxWidth:kStackViewMaxWidth];
}

- (instancetype)initWithFrame:(CGRect)frame
    showTopIncognitoImageAndTitle:(BOOL)showTopIncognitoImageAndTitle
        stackViewHorizontalMargin:(CGFloat)stackViewHorizontalMargin
                stackViewMaxWidth:(CGFloat)stackViewMaxWidth {
  self = [super initWithFrame:frame];
  if (self) {
    _dragDropHandler = [[URLDragDropHandler alloc] init];
    _dragDropHandler.dropDelegate = self;
    [self addInteraction:[[UIDropInteraction alloc]
                             initWithDelegate:_dragDropHandler]];

    self.alwaysBounceVertical = YES;
    // The bottom safe area is taken care of with the bottomUnsafeArea guides.
    self.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;

    // Container to hold and vertically position the stack view.
    _containerView = [[UIView alloc] initWithFrame:frame];
    [_containerView setTranslatesAutoresizingMaskIntoConstraints:NO];

    // Stackview in which all the subviews (image, labels, button) are added.
    _stackView = [[UIStackView alloc] init];
    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.spacing = kStackViewDefaultSpacing;
    _stackView.distribution = UIStackViewDistributionFill;
    _stackView.alignment = UIStackViewAlignmentCenter;
    [_containerView addSubview:_stackView];

    if (showTopIncognitoImageAndTitle) {
      // Incognito image.
      UIImage* incognitoImage;
      UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kIncognitoSymbolImagePointSize
                              weight:UIImageSymbolWeightLight
                               scale:UIImageSymbolScaleMedium];
      incognitoImage =
          SymbolWithPalette(CustomSymbolWithConfiguration(
                                kIncognitoCircleFillSymbol, configuration),
                            LargeIncognitoPalette());

      UIImageView* incognitoImageView =
          [[UIImageView alloc] initWithImage:incognitoImage];
      incognitoImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
      [_stackView addArrangedSubview:incognitoImageView];
      [_stackView setCustomSpacing:kStackViewImageSpacing
                         afterView:incognitoImageView];
    }

    [self addTextSectionsWithTitleShown:showTopIncognitoImageAndTitle];

    // `topGuide` and `bottomGuide` exist to vertically position the stackview
    // inside the container scrollview.
    UILayoutGuide* topGuide = [[UILayoutGuide alloc] init];
    UILayoutGuide* bottomGuide = [[UILayoutGuide alloc] init];
    _bottomUnsafeAreaGuide = [[UILayoutGuide alloc] init];
    [_containerView addLayoutGuide:topGuide];
    [_containerView addLayoutGuide:bottomGuide];
    [_containerView addLayoutGuide:_bottomUnsafeAreaGuide];

    // This layout guide is used to prevent the content from being displayed
    // below the bottom toolbar.
    UILayoutGuide* bottomToolbarMarginGuide = [[UILayoutGuide alloc] init];
    [_containerView addLayoutGuide:bottomToolbarMarginGuide];

    _bottomToolbarMarginHeight =
        [bottomToolbarMarginGuide.heightAnchor constraintEqualToConstant:0];
    // Updates the constraints to the correct value.
    [self updateToolbarMargins];

    [self addSubview:_containerView];

    [NSLayoutConstraint activateConstraints:@[
      // Position the stack view's top at some margin under from the container
      // top.
      [topGuide.topAnchor constraintEqualToAnchor:_containerView.topAnchor],
      [_stackView.topAnchor constraintEqualToAnchor:topGuide.bottomAnchor
                                           constant:kLayoutGuideVerticalMargin],

      // Position the stack view's bottom guide at some margin from the
      // container bottom.
      [bottomGuide.topAnchor
          constraintEqualToAnchor:bottomToolbarMarginGuide.bottomAnchor
                         constant:kLayoutGuideVerticalMargin],
      [_containerView.bottomAnchor
          constraintEqualToAnchor:bottomGuide.bottomAnchor],

      // Position the stack view above the bottom toolbar margin guide.
      [bottomToolbarMarginGuide.topAnchor
          constraintEqualToAnchor:_stackView.bottomAnchor],

      // Center the stackview horizontally with a minimum margin.
      [_stackView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_containerView.leadingAnchor
                                      constant:stackViewHorizontalMargin],
      [_stackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:_containerView.trailingAnchor
                                   constant:-stackViewHorizontalMargin],
      [_stackView.centerXAnchor
          constraintEqualToAnchor:_containerView.centerXAnchor],

      // Constraint the _bottomUnsafeAreaGuide to the stack view and the
      // container view. Its height is set in the -didMoveToSuperview to take
      // into account the unsafe area.
      [_bottomUnsafeAreaGuide.topAnchor
          constraintEqualToAnchor:_stackView.bottomAnchor
                         constant:2 * kLayoutGuideMinHeight +
                                  kLayoutGuideVerticalMargin],
      [_bottomUnsafeAreaGuide.bottomAnchor
          constraintEqualToAnchor:_containerView.bottomAnchor],

      // Ensure that the stackview width is constrained.
      [_stackView.widthAnchor
          constraintLessThanOrEqualToConstant:stackViewMaxWidth],

      // Activate the height constraints.
      _bottomToolbarMarginHeight,

      // Set a minimum top margin and make the bottom guide twice as tall as the
      // top guide.
      [topGuide.heightAnchor
          constraintGreaterThanOrEqualToConstant:kLayoutGuideMinHeight],
      [bottomGuide.heightAnchor constraintEqualToAnchor:topGuide.heightAnchor
                                             multiplier:2.0],
    ]];

    // Constraints comunicating the size of the contentView to the scrollview.
    // See UIScrollView autolayout information at
    // https://developer.apple.com/library/ios/releasenotes/General/RN-iOSSDK-6_0/index.html
    NSDictionary* viewsDictionary = @{@"containerView" : _containerView};
    NSArray* constraints = @[
      @"V:|-0-[containerView]-0-|",
      @"H:|-0-[containerView]-0-|",
    ];
    ApplyVisualConstraints(constraints, viewsDictionary);

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self ]);
      [self registerForTraitChanges:traits
                         withAction:@selector(updateToolbarMargins)];
    }
  }
  return self;
}

#pragma mark - UIView overrides

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (!self.superview) {
    return;
  }

  id<LayoutGuideProvider> safeAreaGuide = self.superview.safeAreaLayoutGuide;
  _bottomUnsafeAreaGuideInSuperview = [[UILayoutGuide alloc] init];
  [self.superview addLayoutGuide:_bottomUnsafeAreaGuideInSuperview];

  _superViewConstraints = @[
    [safeAreaGuide.bottomAnchor
        constraintEqualToAnchor:_bottomUnsafeAreaGuideInSuperview.topAnchor],
    [self.superview.bottomAnchor
        constraintEqualToAnchor:_bottomUnsafeAreaGuideInSuperview.bottomAnchor],
    [_bottomUnsafeAreaGuide.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_bottomUnsafeAreaGuideInSuperview
                                                 .heightAnchor],
    [_containerView.widthAnchor
        constraintEqualToAnchor:self.superview.widthAnchor],
    [_containerView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.superview.heightAnchor],
  ];

  [NSLayoutConstraint activateConstraints:_superViewConstraints];
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [NSLayoutConstraint deactivateConstraints:_superViewConstraints];
  [self.superview removeLayoutGuide:_bottomUnsafeAreaGuideInSuperview];
  [super willMoveToSuperview:newSuperview];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateToolbarMargins];
}
#endif

- (void)safeAreaInsetsDidChange {
  [super safeAreaInsetsDidChange];
  [self updateToolbarMargins];
}

#pragma mark - Notifications

- (void)contentSizeCategoryDidChange {
  UIColor* bodyTextColor = BodyTextColor();

  // Recompute the text for `_notSavedLabel` and `_visibleDataLabel`, as these
  // two include font information in their attributedText.
  _notSavedLabel.attributedText = FormatHTMLListForUILabel(
      l10n_util::GetNSString(IDS_NEW_TAB_OTR_NOT_SAVED));
  _notSavedLabel.textColor = bodyTextColor;
  _visibleDataLabel.attributedText =
      FormatHTMLListForUILabel(l10n_util::GetNSString(IDS_NEW_TAB_OTR_VISIBLE));
  _visibleDataLabel.textColor = bodyTextColor;
}

#pragma mark - URLDropDelegate

- (BOOL)canHandleURLDropInView:(UIView*)view {
  return YES;
}

- (void)view:(UIView*)view didDropURL:(const GURL&)URL atPoint:(CGPoint)point {
  [self.URLLoaderDelegate loadURLInTab:URL];
}

#pragma mark - Private

// Updates the height of the margins for the top and bottom toolbars.
- (void)updateToolbarMargins {
  if (IsSplitToolbarMode(self)) {
    _bottomToolbarMarginHeight.constant = kSecondaryToolbarWithoutOmniboxHeight;
  } else {
    _bottomToolbarMarginHeight.constant = 0;
  }
}

// Triggers a navigation to the help page.
- (void)learnMoreButtonPressed {
  [self.URLLoaderDelegate loadURLInTab:GetLearnMoreIncognitoUrl()];
}

// Adds views containing the text of the incognito page to `_stackView`.
- (void)addTextSectionsWithTitleShown:(BOOL)showTitle {
  UIColor* bodyTextColor = BodyTextColor();
  UIColor* linkTextColor = [UIColor colorNamed:kBlueColor];

  // Title.
  if (showTitle) {
    UIColor* titleTextColor = [UIColor colorNamed:kTextPrimaryColor];
    UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    titleLabel.font = TitleFont();
    titleLabel.textColor = titleTextColor;
    titleLabel.numberOfLines = 0;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = l10n_util::GetNSString(IDS_NEW_TAB_OTR_TITLE);
    titleLabel.adjustsFontForContentSizeCategory = YES;
    [_stackView addArrangedSubview:titleLabel];
  }

  // The Subtitle and Learn More link have no vertical spacing between them,
  // so they are embedded in a separate stack view.
  UILabel* subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  subtitleLabel.font = BodyFont();
  subtitleLabel.textColor = bodyTextColor;
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.text =
      l10n_util::GetNSString(IDS_NEW_TAB_OTR_SUBTITLE_WITH_READING_LIST);
  subtitleLabel.adjustsFontForContentSizeCategory = YES;

  UIButton* learnMoreButton = [UIButton buttonWithType:UIButtonTypeCustom];
  learnMoreButton.accessibilityTraits = UIAccessibilityTraitLink;
  learnMoreButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_LEARN_MORE_HINT);
  [learnMoreButton
      setTitle:l10n_util::GetNSString(IDS_NEW_TAB_OTR_LEARN_MORE_LINK)
      forState:UIControlStateNormal];
  [learnMoreButton setTitleColor:linkTextColor forState:UIControlStateNormal];
  learnMoreButton.titleLabel.font = BodyFont();
  learnMoreButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [learnMoreButton addTarget:self
                      action:@selector(learnMoreButtonPressed)
            forControlEvents:UIControlEventTouchUpInside];
  // TODO(crbug.com/40128314): Style as a link rather than a button.
  learnMoreButton.pointerInteractionEnabled = YES;

  UIStackView* subtitleStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ subtitleLabel, learnMoreButton ]];
  subtitleStackView.axis = UILayoutConstraintAxisVertical;
  subtitleStackView.spacing = 0;
  subtitleStackView.distribution = UIStackViewDistributionFill;
  subtitleStackView.alignment = UIStackViewAlignmentLeading;
  [_stackView addArrangedSubview:subtitleStackView];

  // Text explaining what data that is not saved. This label uses an attributed
  // string, so it must be manually adjusted when Dynamic Type settings are
  // changed.
  NSAttributedString* notSavedText = FormatHTMLListForUILabel(
      l10n_util::GetNSString(IDS_NEW_TAB_OTR_NOT_SAVED));
  _notSavedLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _notSavedLabel.numberOfLines = 0;
  _notSavedLabel.adjustsFontForContentSizeCategory = NO;
  _notSavedLabel.attributedText = notSavedText;
  _notSavedLabel.textColor = bodyTextColor;
  [_stackView addArrangedSubview:_notSavedLabel];

  // Text explaining what data might still be visible. This label uses an
  // attributed string, so it must be manually adjusted when Dynamic Type
  // settings are changed.
  NSAttributedString* visibleDataText =
      FormatHTMLListForUILabel(l10n_util::GetNSString(IDS_NEW_TAB_OTR_VISIBLE));
  _visibleDataLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _visibleDataLabel.numberOfLines = 0;
  _visibleDataLabel.adjustsFontForContentSizeCategory = NO;
  _visibleDataLabel.attributedText = visibleDataText;
  _visibleDataLabel.textColor = bodyTextColor;
  [_stackView addArrangedSubview:_visibleDataLabel];

  // `_notSavedLabel` and `visibleDataLabel` should have the same width as
  // `subtitleStackView`, even if they can be constrained narrower.
  [NSLayoutConstraint activateConstraints:@[
    [_notSavedLabel.widthAnchor
        constraintEqualToAnchor:subtitleStackView.widthAnchor],
    [_visibleDataLabel.widthAnchor
        constraintEqualToAnchor:subtitleStackView.widthAnchor],
  ]];

  // Register for notifications when the Dynamic Type setting is changed.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(contentSizeCategoryDidChange)
             name:UIContentSizeCategoryDidChangeNotification
           object:nil];
}

@end
