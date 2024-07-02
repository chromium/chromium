// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view.h"

#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "components/ui_metrics/sadtab_metrics_types.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
// Layout constants.
const UIEdgeInsets kLayoutInsets = {24.0f, 24.0f, 24.0f, 24.0f};
const CGFloat kLayoutBoundsMaxWidth = 600.0f;
const CGFloat kContainerViewLandscapeTopPadding = 22.0f;
const CGFloat kTitleLabelTopPadding = 26.0f;
const CGFloat kMessageTextViewTopPadding = 16.0f;
const CGFloat kFooterLabelTopPadding = 16.0f;
const CGFloat kActionButtonHeight = 48.0f;
const CGFloat kActionButtonTopPadding = 16.0f;
// Label font sizes.
const CGFloat kTitleLabelFontSize = 23.0f;
const CGFloat kMessageTextViewFontSize = 14.0f;
const CGFloat kActionButtonFontSize = 14.0f;
const CGFloat kFooterLabelFontSize = 14.0f;
// Feedback message bullet indentation.
const CGFloat kBulletIndent = 17.0f;        // Left margin to bullet indent.
const CGFloat kBulletedTextIndent = 15.0f;  // Bullet to text indent.
// Format for bulleted line (<tab><bullet><tab><string>).
NSString* const kMessageTextViewBulletPrefix = @"\t\u2022\t";
// Separator for each new bullet line.
NSString* const kMessageTextViewBulletSuffix = @"\n";
// "<RTL Begin Indicator><NSString Token><RTL End Indicator>".
NSString* const kMessageTextViewBulletRTLFormat = @"\u202E%@\u202C";
}  // namespace

@interface SadTabView () <UITextViewDelegate> {
  UITextView* _messageTextView;
  UIButton* _actionButton;
}

// YES if the SadTab UI is displayed in Off The Record browsing mode.
@property(nonatomic, readonly, getter=isOffTheRecord) BOOL offTheRecord;
// Container view that displays all other subviews.
@property(nonatomic, readonly, strong) UIView* containerView;
// Displays the Sad Tab face.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Displays the Sad Tab title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// Displays the Sad Tab footer message (including a link to more help).
@property(nonatomic, readonly, strong) UITextView* footerLabel;
// The bounds of `containerView`, with a height updated to CGFLOAT_MAX to allow
// text to be laid out using as many lines as necessary.
@property(nonatomic, readonly) CGRect containerBounds;

// Subview layout methods.  Must be called in the following order, as subsequent
// layouts reference the values set in previous functions.
- (void)layoutImageView;
- (void)layoutTitleLabel;
- (void)layoutMessageTextView;
- (void)layoutFooterLabel;
- (void)layoutActionButton;
- (void)layoutContainerView;

// Takes an array of strings and bulletizes them into a single multi-line string
// for display. The string has NSParagraphStyle attributes for tab alignment.
+ (nonnull NSAttributedString*)bulletedAttributedStringFromStrings:
    (nonnull NSArray<NSString*>*)strings;

// Returns the appropriate title for the view, e.g. 'Aw Snap!'.
- (nonnull NSString*)titleLabelText;
// Returns the appropriate message text body for the view, this will typically
// be a larger body of explanation or help text. Returns an attributed string
// to allow for text formatting and layout to be applied to the returned string.
- (nonnull NSAttributedString*)messageTextViewAttributedText;
// Returns the full footer string containing a link, intended to be the last
// piece of text.
- (nonnull NSString*)footerLabelText;
// Returns the substring of the footer string which is to be the underlined link
// text. (May be the entire footer label string).
- (nonnull NSString*)footerLinkText;
// Returns the string to be used for the main action button.
- (nonnull NSString*)buttonText;

// The action selector for `_actionButton`.
- (void)handleActionButtonTapped;

// Returns the desired background color.
+ (UIColor*)sadTabBackgroundColor;

@end

#pragma mark - SadTabView

@implementation SadTabView

@synthesize imageView = _imageView;
@synthesize containerView = _containerView;
@synthesize titleLabel = _titleLabel;
@synthesize footerLabel = _footerLabel;

- (instancetype)initWithMode:(SadTabViewMode)mode
                offTheRecord:(BOOL)offTheRecord {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _mode = mode;
    _offTheRecord = offTheRecord;
    self.backgroundColor = [[self class] sadTabBackgroundColor];
  }
  return self;
}

#pragma mark - Text Utilities

