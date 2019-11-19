// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/session/remoting_client.h"

#include <memory>

#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/audio/audio_playback_sink_ios.h"
#import "remoting/ios/display/gl_display_handler.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/persistence/host_pairing_info.h"
#import "remoting/ios/persistence/remoting_preferences.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/client/audio/audio_playback_stream.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/client/gesture_interpreter.h"
#include "remoting/client/input/keyboard_interpreter.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#include "remoting/ios/session/remoting_client_session_delegate.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/video_renderer.h"

NSString* const kHostSessionStatusChanged = @"kHostSessionStatusChanged";
NSString* const kHostSessionPinProvided = @"kHostSessionPinProvided";

NSString* const kSessionDetails = @"kSessionDetails";
NSString* const kSessionSupportsPairing = @"kSessionSupportsPairing";
NSString* const kSessonStateErrorCode = @"kSessonStateErrorCode";

NSString* const kHostSessionCreatePairing = @"kHostSessionCreatePairing";
NSString* const kHostSessionHostName = @"kHostSessionHostName";
NSString* const kHostSessionPin = @"kHostSessionPin";

static std::string GetCurrentUserId() {
  return base::SysNSStringToUTF8(
      RemotingService.instance.authentication.user.userId);
}

// Block doesn't work with rvalue passing. This function helps exposing the data
// to the block.
static void ResolveFeedbackDataCallback(
    void (^callback)(const remoting::FeedbackData&),
    std::unique_ptr<remoting::FeedbackData> data) {
  DCHECK(remoting::ChromotingClientRuntime::GetInstance()
             ->ui_task_runner()
             ->BelongsToCurrentThread());
  remoting::FeedbackData* raw_data = data.get();
  callback(*raw_data);
}

@interface RemotingClient () {
  remoting::ChromotingClientRuntime* _runtime;
  std::unique_ptr<remoting::RemotingClientSessonDelegate> _sessonDelegate;
  ClientSessionDetails* _sessionDetails;
  remoting::protocol::SecretFetchedCallback _secretFetchedCallback;
  remoting::GestureInterpreter _gestureInterpreter;
  remoting::KeyboardInterpreter _keyboardInterpreter;

  // _session is valid only when the session is connected.
  std::unique_ptr<remoting::ChromotingSession> _session;
}
@end

@implementation RemotingClient

@synthesize displayHandler = _displayHandler;

