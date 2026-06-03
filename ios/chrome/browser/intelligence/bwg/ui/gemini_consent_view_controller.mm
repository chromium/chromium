// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackSpacing = 16.0;

// Header traits.
const CGFloat kHeaderIconContainerCornerRadius = 16.0;
const CGFloat kHeaderIconContainerWidthMultiplier = 0.14;
const CGFloat kHeaderIconSizeMultiplier = 0.55;

}  // namespace

@interface GeminiConsentViewController () <UITextViewDelegate,
                                           GeminiConsentAccordionViewDelegate>
@end

@implementation GeminiConsentViewController {
  // Main stack view. This view itself does not scroll.
  UIStackView* _mainStackView;
  // The view data for this view controller.
  GeminiConsentConfiguration* _configuration;
}

- (instancetype)initWithConfiguration:
    (GeminiConsentConfiguration*)configuration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self configureMainStackView];
}

#pragma mark - GeminiFREViewControllerProtocol

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Configures the main stack view with the accordion, header and footnotes.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];
  AddSameConstraints(_mainStackView, self.view);

  if (_configuration.header) {
    [_mainStackView addArrangedSubview:[self createHeaderView]];
  }

  [_mainStackView addArrangedSubview:[self createAccordionView]];

  if (_configuration.footnote) {
    [_mainStackView addArrangedSubview:[self createFootnoteView]];
  }
}

// Creates the header using the header configuration.
- (UIView*)createHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] init];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing = kMainStackSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

  [headerView addSubview:verticalStack];
  AddSameConstraintsWithInsets(verticalStack, headerView,
                               NSDirectionalEdgeInsetsZero);

  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  iconContainer.layer.cornerRadius = kHeaderIconContainerCornerRadius;

  [verticalStack addArrangedSubview:iconContainer];
  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor
        constraintEqualToAnchor:headerView.widthAnchor
                     multiplier:kHeaderIconContainerWidthMultiplier],
    [iconContainer.heightAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor]
  ]];

  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.image = _configuration.header.icon;
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.translatesAutoresizingMaskIntoConstraints = NO;

  [iconContainer addSubview:iconView];
  [NSLayoutConstraint activateConstraints:@[
    [iconView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [iconView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
    [iconView.widthAnchor constraintEqualToAnchor:iconContainer.widthAnchor
                                       multiplier:kHeaderIconSizeMultiplier],
    [iconView.heightAnchor constraintEqualToAnchor:iconContainer.heightAnchor
                                        multiplier:kHeaderIconSizeMultiplier]
  ]];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.attributedText = _configuration.header.title;
  [verticalStack addArrangedSubview:titleLabel];

  return headerView;
}

// Creates the accordion view using GeminiConsentAccordionView.
- (UIView*)createAccordionView {
  GeminiConsentAccordionView* accordionView =
      [[GeminiConsentAccordionView alloc]
          initWithRows:_configuration.rows
           collapsible:_configuration.collapsible];
  accordionView.delegate = self;
  accordionView.translatesAutoresizingMaskIntoConstraints = NO;

  return accordionView;
}

// Creates the footnote view.
- (UITextView*)createFootnoteView {
  UITextView* footNoteTextView = [[UITextView alloc] init];
  footNoteTextView.backgroundColor = [UIColor clearColor];
  footNoteTextView.scrollEnabled = NO;
  footNoteTextView.editable = NO;
  footNoteTextView.textDragInteraction.enabled = NO;
  footNoteTextView.delegate = self;
  footNoteTextView.textContainerInset = UIEdgeInsetsZero;
  footNoteTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]};
  footNoteTextView.attributedText = _configuration.footnote;
  footNoteTextView.accessibilityIdentifier =
      kGeminiFootNoteTextViewAccessibilityIdentifier;

  return footNoteTextView;
}

#pragma mark - UITextViewDelegate

// Helper to handle link actions.
- (void)handleLinkAction:(NSString*)actionString {
  RecordFREConsentAction(IOSGeminiFREAction::kLinkClick);
  if ([actionString isEqualToString:kGeminiFirstFootnoteLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kFirstFootnoteLinkURL)];
  } else if ([actionString isEqualToString:kGeminiSecondFootnoteLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kSecondFootnoteLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiSecondBoxLinkActionManagedAccount]) {
    [self.mutator openNewTabWithURL:GURL(kSecondBoxLinkURLManagedAccount)];
  } else if ([actionString isEqualToString:
                               kGeminiSecondBoxLink1ActionNonManagedAccount]) {
    [self.mutator openNewTabWithURL:GURL(kSecondBoxLink1URLNonManagedAccount)];
  } else if ([actionString isEqualToString:
                               kGeminiSecondBoxLink2ActionNonManagedAccount]) {
    [self.mutator openNewTabWithURL:GURL(kSecondBoxLink2URLNonManagedAccount)];
  } else if ([actionString
                 isEqualToString:kGeminiLivePrivacyNoticeLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kLivePrivacyNoticeLinkURL)];
  } else if ([actionString isEqualToString:kGeminiLiveLearnMoreLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kLiveLearnMoreLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiLivePrivacyPolicyLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kLivePrivacyPolicyLinkURL)];
  } else if ([actionString isEqualToString:kGeminiKoreanTermsLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kKoreanTermsFootnoteLinkURL)];
  } else if ([actionString isEqualToString:kGeminiWatchLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kWatchLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiDataGovernanceManagedLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kDataGovernanceManagedLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiDataGovernanceStrictLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kDataGovernanceStrictLinkURL)];
  } else if ([actionString isEqualToString:
                               kGeminiDataGovernanceNormalChoicesLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kDataGovernanceNormalChoicesLinkURL)];
  } else if ([actionString isEqualToString:
                               kGeminiDataGovernanceNormalLocationLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kDataGovernanceNormalLocationLinkURL)];
  } else if ([actionString
                 isEqualToString:kGeminiConnectedServicesLinkAction]) {
    [self.mutator openNewTabWithURL:GURL(kConnectedServicesLinkURL)];
  }
}

// Handles tap on UITextView.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return nil;
  }

  NSString* actionString = textItem.link.absoluteString;
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf handleLinkAction:actionString];
  }];
}

// If the text item is a link, return nil to prevent the long-press context menu
// from appearing.
- (UIMenu*)textView:(UITextView*)textView
    menuConfigurationForTextItem:(UITextItem*)textItem
                     defaultMenu:(UIMenu*)defaultMenu {
  if (textItem.link) {
    return nil;
  }
  return defaultMenu;
}

#pragma mark - GeminiConsentAccordionViewDelegate

- (void)accordionView:(GeminiConsentAccordionView*)view didTapLink:(NSURL*)url {
  [self handleLinkAction:url.absoluteString];
}

- (void)accordionView:(GeminiConsentAccordionView*)view
         didToggleRow:(GeminiConsentRow*)row {
  [self.delegate consentViewControllerDidExpandAccordionItem:self];
}

@end
