// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/client_connection_view_controller.h"

#import <MaterialComponents/MDCActivityIndicator.h>
#import <MaterialComponents/MDCNavigationBar.h>
#import <MaterialComponents/MaterialButtons.h>
#import <MaterialComponents/MaterialCollections.h>
#import <MaterialComponents/MaterialSnackbar.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#import "remoting/ios/app/help_and_feedback.h"
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/pin_entry_view.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/session_reconnect_view.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/session/remoting_client.h"
#include "remoting/protocol/client_authentication_config.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kIconRadius = 30.f;
static const CGFloat kActivityIndicatorStrokeWidth = 3.f;
static const CGFloat kActivityIndicatorRadius = 33.f;

static const CGFloat kPinEntryViewWidth = 240.f;
static const CGFloat kPinEntryViewHeight = 90.f;

static const CGFloat kReconnectViewWidth = 240.f;
static const CGFloat kReconnectViewHeight = 90.f;

static const CGFloat kPadding = 20.f;
static const CGFloat kMargin = 20.f;

static const CGFloat kKeyboardAnimationTime = 0.3;

static NSString* const kConnectionErrorFeedbackContext =
    @"ConnectionErrorFeedbackContext";

using EntryPoint = remoting::ChromotingEvent::SessionEntryPoint;

@interface ClientConnectionViewController ()<PinEntryDelegate,
                                             SessionReconnectViewDelegate> {
  UIImageView* _iconView;
  MDCActivityIndicator* _activityIndicator;
  NSLayoutConstraint* _activityIndicatorTopConstraintFull;
  NSLayoutConstraint* _activityIndicatorTopConstraintKeyboard;
  UILabel* _statusLabel;
  MDCNavigationBar* _navBar;
  PinEntryView* _pinEntryView;
  SessionReconnectView* _reconnectView;
  NSString* _remoteHostName;
  RemotingClient* _client;
  SessionErrorCode _lastError;
  HostInfo* _hostInfo;
  BOOL _hasViewAppeared;
}

@property(nonatomic, assign) SessionErrorCode lastError;

@end

@implementation ClientConnectionViewController

@synthesize state = _state;
@synthesize lastError = _lastError;

- (instancetype)initWithHostInfo:(HostInfo*)hostInfo {
  self = [super init];
  if (self) {
    _hostInfo = hostInfo;
    _remoteHostName = hostInfo.hostName;
    _hasViewAppeared = NO;

    // TODO(yuweih): This logic may be reused by other views.
    UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL).uppercaseString
                  forState:UIControlStateNormal];
    [cancelButton
        setImage:[RemotingTheme
                         .backIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    [cancelButton addTarget:self
                     action:@selector(didTapCancel:)
           forControlEvents:UIControlEventTouchUpInside];
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithCustomView:cancelButton];

    _navBar = [[MDCNavigationBar alloc] initWithFrame:CGRectZero];
    [_navBar observeNavigationItem:self.navigationItem];

    [_navBar setBackgroundColor:RemotingTheme.connectionViewBackgroundColor];
    MDCNavigationBarTextColorAccessibilityMutator* mutator =
        [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
    [mutator mutate:_navBar];
    [self.view addSubview:_navBar];
    _navBar.translatesAutoresizingMaskIntoConstraints = NO;

    // Attach navBar to the top of the view.
    UILayoutGuide* layoutGuide =
        remoting::SafeAreaLayoutGuideForView(self.view);
    [NSLayoutConstraint activateConstraints:@[
      [_navBar.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
      [_navBar.leadingAnchor constraintEqualToAnchor:layoutGuide.leadingAnchor],
      [_navBar.trailingAnchor
          constraintEqualToAnchor:layoutGuide.trailingAnchor],
    ]];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = RemotingTheme.connectionViewBackgroundColor;

  _activityIndicator = [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  _activityIndicator.radius = kActivityIndicatorRadius;
  _activityIndicator.trackEnabled = YES;
  _activityIndicator.strokeWidth = kActivityIndicatorStrokeWidth;
  _activityIndicator.cycleColors =
      @[ RemotingTheme.connectionViewForegroundColor ];
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_activityIndicator];

  _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _statusLabel.numberOfLines = 1;
  _statusLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _statusLabel.textColor = RemotingTheme.connectionViewForegroundColor;
  _statusLabel.textAlignment = NSTextAlignmentCenter;
  _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_statusLabel];

  _iconView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _iconView.contentMode = UIViewContentModeCenter;
  _iconView.alpha = 0.87f;
  _iconView.backgroundColor = RemotingTheme.hostOnlineColor;
  _iconView.layer.cornerRadius = kIconRadius;
  _iconView.layer.masksToBounds = YES;
  _iconView.image = RemotingTheme.desktopIcon;
  _iconView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_iconView];

  _reconnectView = [[SessionReconnectView alloc] initWithFrame:CGRectZero];
  _reconnectView.hidden = YES;
  _reconnectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_reconnectView];
  _reconnectView.delegate = self;

  _pinEntryView = [[PinEntryView alloc] init];
  _pinEntryView.hidden = YES;
  _pinEntryView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_pinEntryView];
  _pinEntryView.delegate = self;

  [self
      initializeLayoutConstraintsWithViews:NSDictionaryOfVariableBindings(
                                               _activityIndicator, _statusLabel,
                                               _iconView, _reconnectView,
                                               _pinEntryView)];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(hostSessionStatusChanged:)
             name:kHostSessionStatusChanged
           object:nil];

  [self attemptConnectionToHostWithEntryPoint:EntryPoint::CONNECT_BUTTON];

  // Although keyboard listeners are registered here, they won't work properly
  // if the keyboard shows/hides before the view appears.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];
}

