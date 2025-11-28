// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_container_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_table_view_controller.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_infobar_metrics_recorder.h"
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

@interface InfobarPasswordContainerViewController () <
    InfobarPasswordTableViewControllerContainerDelegate>
@end

@implementation InfobarPasswordContainerViewController {
  // The table view with the password details.
  InfobarPasswordTableViewController* _passwordViewController;
  // The scroll view containing the content.
  UIScrollView* _scrollView;
  // InfobarPasswordModalDelegate for this ViewController.
  __weak id<InfobarPasswordModalDelegate> _infobarModalDelegate;
  // Used to build and record metrics.
  InfobarMetricsRecorder* _metricsRecorder;
  // Used to build and record metrics specific to passwords.
  IOSChromePasswordInfobarMetricsRecorder* _passwordMetricsRecorder;
  // Whether the current set of credentials has already been saved.
  BOOL _currentCredentialsSaved;
  // The "accept" button.
  ChromeButton* _acceptButton;
  NSString* _acceptButtonText;
  // The "cancel" button.
  ChromeButton* _cancelButton;
  NSString* _cancelButtonText;
  // The stack containing the buttons.
  UIStackView* _buttonStack;
}

- (instancetype)initWithDelegate:(id<InfobarPasswordModalDelegate>)modalDelegate
                            type:(InfobarType)infobarType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _metricsRecorder =
        [[InfobarMetricsRecorder alloc] initWithType:infobarType];
    _infobarModalDelegate = modalDelegate;
    switch (infobarType) {
      case InfobarType::kInfobarTypePasswordUpdate:
        _passwordMetricsRecorder =
            [[IOSChromePasswordInfobarMetricsRecorder alloc]
                initWithType:PasswordInfobarType::kPasswordInfobarTypeUpdate];
        break;
      case InfobarType::kInfobarTypePasswordSave:
        _passwordMetricsRecorder =
            [[IOSChromePasswordInfobarMetricsRecorder alloc]
                initWithType:PasswordInfobarType::kPasswordInfobarTypeSave];
        break;
      default:
        NOTREACHED();
    }
    _passwordViewController = [[InfobarPasswordTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
    _passwordViewController.containerDelegate = self;
    _passwordViewController.passwordMetricsRecorder = _passwordMetricsRecorder;
  }
  return self;
}

- (id<InfobarPasswordModalConsumer>)passwordConsumer {
  return _passwordViewController;
}

- (void)loadView {
  _scrollView = [[UIScrollView alloc] init];
  self.view = _scrollView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;

  UIImage* gearImage = DefaultSymbolWithPointSize(kSettingsFilledSymbol,
                                                  kInfobarSymbolPointSize);
  UIBarButtonItem* settingsButton = [[UIBarButtonItem alloc]
      initWithImage:gearImage
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(presentPasswordSettings)];
  settingsButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_MODAL_PASSWORD_SETTINGS_HINT);
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _buttonStack = [[UIStackView alloc] init];
  _buttonStack.axis = UILayoutConstraintAxisVertical;
  _buttonStack.alignment = UIStackViewAlignmentFill;
  _buttonStack.spacing = kButtonStackSpacing;
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;

  _acceptButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _acceptButton.title = [_acceptButtonText copy];
  _acceptButton.enabled = !_currentCredentialsSaved;
  [_acceptButton addTarget:self
                    action:@selector(saveCredentialsButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
  [_buttonStack addArrangedSubview:_acceptButton];

  if (_cancelButtonText.length > 0) {
    _cancelButton =
        [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
    _cancelButton.title = [_cancelButtonText copy];
    [_cancelButton addTarget:self
                      action:@selector(neverSaveCredentialsForCurrentSite)
            forControlEvents:UIControlEventTouchUpInside];
    [_buttonStack addArrangedSubview:_cancelButton];
  }

  [view addSubview:_buttonStack];

  UIView* passwordView = _passwordViewController.view;
  _passwordViewController.view.translatesAutoresizingMaskIntoConstraints = NO;

  [self addChildViewController:_passwordViewController];
  [view addSubview:passwordView];
  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor constraintEqualToAnchor:passwordView.leadingAnchor],
    [view.topAnchor constraintEqualToAnchor:passwordView.topAnchor],
    [view.trailingAnchor constraintEqualToAnchor:passwordView.trailingAnchor],
    [view.widthAnchor constraintEqualToAnchor:passwordView.widthAnchor],

    [passwordView.bottomAnchor constraintEqualToAnchor:_buttonStack.topAnchor],

    [view.centerXAnchor constraintEqualToAnchor:_buttonStack.centerXAnchor],
    [view.widthAnchor constraintEqualToAnchor:_buttonStack.widthAnchor
                                     constant:2 * kButtonStackHorizontalMargin],
    [view.bottomAnchor constraintEqualToAnchor:_buttonStack.bottomAnchor
                                      constant:kButtonStackVerticalMargin],
  ]];
  [_passwordViewController didMoveToParentViewController:self];
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
  [_infobarModalDelegate modalInfobarWasDismissed:self];
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
}

#pragma mark - InfobarPasswordTableViewControllerContainerDelegate

- (void)setAcceptButtonText:(NSString*)acceptButtonText {
  _acceptButtonText = [acceptButtonText copy];
}

- (void)setCancelButtonText:(NSString*)cancelButtonText {
  _cancelButtonText = [cancelButtonText copy];
}

- (void)setCurrentCredentialsSaved:(BOOL)currentCredentialsSaved {
  _currentCredentialsSaved = currentCredentialsSaved;
}

- (void)updateAcceptButtonEnabled:(BOOL)enabled title:(NSString*)title {
  _acceptButton.enabled = enabled;
  _acceptButtonText = title;
  _acceptButton.title = title;
}

#pragma mark - Private

// Dismisses the modal.
- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [_infobarModalDelegate dismissInfobarModal:self];
}

// Presents the password settings.
- (void)presentPasswordSettings {
  base::RecordAction(base::UserMetricsAction("MobileMessagesModalSettings"));
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::SettingsOpened];
  [_infobarModalDelegate presentPasswordSettings];
}

// Called to not save the password.
- (void)neverSaveCredentialsForCurrentSite {
  base::RecordAction(base::UserMetricsAction("MobileMessagesModalNever"));
  [_passwordMetricsRecorder
      recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                             TappedNeverForThisSite];
  [_infobarModalDelegate neverSaveCredentialsForCurrentSite];
}

// Called to save the password.
- (void)saveCredentialsButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  if ([_acceptButton.title
          isEqualToString:l10n_util::GetNSString(
                              IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]) {
    [_passwordMetricsRecorder
        recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                               SavedCredentials];
  } else {
    [_passwordMetricsRecorder
        recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                               UpdatedCredentials];
  }
  [_infobarModalDelegate
      updateCredentialsWithUsername:_passwordViewController.username
                           password:_passwordViewController.unmaskedPassword];
}

@end
