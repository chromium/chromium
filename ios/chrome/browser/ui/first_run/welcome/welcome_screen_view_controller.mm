// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/tos_commands.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kDefaultMargin = 16;
constexpr CGFloat kEnterpriseIconBorderWidth = 1;
constexpr CGFloat kEnterpriseIconCornerRadius = 7.0;
constexpr CGFloat kEnterpriseIconContainerLength = 30;

// URL for the terms of service text.
NSString* const kTermsOfServiceURL = @"internal://terms-of-service";

// URL for the terms of service text.
NSString* const kManageMetricsReportedURL = @"internal://uma-manager";

NSString* const kEnterpriseIconImageName = @"enterprise_icon";

}  // namespace

@interface WelcomeScreenViewController () <UITextViewDelegate>

@property(nonatomic, strong) UITextView* footerTextView;
@property(nonatomic, weak) id<TOSCommands> TOSHandler;

@end

@implementation WelcomeScreenViewController

@dynamic delegate;
@synthesize isManaged = _isManaged;

- (instancetype)initWithTOSHandler:(id<TOSCommands>)TOSHandler {
  DCHECK(TOSHandler);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _TOSHandler = TOSHandler;
  }
  return self;
}

- (void)viewDidLoad {
  [self configureLabels];
  self.view.accessibilityIdentifier =
      first_run::kFirstRunWelcomeScreenAccessibilityIdentifier;
  self.bannerName = @"welcome_screen_banner";
  self.isTallBanner = YES;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON);

  self.footerTextView = [self createFooterTextView];
  [self.specificContentView addSubview:self.footerTextView];

  [NSLayoutConstraint activateConstraints:@[
    [self.footerTextView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.footerTextView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    [self.footerTextView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
  ]];

  if (self.isManaged) {
    UILabel* managedLabel = [self createManagedLabel];
    UIView* managedIcon = [self createManagedIcon];
    [self.specificContentView addSubview:managedLabel];
    [self.specificContentView addSubview:managedIcon];

    [NSLayoutConstraint activateConstraints:@[
      [managedLabel.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [managedLabel.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
      [managedLabel.widthAnchor
          constraintLessThanOrEqualToAnchor:self.specificContentView
                                                .widthAnchor],

      [managedIcon.topAnchor constraintEqualToAnchor:managedLabel.bottomAnchor
                                            constant:kDefaultMargin],
      [managedIcon.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    ]];

    // Put the footer below the header in the content area with a margin, when
    // the header is not empty.
    [self.footerTextView.topAnchor
        constraintGreaterThanOrEqualToAnchor:managedIcon.bottomAnchor
                                    constant:kDefaultMargin]
        .active = YES;
  } else {
    // Put the footer at the top of the content area when there is no header.
    [self.footerTextView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView.topAnchor]
        .active = YES;
  }
  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.titleLabel);
  [self.delegate logScrollButtonVisible:!self.didReachBottom
                 withUMACheckboxVisible:false];
}

#pragma mark - Private

// Configures the text for the title and subtitle based on whether the browser
// is managed or not.
- (void)configureLabels {
  if (self.isManaged) {
    self.titleText = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_ENTERPRISE);
    self.subtitleText = l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE_ENTERPRISE);
  } else {
    self.titleText =
        (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
            ? l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
            : l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
    self.subtitleText =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
  }
}

// Creates and configures the label for the disclaimer that the browser is
// managed.
- (UILabel*)createManagedLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  label.numberOfLines = 0;
  label.text = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_MANAGED);
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Creates and configures the icon indicating that the browser is managed.
- (UIView*)createManagedIcon {
  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.layer.cornerRadius = kEnterpriseIconCornerRadius;
  iconContainer.layer.borderWidth = kEnterpriseIconBorderWidth;
  iconContainer.layer.borderColor = [UIColor colorNamed:kGrey200Color].CGColor;

  UIImage* image = [[UIImage imageNamed:kEnterpriseIconImageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.tintColor = [UIColor colorNamed:kGrey500Color];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  [iconContainer addSubview:imageView];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor
        constraintEqualToConstant:kEnterpriseIconContainerLength],
    [iconContainer.heightAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor],
    [imageView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [imageView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
  ]];

  return iconContainer;
}

// Creates and configures the text view for the terms of service, with a
// formatted link to the full text of the terms of service.
- (UITextView*)createFooterTextView {
  NSAttributedString* termsOfServiceString =
      [self createFooterAttributedStringWithString:
                l10n_util::GetNSString(
                    IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE)
                                        linkString:kTermsOfServiceURL];
  NSAttributedString* endOfLine =
      [self createFooterAttributedStringWithString:@"\n" linkString:nil];
  NSAttributedString* manageMetricsReported =
      [self createFooterAttributedStringWithString:
                l10n_util::GetNSString(
                    IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING)
                                        linkString:kManageMetricsReportedURL];
  NSMutableAttributedString* footerString =
      [[NSMutableAttributedString alloc] init];
  [footerString appendAttributedString:termsOfServiceString];
  [footerString appendAttributedString:endOfLine];
  [footerString appendAttributedString:manageMetricsReported];

  UITextView* textView = CreateUITextViewWithTextKit1();
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.delegate = self;
  textView.backgroundColor = UIColor.clearColor;
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.attributedText = footerString;

  return textView;
}

// Returns a NSAttributedString using a `string` and `linkString`.
// `linkString` should be nil, if `string` doesn't contain any link.
- (NSAttributedString*)createFooterAttributedStringWithString:(NSString*)string
                                                   linkString:
                                                       (NSString*)linkString {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  if (!linkString) {
    return [[NSAttributedString alloc] initWithString:string
                                           attributes:textAttributes];
  }
  NSDictionary* linkAttributes =
      @{NSLinkAttributeName : [NSURL URLWithString:linkString]};
  return AttributedStringFromStringWithLink(string, textAttributes,
                                            linkAttributes);
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(textView == self.footerTextView);
  NSString* URLString = URL.absoluteString;
  if (URLString == kTermsOfServiceURL) {
    [self.TOSHandler showTOSPage];
  } else if (URLString == kManageMetricsReportedURL) {
    [self.delegate showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }

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

@end