- (void)initializeLayoutConstraintsWithViews:(NSDictionary*)views {
  // Metrics to use in visual format strings.
  NSDictionary* layoutMetrics = @{
    @"padding" : @(kPadding),
    @"margin" : @(kMargin),
    @"iconDiameter" : @(kIconRadius * 2),
    @"pinEntryViewWidth" : @(kPinEntryViewWidth),
    @"pinEntryViewHeight" : @(kPinEntryViewHeight),
    @"reconnectViewWidth" : @(kReconnectViewWidth),
    @"reconnectViewHeight" : @(kReconnectViewHeight),
  };
  [_activityIndicator sizeToFit];
  NSString* f;

  // Horizontal constraints:
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"H:[_iconView(iconDiameter)]"
                                         options:0
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:|-margin-[_statusLabel]-margin-|"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:[_pinEntryView(pinEntryViewWidth)]"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:[_reconnectView(reconnectViewWidth)]"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  // Anchors:
  _activityIndicatorTopConstraintFull = [_activityIndicator.bottomAnchor
      constraintEqualToAnchor:self.view.centerYAnchor];
  _activityIndicatorTopConstraintFull.active = YES;

  [_iconView.centerYAnchor
      constraintEqualToAnchor:_activityIndicator.centerYAnchor]
      .active = YES;

  // Vertical constraints:
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"V:[_iconView(iconDiameter)]"
                                         options:0
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:
                         @"V:[_activityIndicator]-(padding)-[_statusLabel]"
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:
                         @"V:[_iconView]-(padding)-[_statusLabel]"
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  f = @"V:[_statusLabel]-(padding)-[_pinEntryView(pinEntryViewHeight)]";
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:f
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  f = @"V:[_statusLabel]-padding-[_reconnectView(reconnectViewHeight)]";
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:f
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view setNeedsUpdateConstraints];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_activityIndicator startAnimating];

  _hasViewAppeared = YES;

  self.state = _state;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [_activityIndicator stopAnimating];
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

#pragma mark - Keyboard

- (void)keyboardWillShow:(NSNotification*)notification {
  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey]
          CGRectValue]
          .size;

  CGFloat newHeight = self.view.frame.size.height - keyboardSize.height;
  CGFloat overlap = newHeight - (_pinEntryView.frame.origin.y +
                                 _pinEntryView.frame.size.height + kPadding);
  if (overlap > 0) {
    overlap = 0;
  }
  _activityIndicatorTopConstraintKeyboard.active = NO;
  _activityIndicatorTopConstraintKeyboard = [_activityIndicator.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:_activityIndicator.frame.origin.y + overlap];
  _activityIndicatorTopConstraintFull.active = NO;
  _activityIndicatorTopConstraintKeyboard.active = YES;
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     [self.view layoutIfNeeded];
                   }];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  _activityIndicatorTopConstraintKeyboard.active = NO;
  _activityIndicatorTopConstraintFull.active = YES;
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     [self.view layoutIfNeeded];
                   }];
}

#pragma mark - Properties

- (void)setState:(ClientConnectionViewState)state {
  _state = state;
  if (!_hasViewAppeared) {
    // Showing different state will re-layout the view, which will be broken if
    // the view is not shown yet.
    return;
  }
  switch (_state) {
    case ClientViewConnecting:
      [self showConnectingState];
      break;
    case ClientViewPinPrompt:
      [self showPinPromptState];
      break;
    case ClientViewConnected:
      [self showConnectedState];
      break;
    case ClientViewReconnect:
      [self showReconnect];
      break;
    case ClientViewClosed:
      [self.navigationController popToRootViewControllerAnimated:YES];
      break;
    case ClientViewError:
      [self showError];
      break;
  }
}

