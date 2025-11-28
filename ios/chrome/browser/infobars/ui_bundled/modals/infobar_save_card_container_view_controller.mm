// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_container_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kButtonStackSpacing = 8;
constexpr CGFloat kButtonStackHorizontalMargin = 16;
constexpr CGFloat kButtonStackVerticalMargin = 9;
}  // namespace

@interface InfobarSaveCardContainerViewController () <
    InfobarSaveCardTableViewControllerContainerDelegate>
@end

@implementation InfobarSaveCardContainerViewController {
  // The table view containing the card info.
  InfobarSaveCardTableViewController* _saveCardTableViewController;
  // The scroll view containing the content.
  UIScrollView* _scrollView;
  // InfobarSaveCardModalDelegate for this ViewController.
  __weak id<InfobarSaveCardModalDelegate> _saveCardModalDelegate;
  // Used to build and record metrics.
  InfobarMetricsRecorder* _metricsRecorder;
  // The "save" button.
  ChromeButton* _saveButton;
  // The stack containing the buttons.
  UIStackView* _buttonStack;
  // Whether the saveButton should be enabled.
  BOOL _saveButtonEnabled;
}

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveCardModalDelegate>)modalDelegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveCard];
    _saveCardModalDelegate = modalDelegate;
    _saveCardTableViewController = [[InfobarSaveCardTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
    _saveCardTableViewController.containerDelegate = self;
  }
  return self;
}

- (id<InfobarSaveCardModalConsumer>)saveCardConsumer {
  return _saveCardTableViewController;
}

- (void)loadView {
  _scrollView = [[UIScrollView alloc] init];
  self.view = _scrollView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure the NavigationBar.
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD_CLOSE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(dismissInfobarModal)];
  closeButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = closeButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _buttonStack = [[UIStackView alloc] init];
  _buttonStack.axis = UILayoutConstraintAxisVertical;
  _buttonStack.alignment = UIStackViewAlignmentFill;
  _buttonStack.spacing = kButtonStackSpacing;
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;

  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _saveButton.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  _saveButton.enabled = _saveButtonEnabled;
  [_saveButton addTarget:self
                  action:@selector(saveCardButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  [_buttonStack addArrangedSubview:_saveButton];

  [view addSubview:_buttonStack];

  UIView* saveCardView = _saveCardTableViewController.view;
  _saveCardTableViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;

  [self addChildViewController:_saveCardTableViewController];
  [view addSubview:saveCardView];
  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor constraintEqualToAnchor:saveCardView.leadingAnchor],
    [view.topAnchor constraintEqualToAnchor:saveCardView.topAnchor],
    [view.trailingAnchor constraintEqualToAnchor:saveCardView.trailingAnchor],
    [view.widthAnchor constraintEqualToAnchor:saveCardView.widthAnchor],

    [saveCardView.bottomAnchor constraintEqualToAnchor:_buttonStack.topAnchor],

    [view.centerXAnchor constraintEqualToAnchor:_buttonStack.centerXAnchor],
    [view.widthAnchor constraintEqualToAnchor:_buttonStack.widthAnchor
                                     constant:2 * kButtonStackHorizontalMargin],
    [view.bottomAnchor constraintEqualToAnchor:_buttonStack.bottomAnchor
                                      constant:kButtonStackVerticalMargin],
  ]];
  [_saveCardTableViewController didMoveToParentViewController:self];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  CGFloat scrollableHeight = _scrollView.contentSize.height +
                             _scrollView.adjustedContentInset.top +
                             _scrollView.adjustedContentInset.bottom;
  CGFloat viewHeight = self.view.frame.size.height;
  // Make sure there is at least one point of scroll to avoid rounding errors.
  _scrollView.bounces = scrollableHeight - 1 > viewHeight;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [_saveCardModalDelegate modalInfobarWasDismissed:self];
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
}

#pragma mark - InfobarSaveCardTableViewControllerContainerDelegate

- (void)updateSaveButtonEnabled:(BOOL)enabled {
  _saveButton.enabled = enabled;
  _saveButtonEnabled = enabled;
}

- (void)showProgressWithUploadCompleted:(BOOL)uploadCompleted {
  _saveButton.enabled = NO;
  _saveButton.title = @"";
  _saveButton.tunedDownStyle = YES;
  _saveButton.primaryButtonImage =
      uploadCompleted ? PrimaryButtonImageCheckmark : PrimaryButtonImageSpinner;
  if (uploadCompleted) {
    UIAccessibilityPostNotification(
        UIAccessibilityAnnouncementNotification,
        l10n_util::GetNSString(
            IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
  }

  _saveButton.accessibilityLabel =
      uploadCompleted
          ? l10n_util::GetNSString(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME)
          : l10n_util::GetNSString(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_LOADING_THROBBER_ACCESSIBLE_NAME);
}

- (void)dismissModalAndOpenURL:(const GURL&)URL {
  [_saveCardModalDelegate dismissModalAndOpenURL:URL];
}

#pragma mark - Private

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [_saveCardModalDelegate dismissInfobarModal:self];
}

- (void)saveCardButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];

  NSString* cardholderName =
      [_saveCardTableViewController currentCardholderName];
  NSString* expirationMonth =
      [_saveCardTableViewController currentExpirationMonth];
  NSString* expirationYear =
      [_saveCardTableViewController currentExpirationYear];
  NSString* cardCVC = [_saveCardTableViewController currentCardCVC];

  [_saveCardModalDelegate saveCardWithCardholderName:cardholderName
                                     expirationMonth:expirationMonth
                                      expirationYear:expirationYear
                                             cardCvc:cardCVC];
}

@end
