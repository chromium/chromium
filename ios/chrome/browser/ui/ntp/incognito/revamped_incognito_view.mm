// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito/revamped_incognito_view.h"

#import "base/ios/ns_range.h"
#import "components/content_settings/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/drag_and_drop/url_drag_drop_handler.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kStackViewHorizontalMargin = 16.0;
const CGFloat kStackViewMaxWidth = 470.0;
const CGFloat kStackViewDefaultSpacing = 32.0;
const CGFloat kStackViewImageSpacing = 16.0;
const CGFloat kStackViewDescriptionsSpacing = 20.0;
const CGFloat kLayoutGuideVerticalMargin = 8.0;
const CGFloat kLayoutGuideMinHeight = 12.0;
const CGFloat kDescriptionsInnerMargin = 17.0;
const CGFloat kLearnMoreVerticalInnerMargin = 8.0;
const CGFloat kLearnMoreHorizontalInnerMargin = 16.0;

// The size of the incognito symbol image.
NSInteger kIncognitoSymbolImagePointSize = 72;

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the title of the incognito page.
UIFont* TitleFont() {
  return [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont boldSystemFontOfSize:22.0]];
}

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the subtitle of the incognito page.
UIFont* SubtitleFont() {
  return [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont boldSystemFontOfSize:15.0]];
}

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the body text of the incognito page.
UIFont* BodyFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
}

// Returns a color,that is suitable for the body text of the incognito page.
UIColor* BodyColor() {
  return [UIColor colorNamed:kGrey600Color];
}

// Returns a color,that is suitable for the backround of texts of the incognito
// page.
UIColor* TextBackgroudColor() {
  return [UIColor colorNamed:kGrey50Color];
}

// Takes an HTML string containing a bulleted list and formats it to display
// properly in a UILabel.  Removes the "<ul>" tag and replaces "<li>" with a
// bullet unicode character.
NSAttributedString* FormatHTMLListForUILabel(NSString* listString) {
  listString = [listString
      stringByReplacingOccurrencesOfString:@"\n *<ul>"
                                withString:@""
                                   options:NSRegularExpressionSearch
                                     range:NSMakeRange(0, [listString length])];
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
      stringByReplacingOccurrencesOfString:@"</li>"
                                withString:@""
                                   options:NSRegularExpressionSearch
                                     range:NSMakeRange(0, [listString length])];
  listString = [listString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  const StringWithTag parsedString =
      ParseStringWithTag(listString, @"<em>", @"</em>");

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = 1.18;

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];
  NSDictionary* attributes = @{
    NSFontAttributeName : BodyFont(),
    NSParagraphStyleAttributeName : paragraphStyle,
  };
  [attributedText addAttributes:attributes
                          range:NSMakeRange(0, attributedText.length)];
  return attributedText;
}

// Formats HTML string of learn more view to display properly in a learn more
// section. Replaces "<a>" and </a> with BEGIN_LINK and END_LINK.
NSAttributedString* FormatHTMLForLearnMoreSection() {
  UIColor* linkTextColor = [UIColor colorNamed:kBlueColor];

  NSString* learnMoreText =
      l10n_util::GetNSString(IDS_REVAMPED_INCOGNITO_NTP_LEARN_MORE);
  learnMoreText =
      [learnMoreText stringByReplacingOccurrencesOfString:@"<a>"
                                               withString:@"BEGIN_LINK"];
  learnMoreText =
      [learnMoreText stringByReplacingOccurrencesOfString:@"</a>"
                                               withString:@"END_LINK"];

  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : linkTextColor,
    NSFontAttributeName : BodyFont(),
    NSLinkAttributeName : net::NSURLWithGURL(GetLearnMoreIncognitoUrl()),
  };

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : BodyColor(),
    NSFontAttributeName : BodyFont(),
  };

  return AttributedStringFromStringWithLink(learnMoreText, textAttributes,
                                            linkAttributes);
}

}  // namespace

@interface RevampedIncognitoView () <URLDropDelegate, UITextViewDelegate>

@property(nonatomic, strong) UIView* containerView;
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, assign, getter=isFirstDescriptionAdded)
    bool firstDescriptionAdded;

@end