#pragma mark - SessionReconnectViewDelegate

- (void)didTapReconnect {
  [self attemptConnectionToHostWithEntryPoint:EntryPoint::RECONNECT_BUTTON];
}

- (void)didTapReport {
  [_client createFeedbackDataWithCallback:^(
               const remoting::FeedbackData& feedbackData) {
    [HelpAndFeedback.instance
        presentFeedbackFlowWithContext:kConnectionErrorFeedbackContext
                          feedbackData:feedbackData];
  }];
}

#pragma mark - Private

- (void)attemptConnectionToHostWithEntryPoint:(EntryPoint)entryPoint {
  _client = [[RemotingClient alloc] init];
  __weak ClientConnectionViewController* weakSelf = self;
  __weak RemotingClient* weakClient = _client;
  __weak HostInfo* weakHostInfo = _hostInfo;
  [RemotingService.instance.authentication
      callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                NSString* userEmail, NSString* accessToken) {
        if (status == RemotingAuthenticationStatusSuccess) {
          [weakClient connectToHost:weakHostInfo
                           username:userEmail
                        accessToken:accessToken
                         entryPoint:entryPoint];
        } else {
          LOG(ERROR) << "Failed to fetch access token for connectToHost. ("
                     << status << ")";
          weakSelf.lastError = SessionErrorOAuthTokenInvalid;
          weakSelf.state = ClientViewError;
        }
      }];
  self.state = ClientViewConnecting;
}

- (void)showConnectingState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [self stringWithHostNameForId:IDS_CONNECTING_TO_HOST_MESSAGE];
  [self focusOnStatusLabel];

  _pinEntryView.hidden = YES;

  _reconnectView.hidden = YES;

  _iconView.backgroundColor = RemotingTheme.hostOnlineColor;

  [_activityIndicator stopAnimating];
  _activityIndicator.cycleColors =
      @[ RemotingTheme.connectionViewForegroundColor ];
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeIndeterminate;
  _activityIndicator.hidden = NO;
  [_activityIndicator startAnimating];
}

- (void)showPinPromptState {
  _statusLabel.text = [NSString stringWithFormat:@"%@", _remoteHostName];

  _iconView.backgroundColor = RemotingTheme.hostOnlineColor;

  [_activityIndicator stopAnimating];
  _activityIndicator.hidden = YES;

  _pinEntryView.hidden = NO;

  _reconnectView.hidden = YES;

  _reconnectView.hidden = YES;

  // TODO(yuweih): This may be called before viewDidAppear and miss the keyboard
  // callback.
  [_pinEntryView becomeFirstResponder];
}

- (void)showConnectedState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [self stringWithHostNameForId:IDS_CONNECTED_TO_HOST_MESSAGE];
  [self focusOnStatusLabel];

  _pinEntryView.hidden = YES;
  [_pinEntryView clearPinEntry];

  _iconView.backgroundColor = RemotingTheme.hostOnlineColor;

  _activityIndicator.progress = 0.0;
  _activityIndicator.hidden = NO;
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeDeterminate;
  _activityIndicator.cycleColors = @[ RemotingTheme.hostOnlineColor ];
  [_activityIndicator startAnimating];
  _activityIndicator.progress = 1.0;

  _reconnectView.hidden = YES;

  _reconnectView.hidden = YES;

  HostViewController* hostViewController =
      [[HostViewController alloc] initWithClient:_client];

  [self.navigationController pushViewController:hostViewController animated:NO];
}

// TODO(yuweih): Unused. Remove this method and the ClientViewReconnect enum.
- (void)showReconnect {
  _statusLabel.text =
      [self stringWithHostNameForId:IDS_CONNECTION_CLOSED_FOR_HOST_MESSAGE];
  [self focusOnStatusLabel];

  _iconView.backgroundColor = RemotingTheme.hostErrorColor;

  [_activityIndicator stopAnimating];
  _activityIndicator.hidden = YES;

  _pinEntryView.hidden = YES;

  _reconnectView.hidden = NO;
  _reconnectView.errorText =
      l10n_util::GetNSString(IDS_MESSAGE_SESSION_FINISHED);

  [self.navigationController popToViewController:self animated:YES];
}

