// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_enhanced_protection_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The size of the symbols.
const CGFloat kSymbolSize = 20;
// The margin in the stack view.
const CGFloat kMargin = 16;
// Spacing for the stack view.
const CGFloat kStackViewSpacing = 24;
// Margin for the text in the detail items.
const CGFloat kTextMargin = 44;
// Margin for the center of the image in the detail items.
const CGFloat kImageCenterMargin = 14;

}  // namespace

@interface SafeBrowsingEnhancedProtectionViewController () <UITextViewDelegate>

@end

@implementation SafeBrowsingEnhancedProtectionViewController {
  UIStackView* _stackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.title =
      l10n_util::GetNSString(IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE);

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismiss)];
  self.navigationItem.rightBarButtonItem = doneButton;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.contentInset = UIEdgeInsetsMake(kMargin, 0, kMargin, 0);
  scrollView.accessibilityIdentifier =
      kSafeBrowsingEnhancedProtectionScrollViewId;
  [self.view addSubview:scrollView];

  _stackView = [[UIStackView alloc] init];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.spacing = kStackViewSpacing;
  [scrollView addSubview:_stackView];

  AddSameConstraints(scrollView, self.view);

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.centerXAnchor constraintEqualToAnchor:scrollView.centerXAnchor],
    [_stackView.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor
                                           constant:-2 * kMargin],
    [_stackView.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [_stackView.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
  ]];

  [self addContentToStackView];
}

#pragma mark - Private

// Adds all the content to the _stackView.
- (void)addContentToStackView {
  // First section.
  [self addHeaderWithTextID:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_WHEN_ON_HEADER
                toStackView:_stackView];

  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DATA_ICON_DESCRIPTION
                        image:DefaultSymbolWithPointSize(kChartBarXAxisSymbol,
                                                         kSymbolSize)
                  toStackView:_stackView];
  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DOWNLOAD_ICON_DESCRIPTION
                        image:DefaultSymbolWithPointSize(kSaveImageActionSymbol,
                                                         kSymbolSize)
                  toStackView:_stackView];
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* gIcon = CustomSymbolWithPointSize(kGoogleShieldSymbol, kSymbolSize);
#else
  UIImage* gIcon = DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolSize);
#endif
  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_G_ICON_DESCRIPTION
                        image:gIcon
                  toStackView:_stackView];
  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_GLOBE_ICON_DESCRIPTION
                        image:DefaultSymbolWithPointSize(kGlobeAmericasSymbol,
                                                         kSymbolSize)
                  toStackView:_stackView];
  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_KEY_ICON_DESCRIPTION
                        image:CustomSymbolWithPointSize(kPasswordSymbol,
                                                        kSymbolSize)
                  toStackView:_stackView];

  // Second section.
  [self addHeaderWithTextID:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_THINGS_TO_CONSIDER_HEADER
                toStackView:_stackView];

  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_LINK_ICON_DESCRIPTION
                        image:DefaultSymbolWithPointSize(kLinkActionSymbol,
                                                         kSymbolSize)
                  toStackView:_stackView];
  [self addDetailItemWithText:
            IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_ACCOUNT_ICON_DESCRIPTION
                        image:DefaultSymbolWithPointSize(
                                  kPersonCropCircleSymbol, kSymbolSize)
                  toStackView:_stackView];
  [self
      addDetailItemWithText:
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_PERFORMANCE_ICON_DESCRIPTION
                      image:DefaultSymbolWithPointSize(kSpeedometerSymbol,
                                                       kSymbolSize)
                toStackView:_stackView];

  // Footer.
  [self addFooterToStackView:_stackView];
}

// Adds a header with `textID` to `stackView`.
- (void)addHeaderWithTextID:(NSInteger)textID
                toStackView:(UIStackView*)stackView {
  UILabel* header = [[UILabel alloc] init];
  header.adjustsFontForContentSizeCategory = YES;
  header.text = l10n_util::GetNSString(textID);
  header.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];

  [stackView addArrangedSubview:header];
}

// Adds a detail item with `textID` and `image` to `stackView`.
- (void)addDetailItemWithText:(NSInteger)textID
                        image:(UIImage*)image
                  toStackView:(UIStackView*)stackView {
  UIView* detailItem = [[UIView alloc] init];

  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.tintColor = [UIColor colorNamed:kGrey600Color];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.text = l10n_util::GetNSString(textID);
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];

  [detailItem addSubview:imageView];
  [detailItem addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [detailItem.topAnchor constraintEqualToAnchor:label.topAnchor],
    [detailItem.topAnchor constraintEqualToAnchor:imageView.topAnchor],
    [label.leadingAnchor constraintEqualToAnchor:detailItem.leadingAnchor
                                        constant:kTextMargin],
    [label.trailingAnchor constraintEqualToAnchor:detailItem.trailingAnchor],
    [label.bottomAnchor
        constraintLessThanOrEqualToAnchor:detailItem.bottomAnchor],
    [imageView.bottomAnchor
        constraintLessThanOrEqualToAnchor:detailItem.bottomAnchor],
    [imageView.centerXAnchor constraintEqualToAnchor:detailItem.leadingAnchor
                                            constant:kImageCenterMargin],
  ]];

  [stackView addArrangedSubview:detailItem];
}

// Adds the footer to `stackView`.
- (void)addFooterToStackView:(UIStackView*)stackView {
  UITextView* footerTextView = [[UITextView alloc] init];
  footerTextView.backgroundColor = UIColor.clearColor;
  footerTextView.editable = NO;
  footerTextView.scrollEnabled = NO;
  footerTextView.adjustsFontForContentSizeCategory = YES;
  footerTextView.textContainer.lineFragmentPadding = 0;
  footerTextView.textContainerInset = UIEdgeInsetsZero;
  footerTextView.delegate = self;

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_FOOTER);

  StringWithTags parsedString = ParseStringWithLinks(text);

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];

  GURL url(kEnhancedSafeBrowsingLearnMoreURL);
  CHECK_EQ(parsedString.ranges.size(), 1ul);
  [attributedText addAttribute:NSLinkAttributeName
                         value:net::NSURLWithGURL(url)
                         range:parsedString.ranges[0]];

  footerTextView.attributedText = attributedText;

  footerTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  [stackView addArrangedSubview:footerTextView];
}

// Removes the view as a result of pressing "Done" button.
- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  NSURL* URL = textItem.link;
  DCHECK(URL);
  GURL gURL = net::GURLWithNSURL(URL);
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:gURL];
    [weakSelf.applicationHandler closePresentedViewsAndOpenURL:command];
  }];
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        safeBrowsingEnhancedProtectionViewControllerDidRemove:self];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction(
      "IOSSafeBrowsingEnhancedProtectionSettingsCloseWithSwipe"));
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
