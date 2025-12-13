// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_learn_more_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat constexpr kSymbolImagePointSize = 24.;
CGFloat constexpr kMainStackSpacing = 24.0;
CGFloat constexpr kHorizontalMargin = 16;
CGFloat constexpr kInnerStackTopSpacing = 4;
CGFloat constexpr kInnerStackTextSpacing = 8;
CGFloat constexpr kInnerStackHorizontalSpacing = 16;
}  // namespace

@implementation ManagedProfileLearnMoreViewController {
  NSString* _userEmail;
  NSString* _hostedDomain;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.title = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_TITLE);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonAction:)];
  self.navigationItem.rightBarButtonItem = doneButton;

  UIView* content = [self createContentView];
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  [scrollView addSubview:content];

  AddSameConstraints(self.view, scrollView);
  AddSameConstraintsWithInsets(
      content, scrollView,
      NSDirectionalEdgeInsetsMake(kMainStackSpacing, 0, 0, 0));

  [NSLayoutConstraint activateConstraints:@[
    [content.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor
                                        constant:-2 * kHorizontalMargin],
    [content.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

#pragma mark - Actions

- (void)doneButtonAction:(id)sender {
  [self.presentationDelegate dismissManagedProfileLearnMoreViewController:self];
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
  [self.presentationDelegate dismissManagedProfileLearnMoreViewController:self];
  // Request the coordinator to be stopped here
}

#pragma mark - Private

// Returns the main content view for this view.
- (UIView*)createContentView {
  UIStackView* stack = [[UIStackView alloc] init];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = kMainStackSpacing;

  UILabel* information = [self createLabel];
  information.text = l10n_util::GetNSStringF(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_HEADER,
      base::SysNSStringToUTF16(_userEmail),
      base::SysNSStringToUTF16(_hostedDomain));
  information.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [stack addArrangedSubview:information];

  UIView* browserInfo = [self
      createViewWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_BROWSER_INFORMATION_TITLE)
                 subtitle:
                     l10n_util::GetNSString(
                         IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_BROWSER_INFORMATION_SUBTITLE)
                   symbol:kCheckmarkShieldSymbol];
  [stack addArrangedSubview:browserInfo];

  UIView* deviceInfo = [self
      createViewWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_DEVICE_INFORMATION_TITLE)
                 subtitle:
                     l10n_util::GetNSString(
                         IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_DEVICE_INFORMATION_SUBTITLE)
                   symbol:kIPhoneSymbol];
  [stack addArrangedSubview:deviceInfo];

  stack.translatesAutoresizingMaskIntoConstraints = NO;

  return stack;
}

// Returns a view with `title`, `subtitle` and `symbol`.
- (UIView*)createViewWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                        symbol:(NSString*)symbol {
  UILabel* titleLabel = [self createLabel];
  titleLabel.text = title;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  UILabel* subtitleLabel = [self createLabel];
  subtitleLabel.text = subtitle;
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, subtitleLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.alignment = UIStackViewAlignmentLeading;
  textStack.spacing = kInnerStackTextSpacing;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  UIImage* symbolImage = MakeSymbolMonochrome(
      DefaultSymbolWithConfiguration(symbol, configuration));
  UIImageView* symbolView = [[UIImageView alloc] initWithImage:symbolImage];
  symbolView.tintColor = [UIColor colorNamed:kGrey600Color];
  symbolView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* container = [[UIView alloc] init];
  [container addSubview:textStack];
  [container addSubview:symbolView];

  [NSLayoutConstraint activateConstraints:@[
    [symbolView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [symbolView.topAnchor constraintEqualToAnchor:container.topAnchor],
    [symbolView.heightAnchor
        constraintLessThanOrEqualToAnchor:container.heightAnchor],

    [textStack.leadingAnchor
        constraintEqualToAnchor:symbolView.trailingAnchor
                       constant:kInnerStackHorizontalSpacing],
    [textStack.topAnchor constraintEqualToAnchor:container.topAnchor
                                        constant:kInnerStackTopSpacing],
    [textStack.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [textStack.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
  ]];

  return container;
}

// Returns a label to be used in this view.
- (UILabel*)createLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 0;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

@end