- (instancetype)init {
  self = [super init];
  if (self) {
    _runtime = remoting::ChromotingClientRuntime::GetInstance();
    _sessonDelegate.reset(new remoting::RemotingClientSessonDelegate(self));
    _sessionDetails = [[ClientSessionDetails alloc] init];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(hostSessionPinProvided:)
               name:kHostSessionPinProvided
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [self disconnectFromHost];
}

- (void)connectToHost:(HostInfo*)hostInfo
             username:(NSString*)username
          accessToken:(NSString*)accessToken
           entryPoint:(remoting::ChromotingEvent::SessionEntryPoint)entryPoint {
  DCHECK(_runtime->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(hostInfo);
  DCHECK(hostInfo.hostId);
  DCHECK(hostInfo.publicKey);

  _sessionDetails.hostInfo = hostInfo;

  remoting::ConnectToHostInfo info;
  info.username = base::SysNSStringToUTF8(username);
  info.auth_token = base::SysNSStringToUTF8(accessToken);
  info.host_jid = base::SysNSStringToUTF8(hostInfo.jabberId);
  info.host_ftl_id = base::SysNSStringToUTF8(hostInfo.ftlId);
  info.host_id = base::SysNSStringToUTF8(hostInfo.hostId);
  info.host_pubkey = base::SysNSStringToUTF8(hostInfo.publicKey);
  info.host_os = base::SysNSStringToUTF8(hostInfo.hostOs);
  info.host_os_version = base::SysNSStringToUTF8(hostInfo.hostOsVersion);
  info.host_version = base::SysNSStringToUTF8(hostInfo.hostVersion);

  remoting::HostPairingInfo pairing = remoting::HostPairingInfo::GetPairingInfo(
      GetCurrentUserId(), info.host_id);
  info.pairing_id = pairing.pairing_id();
  info.pairing_secret = pairing.pairing_secret();

  info.session_entry_point = entryPoint;

  info.capabilities = "";
  if ([RemotingPreferences.instance boolForFlag:RemotingFlagUseWebRTC]) {
    info.flags = "useWebrtc";
    [MDCSnackbarManager
        showMessage:[MDCSnackbarMessage messageWithText:@"Using WebRTC"]];
  }

  auto audioStream = std::make_unique<remoting::AudioPlaybackStream>(
      std::make_unique<remoting::AudioPlaybackSinkIos>(),
      _runtime->audio_task_runner());

  _displayHandler = [[GlDisplayHandler alloc] init];
  _displayHandler.delegate = self;

  _session = std::make_unique<remoting::ChromotingSession>(
      _sessonDelegate->GetWeakPtr(), [_displayHandler createCursorShapeStub],
      [_displayHandler createVideoRenderer], std::move(audioStream), info);
  _gestureInterpreter.SetContext(_displayHandler.rendererProxy, _session.get());
  _keyboardInterpreter.SetContext(_session.get());
}

- (void)disconnectFromHost {
  _session.reset();

  _displayHandler = nil;

  _gestureInterpreter.SetContext(nullptr, nullptr);
  _keyboardInterpreter.SetContext(nullptr);
}

#pragma mark - Eventing

- (void)hostSessionPinProvided:(NSNotification*)notification {
  NSString* pin = [[notification userInfo] objectForKey:kHostSessionPin];
  NSString* name = UIDevice.currentDevice.name;
  BOOL shouldCreatePairing = [[[notification userInfo]
      objectForKey:kHostSessionCreatePairing] boolValue];

  if (_session && shouldCreatePairing) {
    _session->RequestPairing(base::SysNSStringToUTF8(name));
  }

  if (_secretFetchedCallback) {
    std::move(_secretFetchedCallback).Run(base::SysNSStringToUTF8(pin));
  }
}

#pragma mark - Properties

- (HostInfo*)hostInfo {
  return _sessionDetails.hostInfo;
}

- (remoting::GestureInterpreter*)gestureInterpreter {
  return &_gestureInterpreter;
}

- (remoting::KeyboardInterpreter*)keyboardInterpreter {
  return &_keyboardInterpreter;
}

#pragma mark - ChromotingSession::Delegate

- (void)onConnectionState:(remoting::protocol::ConnectionToHost::State)state
                    error:(remoting::protocol::ErrorCode)error {
  switch (state) {
    case remoting::protocol::ConnectionToHost::INITIALIZING:
      _sessionDetails.state = SessionInitializing;
      break;
    case remoting::protocol::ConnectionToHost::CONNECTING:
      _sessionDetails.state = SessionConnecting;
      break;
    case remoting::protocol::ConnectionToHost::AUTHENTICATED:
      _sessionDetails.state = SessionAuthenticated;
      break;
    case remoting::protocol::ConnectionToHost::CONNECTED:
      _sessionDetails.state = SessionConnected;
      break;
    case remoting::protocol::ConnectionToHost::FAILED:
      _sessionDetails.state = SessionFailed;
      break;
    case remoting::protocol::ConnectionToHost::CLOSED:
      _sessionDetails.state = SessionClosed;
      [self disconnectFromHost];
      break;
    default:
      LOG(ERROR) << "onConnectionState, unknown state: " << state;
  }

  switch (error) {
    case remoting::protocol::ErrorCode::OK:
      _sessionDetails.error = SessionErrorOk;
      break;
    case remoting::protocol::ErrorCode::PEER_IS_OFFLINE:
      _sessionDetails.error = SessionErrorPeerIsOffline;
      break;
    case remoting::protocol::ErrorCode::SESSION_REJECTED:
      _sessionDetails.error = SessionErrorSessionRejected;
      break;
    case remoting::protocol::ErrorCode::INCOMPATIBLE_PROTOCOL:
      _sessionDetails.error = SessionErrorIncompatibleProtocol;
      break;
    case remoting::protocol::ErrorCode::AUTHENTICATION_FAILED:
      if (_sessionDetails.error != SessionErrorThirdPartyAuthNotSupported) {
        _sessionDetails.error = SessionErrorAuthenticationFailed;
      }
      break;
    case remoting::protocol::ErrorCode::INVALID_ACCOUNT:
      _sessionDetails.error = SessionErrorInvalidAccount;
      break;
    case remoting::protocol::ErrorCode::CHANNEL_CONNECTION_ERROR:
      _sessionDetails.error = SessionErrorChannelConnectionError;
      break;
    case remoting::protocol::ErrorCode::SIGNALING_ERROR:
      _sessionDetails.error = SessionErrorSignalingError;
      break;
    case remoting::protocol::ErrorCode::SIGNALING_TIMEOUT:
      _sessionDetails.error = SessionErrorSignalingTimeout;
      break;
    case remoting::protocol::ErrorCode::HOST_OVERLOAD:
      _sessionDetails.error = SessionErrorHostOverload;
      break;
    case remoting::protocol::ErrorCode::MAX_SESSION_LENGTH:
      _sessionDetails.error = SessionErrorMaxSessionLength;
      break;
    case remoting::protocol::ErrorCode::HOST_CONFIGURATION_ERROR:
      _sessionDetails.error = SessionErrorHostConfigurationError;
      break;
    default:
      _sessionDetails.error = SessionErrorUnknownError;
      break;
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostSessionStatusChanged
                    object:self
                  userInfo:[NSDictionary dictionaryWithObject:_sessionDetails
                                                       forKey:kSessionDetails]];
}

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)pairingId
                                 secret:(NSString*)secret {
  remoting::HostPairingInfo info = remoting::HostPairingInfo::GetPairingInfo(
      GetCurrentUserId(), base::SysNSStringToUTF8(host));
  std::string utf8PairingId = base::SysNSStringToUTF8(pairingId);
  std::string utf8Secret = base::SysNSStringToUTF8(secret);
  if (utf8PairingId == info.pairing_id() &&
      utf8Secret == info.pairing_secret()) {
    // The pairing has not been changed so we can return early.
    return;
  }

  info.set_pairing_id(utf8PairingId);
  info.set_pairing_secret(utf8Secret);
  info.Save();
}

- (void)
fetchSecretWithPairingSupported:(BOOL)pairingSupported
                       callback:
                           (const remoting::protocol::SecretFetchedCallback&)
                               secretFetchedCallback {
  _secretFetchedCallback = secretFetchedCallback;
  _sessionDetails.state = SessionPinPrompt;

  // Clear pairing credentials if they exist (which are no longer valid).
  [self commitPairingCredentialsForHost:self.hostInfo.hostId id:@"" secret:@""];

  [NSNotificationCenter.defaultCenter
      postNotificationName:kHostSessionStatusChanged
                    object:self
                  userInfo:@{
                    kSessionDetails : _sessionDetails,
                    kSessionSupportsPairing : @(pairingSupported),
                  }];
}

- (void)fetchThirdPartyTokenForUrl:(NSString*)tokenUrl
                          clientId:(NSString*)clientId
                            scopes:(NSString*)scopes
                          callback:(const remoting::protocol::
                                        ThirdPartyTokenFetchedCallback&)
                                       tokenFetchedCallback {
  // Not supported for iOS yet.
  _sessionDetails.state = SessionFailed;
  _sessionDetails.error = SessionErrorThirdPartyAuthNotSupported;
  tokenFetchedCallback.Run("", "");
}

- (void)setCapabilities:(NSString*)capabilities {
  DCHECK(capabilities.length == 0) << "No capability has been implemented on "
                                   << "iOS yet";
}

- (void)handleExtensionMessageOfType:(NSString*)type
                             message:(NSString*)message {
  NOTREACHED() << "handleExtensionMessageOfType is unimplemented. " << type
               << ":" << message;
}

- (void)setHostResolution:(CGSize)dipsResolution scale:(int)scale {
  if (_session) {
    _session->SendClientResolution(dipsResolution.width, dipsResolution.height,
                                   scale);
  }
}

- (void)setVideoChannelEnabled:(BOOL)enabled {
  if (_session) {
    _session->EnableVideoChannel(enabled);
  }
}

- (void)createFeedbackDataWithCallback:
    (void (^)(const remoting::FeedbackData&))callback {
  if (!_session) {
    // Session has never been connected. Returns an empty data.
    callback(remoting::FeedbackData());
    return;
  }

  _session->GetFeedbackData(
      base::BindOnce(&ResolveFeedbackDataCallback, callback));
}

#pragma mark - GlDisplayHandlerDelegate

- (void)canvasSizeChanged:(CGSize)size {
  _gestureInterpreter.OnDesktopSizeChanged(size.width, size.height);
}

- (void)rendererTicked {
  _gestureInterpreter.ProcessAnimations();
}

@end