+ (nonnull NSAttributedString*)bulletedAttributedStringFromStrings:
    (nonnull NSArray<NSString*>*)strings {
  // Ensures the bullet string is appropriately directional.
  NSString* directionalBulletPrefix =
      base::i18n::IsRTL()
          ? [NSString stringWithFormat:kMessageTextViewBulletRTLFormat,
                                       kMessageTextViewBulletPrefix]
          : kMessageTextViewBulletPrefix;

  // Assemble the strings into a single string with each line preceded by a
  // bullet point.
  NSMutableString* bulletedString = [[NSMutableString alloc] init];
  for (NSString* string in strings) {
    // If content line has been added to the bulletedString already, ensure the
    // suffix is applied, otherwise don't (e.g. don't for the first item).
    NSArray* newStringArray =
        bulletedString.length
            ? @[ kMessageTextViewBulletSuffix, directionalBulletPrefix, string ]
            : @[ directionalBulletPrefix, string ];
    [bulletedString appendString:[newStringArray componentsJoinedByString:@""]];
  }

  // Prepare a paragraph style that will allow for the alignment of lines of
  // text separately to the alignment of the bullet-points.
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.tabStops = @[
    [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentNatural
                                    location:kBulletIndent
                                     options:@{}],
    [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentNatural
                                    location:kBulletIndent + kBulletedTextIndent
                                     options:@{}]
  ];
  paragraphStyle.firstLineHeadIndent = 0.0f;
  paragraphStyle.headIndent = kBulletIndent + kBulletedTextIndent;

  // Use the paragraph style on the full string.
  NSAttributedString* bulletedAttributedString = [[NSAttributedString alloc]
      initWithString:bulletedString
          attributes:@{NSParagraphStyleAttributeName : paragraphStyle}];

  DCHECK(bulletedAttributedString);
  return bulletedAttributedString;
}

#pragma mark - Label Text

- (nonnull NSString*)titleLabelText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = l10n_util::GetNSString(IDS_SAD_TAB_TITLE);
      break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TITLE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSAttributedString*)messageTextViewAttributedText {
  NSAttributedString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = [[NSAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_SAD_TAB_MESSAGE)];
      break;
    case SadTabViewMode::FEEDBACK: {
      NSString* feedbackIntroductionString = [NSString
          stringWithFormat:@"%@\n\n",
                           l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TRY)];
      NSMutableAttributedString* feedbackString =
          [[NSMutableAttributedString alloc]
              initWithString:feedbackIntroductionString];

      NSMutableArray* stringsArray = [NSMutableArray
          arrayWithObjects:l10n_util::GetNSString(
                               IDS_SAD_TAB_RELOAD_RESTART_BROWSER),
                           l10n_util::GetNSString(
                               IDS_SAD_TAB_RELOAD_RESTART_DEVICE),
                           nil];
      if (!self.offTheRecord) {
        NSString* incognitoSuggestionString =
            l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_INCOGNITO);
        [stringsArray insertObject:incognitoSuggestionString atIndex:0];
      }

      NSAttributedString* bulletedListString =
          [[self class] bulletedAttributedStringFromStrings:stringsArray];
      [feedbackString appendAttributedString:bulletedListString];
      label = feedbackString;
      break;
    }
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)footerLabelText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD: {
      std::u16string footerLinkText(
          l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK));
      label = base::SysUTF16ToNSString(
          l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE, footerLinkText));
    } break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LEARN_MORE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)footerLinkText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD: {
      std::u16string footerLinkText(
          l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK));
      label = base::SysUTF16ToNSString(footerLinkText);
    } break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LEARN_MORE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)buttonText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LABEL);
      break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_SEND_FEEDBACK_LABEL);
      break;
  }
  DCHECK(label);
  return label;
}

#pragma mark Accessors

- (UIView*)containerView {
  if (!_containerView) {
    _containerView = [[UIView alloc] initWithFrame:CGRectZero];
    [_containerView setBackgroundColor:self.backgroundColor];
  }
  return _containerView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    UIImage* sadTabImage = [NativeImage(IDR_CRASH_SAD_TAB)
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _imageView = [[UIImageView alloc] initWithImage:sadTabImage];
    _imageView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
    [_imageView setBackgroundColor:self.backgroundColor];
  }
  return _imageView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_titleLabel setBackgroundColor:self.backgroundColor];
    [_titleLabel setText:[self titleLabelText]];
    [_titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_titleLabel setNumberOfLines:0];
    [_titleLabel setTextColor:[UIColor colorNamed:kTextPrimaryColor]];
    [_titleLabel setFont:[UIFont systemFontOfSize:kTitleLabelFontSize
                                           weight:UIFontWeightRegular]];
  }
  return _titleLabel;
}