@implementation RevampedIncognitoView {
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

    // Container to hold and vertically position the stack view.
    self.containerView = [[UIView alloc] initWithFrame:frame];
    [self.containerView setTranslatesAutoresizingMaskIntoConstraints:NO];

    // Stackview in which all the subviews (image, labels, button) are added.
    self.stackView = [[UIStackView alloc] init];
    [self.stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    self.stackView.axis = UILayoutConstraintAxisVertical;
    self.stackView.spacing = kStackViewDefaultSpacing;
    self.stackView.distribution = UIStackViewDistributionFill;
    self.stackView.alignment = UIStackViewAlignmentCenter;
    [self.containerView addSubview:self.stackView];

    if (showTopIncognitoImageAndTitle) {
      // Incognito icon.
      UIImage* incognitoImage;
      UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kIncognitoSymbolImagePointSize
                              weight:UIImageSymbolWeightLight
                               scale:UIImageSymbolScaleMedium];
      if (@available(iOS 15, *)) {
        incognitoImage =
            SymbolWithPalette(CustomSymbolWithConfiguration(
                                  kIncognitoCircleFillSymbol, configuration),
                              LargeIncognitoPalette());
      } else {
        incognitoImage = CustomSymbolWithConfiguration(
            kIncognitoCircleFilliOS14Symbol, configuration);
      }

      UIImageView* incognitoImageView =
          [[UIImageView alloc] initWithImage:incognitoImage];
      incognitoImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
      [self.stackView addArrangedSubview:incognitoImageView];
      [self.stackView setCustomSpacing:kStackViewImageSpacing
                             afterView:incognitoImageView];
    }

    [self addTextSectionsWithTitleShown:showTopIncognitoImageAndTitle];

    // `topGuide` and `bottomGuide` exist to vertically position the stackview
    // inside the container scrollview.
    UILayoutGuide* topGuide = [[UILayoutGuide alloc] init];
    UILayoutGuide* bottomGuide = [[UILayoutGuide alloc] init];
    [self.containerView addLayoutGuide:topGuide];
    [self.containerView addLayoutGuide:bottomGuide];

    [self addSubview:self.containerView];

    [NSLayoutConstraint activateConstraints:@[
      // Position the stack view's top at some margin under from the container
      // top.
      [topGuide.topAnchor constraintEqualToAnchor:self.containerView.topAnchor],
      [self.stackView.topAnchor
          constraintEqualToAnchor:topGuide.bottomAnchor
                         constant:kLayoutGuideVerticalMargin],

      // Position the stack view's bottom guide at some margin from the
      // container bottom.
      [bottomGuide.topAnchor
          constraintEqualToAnchor:self.stackView.bottomAnchor
                         constant:kLayoutGuideVerticalMargin],

      [self.containerView.bottomAnchor
          constraintEqualToAnchor:bottomGuide.bottomAnchor],

      // Center the stackview horizontally with a minimum margin.
      [self.stackView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.containerView.leadingAnchor
                                      constant:stackViewHorizontalMargin],
      [self.stackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.containerView.trailingAnchor
                                   constant:-stackViewHorizontalMargin],
      [self.stackView.centerXAnchor
          constraintEqualToAnchor:self.containerView.centerXAnchor],

      // Ensure that the stackview width is constrained.
      [self.stackView.widthAnchor
          constraintLessThanOrEqualToConstant:stackViewMaxWidth],

      // Set a minimum top margin and make the bottom guide twice as tall as the
      // top guide.
      [topGuide.heightAnchor
          constraintGreaterThanOrEqualToConstant:kLayoutGuideMinHeight],
      [bottomGuide.heightAnchor constraintEqualToAnchor:topGuide.heightAnchor
                                             multiplier:2.0],

      // Set super view constraints
      [self.containerView.widthAnchor constraintEqualToAnchor:self.widthAnchor],
      [self.containerView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:self.heightAnchor],
    ]];

    // Constraints comunicating the size of the contentView to the scrollview.
    // See UIScrollView autolayout information at
    // https://developer.apple.com/library/ios/releasenotes/General/RN-iOSSDK-6_0/index.html
    NSDictionary* viewsDictionary = @{@"containerView" : self.containerView};
    NSArray* constraints = @[
      @"V:|-0-[containerView]-0-|",
      @"H:|-0-[containerView]-0-|",
    ];
    ApplyVisualConstraints(constraints, viewsDictionary);
  }
  return self;
}

#pragma mark - URLDropDelegate

- (BOOL)canHandleURLDropInView:(UIView*)view {
  return YES;
}