- (void)showError {
  // Error may happen after the session is connected. In this case we should
  // pop back to the client connection VC.
  if (self.navigationController.topViewController != self) {
    [self.navigationController popToViewController:self animated:YES];
  }

  _statusLabel.text =
      [self stringWithHostNameForId:IDS_ERROR_CONNECTING_TO_HOST_MESSAGE];

  _pinEntryView.hidden = YES;

  _iconView.backgroundColor = RemotingTheme.hostErrorColor;

  _activityIndicator.hidden = YES;

  NSString* message = nil;
  switch (_lastError) {
    case SessionErrorOk:
      // Do nothing.
      break;
    case SessionErrorPeerIsOffline:
      message = l10n_util::GetNSString(IDS_ERROR_HOST_IS_OFFLINE);
      break;
    case SessionErrorSessionRejected:
      message = l10n_util::GetNSString(IDS_ERROR_INVALID_ACCOUNT);
      break;
    case SessionErrorIncompatibleProtocol:
      message = l10n_util::GetNSString(IDS_ERROR_INCOMPATIBLE_PROTOCOL);
      break;
    case SessionErrorAuthenticationFailed:
      message = l10n_util::GetNSString(IDS_ERROR_INVALID_ACCESS_CODE);
      [_pinEntryView clearPinEntry];
      break;
    case SessionErrorInvalidAccount:
      message = l10n_util::GetNSString(IDS_ERROR_INVALID_ACCOUNT);
      break;
    case SessionErrorChannelConnectionError:
      message = l10n_util::GetNSString(IDS_ERROR_NETWORK_FAILURE);
      break;
    case SessionErrorSignalingError:
      message = l10n_util::GetNSString(IDS_ERROR_P2P_FAILURE);
      break;
    case SessionErrorSignalingTimeout:
      message = l10n_util::GetNSString(IDS_ERROR_HOST_IS_OFFLINE);
      break;
    case SessionErrorHostOverload:
      message = l10n_util::GetNSString(IDS_ERROR_HOST_OVERLOAD);
      break;
    case SessionErrorMaxSessionLength:
      message = l10n_util::GetNSString(IDS_ERROR_MAX_SESSION_LENGTH);
      break;
    case SessionErrorHostConfigurationError:
      message = l10n_util::GetNSString(IDS_ERROR_HOST_CONFIGURATION_ERROR);
      break;
    case SessionErrorUnknownError:
      message = l10n_util::GetNSString(IDS_ERROR_UNEXPECTED);
      break;
    case SessionErrorOAuthTokenInvalid:
      message = l10n_util::GetNSString(IDS_ERROR_OAUTH_TOKEN_INVALID);
      break;
    case SessionErrorThirdPartyAuthNotSupported:
      message = l10n_util::GetNSString(IDS_THIRD_PARTY_AUTH_NOT_SUPPORTED);
      break;
  }
  if (message) {
    _reconnectView.errorText = message;
  }
  _reconnectView.hidden = NO;
  remoting::SetAccessibilityFocusElement(_reconnectView);
}

- (void)didProvidePin:(NSString*)pin createPairing:(BOOL)createPairing {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostSessionPinProvided
                    object:self
                  userInfo:@{
                    kHostSessionHostName : _remoteHostName,
                    kHostSessionPin : pin,
                    kHostSessionCreatePairing : @(createPairing)
                  }];
}

- (void)didTapCancel:(id)sender {
  _client = nil;
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)hostSessionStatusChanged:(NSNotification*)notification {
  NSLog(@"hostSessionStatusChanged: %@", [notification userInfo]);
  ClientConnectionViewState state;
  ClientSessionDetails* sessionDetails =
      [[notification userInfo] objectForKey:kSessionDetails];
  switch (sessionDetails.state) {
    case SessionInitializing:
    // Same as HostConnecting in UI. Fall-though.
    case SessionAuthenticated:
    // Same as HostConnecting in UI. Fall-though.
    case SessionConnecting:
      state = ClientViewConnecting;
      break;
    case SessionPinPrompt:
      _pinEntryView.supportsPairing = [[[notification userInfo]
          objectForKey:kSessionSupportsPairing] boolValue];
      state = ClientViewPinPrompt;
      break;
    case SessionConnected:
      state = ClientViewConnected;
      break;
    case SessionFailed:
      state = ClientViewError;
      break;
    case SessionClosed:
      // If the session is closed by the host, just go back to the host list and
      // show a toast.
      state = ClientViewClosed;
      [MDCSnackbarManager.defaultManager
          showMessage:[MDCSnackbarMessage
                          messageWithText:l10n_util::GetNSString(
                                              IDS_MESSAGE_SESSION_FINISHED)]];
      break;
    default:
      LOG(ERROR) << "Unknown State for Session, " << sessionDetails.state;
      return;
  }
  _lastError = sessionDetails.error;
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    self.state = state;
  }];
}

- (NSString*)stringWithHostNameForId:(int)messageId {
  return l10n_util::GetNSStringF(messageId,
                                 base::SysNSStringToUTF16(_remoteHostName));
}

- (void)focusOnStatusLabel {
  remoting::SetAccessibilityFocusElement(_statusLabel);
}

@end