- (UITextView*)footerLabel {
  if (!_footerLabel) {
    _footerLabel = CreateUITextViewWithTextKit1();
    _footerLabel.backgroundColor = self.backgroundColor;
    _footerLabel.delegate = self;

    // Set base text styling for footer.
    NSDictionary<NSAttributedStringKey, id>* footerAttributes = @{
      NSFontAttributeName : [UIFont systemFontOfSize:kFooterLabelFontSize
                                              weight:UIFontWeightRegular],
      NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    };
    NSMutableAttributedString* footerText =
        [[NSMutableAttributedString alloc] initWithString:[self footerLabelText]
                                               attributes:footerAttributes];

    // Add link to footer.
    NSURL* linkURL = net::NSURLWithGURL(GURL(kCrashReasonURL));
    NSDictionary<NSAttributedStringKey, id>* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSLinkAttributeName : linkURL,
    };
    NSRange linkRange = [footerText.string rangeOfString:[self footerLinkText]];
    DCHECK(linkRange.location != NSNotFound);
    DCHECK(linkRange.length > 0);
    [footerText addAttributes:linkAttributes range:linkRange];

    _footerLabel.attributedText = footerText;
  }
  return _footerLabel;
}

- (CGRect)containerBounds {
  CGFloat containerWidth = std::min(
      CGRectGetWidth(self.bounds) - kLayoutInsets.left - kLayoutInsets.right,
      kLayoutBoundsMaxWidth);
  return CGRectMake(0.0, 0.0, containerWidth, CGFLOAT_MAX);
}

#pragma mark Layout

- (void)willMoveToSuperview:(nullable UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (self.containerView.superview) {
    DCHECK_EQ(self.containerView.superview, self);
    return;
  }

  [self addSubview:self.containerView];
  [self.containerView addSubview:self.imageView];
  [self.containerView addSubview:self.titleLabel];
  [self.containerView addSubview:self.messageTextView];
  [self.containerView addSubview:self.footerLabel];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [self layoutImageView];
  [self layoutTitleLabel];
  [self layoutMessageTextView];
  [self layoutFooterLabel];
  [self layoutActionButton];
  [self layoutContainerView];
}

- (CGSize)sizeThatFits:(CGSize)size {
  return size;
}

- (void)layoutImageView {
  LayoutRect imageViewLayout = LayoutRectZero;
  imageViewLayout.boundingWidth = CGRectGetWidth(self.containerBounds);
  imageViewLayout.size = self.imageView.bounds.size;
  self.imageView.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(imageViewLayout));
}

- (void)layoutTitleLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect titleLabelLayout = LayoutRectZero;
  titleLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  titleLabelLayout.size = [self.titleLabel sizeThatFits:containerBounds.size];
  titleLabelLayout.position.originY =
      CGRectGetMaxY(self.imageView.frame) + kTitleLabelTopPadding;
  self.titleLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(titleLabelLayout));
}

- (void)layoutMessageTextView {
  CGRect containerBounds = self.containerBounds;
  LayoutRect messageTextViewLayout = LayoutRectZero;
  messageTextViewLayout.boundingWidth = CGRectGetWidth(containerBounds);
  messageTextViewLayout.size =
      [self.messageTextView sizeThatFits:containerBounds.size];
  messageTextViewLayout.position.originY =
      CGRectGetMaxY(self.titleLabel.frame) + kMessageTextViewTopPadding;
  self.messageTextView.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(messageTextViewLayout));
}

- (void)layoutFooterLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect footerLabelLayout = LayoutRectZero;
  footerLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  footerLabelLayout.size = [self.footerLabel sizeThatFits:containerBounds.size];
  footerLabelLayout.position.originY =
      CGRectGetMaxY(self.messageTextView.frame) + kFooterLabelTopPadding;
  self.footerLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(footerLabelLayout));
}

- (void)layoutActionButton {
  CGRect containerBounds = self.containerBounds;
  BOOL isIPadIdiom = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  BOOL isPortrait = IsPortrait(self.window);
  BOOL shouldAddActionButtonToContainer = isIPadIdiom || !isPortrait;
  LayoutRect actionButtonLayout = LayoutRectZero;
  actionButtonLayout.size =
      isIPadIdiom
          ? [self.actionButton sizeThatFits:CGSizeZero]
          : CGSizeMake(CGRectGetWidth(containerBounds), kActionButtonHeight);
  if (shouldAddActionButtonToContainer) {
    // Right-align actionButton and add it below helpLabel when adding it to
    // the containerView.
    if (self.actionButton.superview != self.containerView)
      [self.containerView addSubview:self.actionButton];
    actionButtonLayout.boundingWidth = CGRectGetWidth(containerBounds);
    actionButtonLayout.position = LayoutRectPositionMake(
        CGRectGetWidth(containerBounds) - actionButtonLayout.size.width,
        CGRectGetMaxY(self.footerLabel.frame) + kActionButtonTopPadding);
  } else {
    // Bottom-align the actionButton with the bounds specified by kLayoutInsets.
    if (self.actionButton.superview != self)
      [self addSubview:self.actionButton];
    actionButtonLayout.boundingWidth = CGRectGetWidth(self.bounds);
    actionButtonLayout.position = LayoutRectPositionMake(
        UIEdgeInsetsGetLeading(kLayoutInsets),
        CGRectGetMaxY(self.bounds) - kLayoutInsets.bottom -
            actionButtonLayout.size.height);
  }
  self.actionButton.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(actionButtonLayout));
}