- (void)view:(UIView*)view didDropURL:(const GURL&)URL atPoint:(CGPoint)point {
  [self.URLLoaderDelegate loadURLInTab:GetLearnMoreIncognitoUrl()];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.URLLoaderDelegate loadURLInTab:GetLearnMoreIncognitoUrl()];

  // The handler is already handling the tap.
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable. Another solution is
  // to subclass `UITextView` and override `canBecomeFirstResponder` to return
  // NO, but that workaround only works on iOS 13.5+. This is the simplest
  // approach that works well on iOS 12, 13 & 14.
  textView.selectedTextRange = nil;
}

#pragma mark - Private

// Adds views containing the text of the incognito page to `self.stackView`.
- (void)addTextSectionsWithTitleShown:(BOOL)showTitle {
  if (showTitle) {
    UIColor* titleTextColor = [UIColor colorNamed:kTextPrimaryColor];

    // Title.
    UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    titleLabel.font = TitleFont();
    titleLabel.textColor = titleTextColor;
    titleLabel.numberOfLines = 0;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = l10n_util::GetNSString(IDS_REVAMPED_INCOGNITO_NTP_TITLE);
    titleLabel.adjustsFontForContentSizeCategory = YES;
    [self.stackView addArrangedSubview:titleLabel];
  }

  // Does section.
  UIView* doesSection = [self
      descriptionViewForTitle:IDS_REVAMPED_INCOGNITO_NTP_DOES_HEADER
                     subtitle:IDS_REVAMPED_INCOGNITO_NTP_DOES_DESCRIPTION];
  [self.stackView addArrangedSubview:doesSection];
  [self.stackView setCustomSpacing:kStackViewDescriptionsSpacing
                         afterView:doesSection];

  // Does not section.
  UIView* doesNotSection = [self
      descriptionViewForTitle:IDS_REVAMPED_INCOGNITO_NTP_DOES_NOT_HEADER
                     subtitle:IDS_REVAMPED_INCOGNITO_NTP_DOES_NOT_DESCRIPTION];
  [self.stackView addArrangedSubview:doesNotSection];

  // Learn more.
  UITextView* learnMore = CreateUITextViewWithTextKit1();
  learnMore.scrollEnabled = NO;
  learnMore.editable = NO;
  learnMore.delegate = self;
  learnMore.attributedText = FormatHTMLForLearnMoreSection();
  learnMore.textAlignment = NSTextAlignmentCenter;
  learnMore.layer.masksToBounds = YES;
  learnMore.layer.cornerRadius = 17;
  learnMore.backgroundColor = TextBackgroudColor();
  learnMore.textContainerInset = UIEdgeInsetsMake(
      kLearnMoreVerticalInnerMargin, kLearnMoreHorizontalInnerMargin,
      kLearnMoreVerticalInnerMargin, kLearnMoreHorizontalInnerMargin);
  [self.stackView addArrangedSubview:learnMore];

  [NSLayoutConstraint activateConstraints:@[
    [doesSection.widthAnchor
        constraintEqualToAnchor:self.stackView.widthAnchor],
    [doesNotSection.widthAnchor
        constraintEqualToAnchor:self.stackView.widthAnchor],
  ]];
}

// Returns the view for does and does not sections.
- (UIView*)descriptionViewForTitle:(int)title subtitle:(int)subtitle {
  UILabel* headerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  headerLabel.numberOfLines = 0;
  headerLabel.adjustsFontForContentSizeCategory = NO;
  headerLabel.text = l10n_util::GetNSString(title);
  headerLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  headerLabel.font = SubtitleFont();
  headerLabel.textAlignment = NSTextAlignmentNatural;

  NSAttributedString* descriptionText =
      FormatHTMLListForUILabel(l10n_util::GetNSString(subtitle));
  UILabel* descriptionLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  descriptionLabel.numberOfLines = 0;
  descriptionLabel.adjustsFontForContentSizeCategory = NO;
  descriptionLabel.attributedText = descriptionText;
  descriptionLabel.textColor = BodyColor();

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ headerLabel, descriptionLabel ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = 8;
  stackView.alignment = UIStackViewAlignmentLeading;
  stackView.backgroundColor = TextBackgroudColor();
  stackView.layer.cornerRadius = 10;
  stackView.layer.masksToBounds = true;
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kDescriptionsInnerMargin, kDescriptionsInnerMargin,
      kDescriptionsInnerMargin, kDescriptionsInnerMargin);
  return stackView;
}

@end