- (void)layoutContainerView {
  UIView* bottomSubview = self.actionButton.superview == self.containerView
                              ? self.actionButton
                              : self.footerLabel;
  CGSize containerSize = CGSizeMake(CGRectGetWidth(self.containerBounds),
                                    CGRectGetMaxY(bottomSubview.frame));
  CGFloat containerOriginX =
      (CGRectGetWidth(self.bounds) - containerSize.width) / 2.0f;
  CGFloat containerOriginY = 0.0f;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Center the containerView on iPads.
    containerOriginY =
        (CGRectGetHeight(self.bounds) - containerSize.height) / 2.0f;
  } else if (IsPortrait(self.window)) {
    // Align containerView to a quarter of the view height on portrait iPhones.
    containerOriginY =
        (CGRectGetHeight(self.bounds) - containerSize.height) / 4.0f;
  } else {
    // Top-align containerView on landscape iPhones.
    containerOriginY = kContainerViewLandscapeTopPadding;
  }
  self.containerView.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake(containerOriginX, containerOriginY, containerSize.width,
                 containerSize.height));
}

#pragma mark Util

- (void)handleActionButtonTapped {
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabReloadHistogramKey,
                                ui_metrics::SadTabEvent::BUTTON_CLICKED,
                                ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
      [self.delegate sadTabViewReload:self];
      break;
    case SadTabViewMode::FEEDBACK: {
      UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabFeedbackHistogramKey,
                                ui_metrics::SadTabEvent::BUTTON_CLICKED,
                                ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
      [self.delegate sadTabViewShowReportAnIssue:self];
      break;
    }
  };
}

+ (UIColor*)sadTabBackgroundColor {
  return [UIColor colorNamed:kBackgroundColor];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(self.footerLabel == textView);
  DCHECK(URL);

  [self.delegate sadTabView:self
      showSuggestionsPageWithURL:net::GURLWithNSURL(URL)];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

@end

#pragma mark -

@implementation SadTabView (UIElements)

- (UITextView*)messageTextView {
  if (!_messageTextView) {
    _messageTextView = CreateUITextViewWithTextKit1();
    [_messageTextView setBackgroundColor:self.backgroundColor];
    [_messageTextView setAttributedText:[self messageTextViewAttributedText]];
    _messageTextView.textContainer.lineFragmentPadding = 0.0f;
    [_messageTextView setTextColor:[UIColor colorNamed:kTextSecondaryColor]];
    [_messageTextView setFont:[UIFont systemFontOfSize:kMessageTextViewFontSize
                                                weight:UIFontWeightRegular]];
    [_messageTextView setUserInteractionEnabled:NO];
  }
  return _messageTextView;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [[UIButton alloc] init];
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.background.backgroundColor = [UIColor colorNamed:kBlueColor];
    buttonConfig.background.cornerRadius = 0.0;

    buttonConfig.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
    UIFont* font = [UIFont systemFontOfSize:kActionButtonFontSize
                                     weight:UIFontWeightMedium];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSMutableAttributedString* attributedString =
        [[NSMutableAttributedString alloc]
            initWithString:[self buttonText].uppercaseString
                attributes:attributes];
    buttonConfig.attributedTitle = attributedString;

    _actionButton.configuration = buttonConfig;
    _actionButton.configurationUpdateHandler = ^(UIButton* incomingButton) {
      UIButtonConfiguration* updatedConfig = incomingButton.configuration;
      switch (incomingButton.state) {
        case UIControlStateNormal: {
          updatedConfig.background.backgroundColor =
              [UIColor colorNamed:kBlueColor];
          break;
        }
        case UIControlStateDisabled:
          updatedConfig.background.backgroundColor =
              [UIColor colorNamed:kDisabledTintColor];
          break;
        case UIControlStateSelected:
        case UIControlStateFocused:
        case UIControlStateApplication:
        case UIControlStateReserved:
          break;
      }
      incomingButton.configuration = updatedConfig;
    };

    [_actionButton addTarget:self
                      action:@selector(handleActionButtonTapped)
            forControlEvents:UIControlEventTouchUpInside];
    _actionButton.pointerInteractionEnabled = YES;
    _actionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
  }
  return _actionButton;
}

@end
