// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/capabilities.h"
#include "remoting/base/constants.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/base/session_options.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/action_message_handler.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/file_transfer/file_transfer_message_handler.h"
#include "remoting/host/file_transfer/rtc_log_file_operations.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mouse_shape_pump.h"
#include "remoting/host/remote_open_url/remote_open_url_constants.h"
#include "remoting/host/remote_open_url/remote_open_url_message_handler.h"
#include "remoting/host/remote_open_url/remote_open_url_util.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/remote_open_url/url_forwarder_control_message_handler.h"
#include "remoting/host/security_key/security_key_extension.h"
#include "remoting/host/security_key/security_key_extension_session.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"
#include "remoting/host/webauthn/remote_webauthn_message_handler.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/audio_stream.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/peer_connection_controls.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/video_frame_pump.h"
#include "remoting/protocol/webrtc_video_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

#if defined(WEBRTC_USE_GIO)
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"
#endif

namespace {

constexpr char kRtcLogTransferDataChannelPrefix[] = "rtc-log-transfer-";

constexpr base::TimeDelta kDefaultBoostCaptureInterval = base::Milliseconds(5);
constexpr base::TimeDelta kDefaultBoostDuration = base::Milliseconds(50);

}  // namespace

namespace remoting {

using protocol::ActionRequest;

ClientSession::ClientSession(
    EventHandler* event_handler,
    std::unique_ptr<protocol::ConnectionToClient> connection,
    DesktopEnvironmentFactory* desktop_environment_factory,
    const DesktopEnvironmentOptions& desktop_environment_options,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    const std::vector<raw_ptr<HostExtension, VectorExperimental>>& extensions,
    const LocalSessionPoliciesProvider* local_session_policies_provider)
    : event_handler_(event_handler),
      desktop_environment_factory_(desktop_environment_factory),
      desktop_environment_options_(desktop_environment_options),
      remote_input_filter_(&input_tracker_),
      fractional_input_filter_(&remote_input_filter_),
      mouse_clamping_filter_(&fractional_input_filter_),
      observing_input_filter_(&mouse_clamping_filter_),
      desktop_and_cursor_composer_notifier_(&observing_input_filter_, this),
      disable_input_filter_(&desktop_and_cursor_composer_notifier_),
      host_clipboard_filter_(clipboard_echo_filter_.host_filter()),
      client_clipboard_filter_(clipboard_echo_filter_.client_filter()),
      client_clipboard_factory_(&client_clipboard_filter_),
      pairing_registry_(pairing_registry),
      connection_(std::move(connection)),
      client_jid_(connection_->session()->jid()),
      local_session_policies_provider_(local_session_policies_provider) {
  connection_->session()->AddPlugin(&host_experiment_session_plugin_);
  connection_->SetEventHandler(this);

  // Create a manager for the configured extensions, if any.
  extension_manager_ =
      std::make_unique<HostExtensionSessionManager>(extensions, this);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  // LocalMouseInputMonitorWin and LocalPointerInputMonitorChromeos filter out
  // an echo of the injected input before it reaches |remote_input_filter_|.
  remote_input_filter_.SetExpectLocalEcho(false);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
}

ClientSession::~ClientSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!audio_stream_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(video_streams_.empty());
}

void ClientSession::NotifyClientResolution(
    const protocol::ClientResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(resolution.width_pixels() >= 0 && resolution.height_pixels() >= 0);
  VLOG(1) << "Received ClientResolution (width=" << resolution.width_pixels()
          << ", height=" << resolution.height_pixels()
          << ", x_dpi=" << resolution.x_dpi()
          << ", y_dpi=" << resolution.y_dpi() << ")";

  if (!screen_controls_) {
    return;
  }

  webrtc::DesktopSize client_size(resolution.width_pixels(),
                                  resolution.height_pixels());
  if (connection_->session()->config().protocol() ==
      protocol::SessionConfig::Protocol::WEBRTC) {
    // When using WebRTC round down the dimensions to multiple of 2. Otherwise
    // the dimensions will be rounded on the receiver, which will cause blurring
    // due to scaling. The resulting size is still close to the client size and
    // will fit on the client's screen without scaling.
    // TODO(sergeyu): Make WebRTC handle odd dimensions properly.
    // crbug.com/636071
    client_size.set(client_size.width() & (~1), client_size.height() & (~1));
  }

  // TODO(joedow): Determine if other platforms support desktop scaling.
  webrtc::DesktopVector dpi_vector{kDefaultDpi, kDefaultDpi};
#if BUILDFLAG(IS_WIN)
  // Matching the client DPI is only supported on Windows when curtained.
  if (desktop_environment_options_.enable_curtaining()) {
    dpi_vector.set(resolution.x_dpi(), resolution.y_dpi());
  }
#elif BUILDFLAG(IS_LINUX)
  dpi_vector.set(resolution.x_dpi(), resolution.y_dpi());
#endif

  // Try to match the client's resolution.
  ScreenResolution screen_resolution(client_size, dpi_vector);
  std::optional<webrtc::ScreenId> screen_id;
  if (resolution.has_screen_id()) {
    screen_id = resolution.screen_id();
  }
  screen_controls_->SetScreenResolution(screen_resolution, screen_id);
}

void ClientSession::ControlVideo(const protocol::VideoControl& video_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note that |video_stream_| may be null, depending upon whether
  // extensions choose to wrap or "steal" the video capturer or encoder.
  if (video_control.has_enable()) {
    VLOG(1) << "Received VideoControl (enable=" << video_control.enable()
            << ")";
    pause_video_ = !video_control.enable();
    for (const auto& [_, video_stream] : video_streams_) {
      video_stream->Pause(pause_video_);
    }
  }

  if (video_control.has_target_framerate()) {
    target_framerate_ = video_control.target_framerate();
    LOG(INFO) << "Received target framerate: " << target_framerate_;
    for (const auto& [_, video_stream] : video_streams_) {
      video_stream->SetTargetFramerate(target_framerate_);
    }
    if (mouse_shape_pump_) {
      mouse_shape_pump_->SetCursorCaptureInterval(
          base::Hertz(target_framerate_));
    }
  }

  if (video_control.has_framerate_boost()) {
    auto framerate_boost = video_control.framerate_boost();
    DCHECK(framerate_boost.has_enabled());

    if (!framerate_boost.enabled()) {
      LOG(INFO) << "FramerateBoost disabled.";
      observing_input_filter_.ClearInputEventCallback();
    } else {
      base::TimeDelta capture_interval =
          framerate_boost.has_capture_interval_ms()
              ? std::clamp(
                    base::Milliseconds(framerate_boost.capture_interval_ms()),
                    base::Milliseconds(1), base::Milliseconds(1000))
              : kDefaultBoostCaptureInterval;
      base::TimeDelta boost_duration =
          framerate_boost.has_boost_duration_ms()
              ? std::clamp(
                    base::Milliseconds(framerate_boost.boost_duration_ms()),
                    base::Milliseconds(1), base::Milliseconds(1000))
              : kDefaultBoostDuration;
      LOG(INFO) << "FramerateBoost enabled (interval: "
                << capture_interval.InMilliseconds()
                << "ms, duration: " << boost_duration.InMilliseconds() << "ms)";

      // Unretained is sound as this instance owns |observing_input_filter_|.
      observing_input_filter_.SetInputEventCallback(base::BindRepeating(
          &ClientSession::BoostFramerateOnInput, base::Unretained(this),
          capture_interval, boost_duration, base::OwnedRef(false)));
    }
  }
}

void ClientSession::ControlAudio(const protocol::AudioControl& audio_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_control.has_enable()) {
    VLOG(1) << "Received AudioControl (enable=" << audio_control.enable()
            << ")";
    if (audio_stream_) {
      audio_stream_->Pause(!audio_control.enable());
    }
  }
}

void ClientSession::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all the messages but the 1st one.
  if (client_capabilities_) {
    LOG(WARNING) << "protocol::Capabilities has been received already.";
    return;
  }

  // Compute the set of capabilities supported by both client and host.
  client_capabilities_ = std::make_unique<std::string>();
  if (capabilities.has_capabilities()) {
    *client_capabilities_ = capabilities.capabilities();
  }
  capabilities_ =
      IntersectCapabilities(*client_capabilities_, host_capabilities_);
  extension_manager_->OnNegotiatedCapabilities(connection_->client_stub(),
                                               capabilities_);

  if (HasCapability(capabilities_, protocol::kFileTransferCapability)) {
    data_channel_manager_.RegisterCreateHandlerCallback(
        kFileTransferDataChannelPrefix,
        base::BindRepeating(&ClientSession::CreateFileTransferMessageHandler,
                            base::Unretained(this)));
  }

  if (HasCapability(capabilities_, protocol::kRtcLogTransferCapability)) {
    data_channel_manager_.RegisterCreateHandlerCallback(
        kRtcLogTransferDataChannelPrefix,
        base::BindRepeating(&ClientSession::CreateRtcLogTransferMessageHandler,
                            base::Unretained(this)));
  }

  if (HasCapability(capabilities_, protocol::kRemoteOpenUrlCapability)) {
    data_channel_manager_.RegisterCreateHandlerCallback(
        kRemoteOpenUrlDataChannelName,
        base::BindRepeating(&ClientSession::CreateRemoteOpenUrlMessageHandler,
                            base::Unretained(this)));
    data_channel_manager_.RegisterCreateHandlerCallback(
        UrlForwarderControlMessageHandler::kDataChannelName,
        base::BindRepeating(
            &ClientSession::CreateUrlForwarderControlMessageHandler,
            base::Unretained(this)));
  }

  if (HasCapability(capabilities_, protocol::kRemoteWebAuthnCapability)) {
    data_channel_manager_.RegisterCreateHandlerCallback(
        kRemoteWebAuthnDataChannelName,
        base::BindRepeating(&ClientSession::CreateRemoteWebAuthnMessageHandler,
                            base::Unretained(this)));
  }

  std::vector<ActionRequest::Action> supported_actions;
  if (HasCapability(capabilities_, protocol::kSendAttentionSequenceAction)) {
    supported_actions.push_back(ActionRequest::SEND_ATTENTION_SEQUENCE);
  }
  if (HasCapability(capabilities_, protocol::kLockWorkstationAction)) {
    supported_actions.push_back(ActionRequest::LOCK_WORKSTATION);
  }

  if (supported_actions.size() > 0) {
    // Register the action message handler.
    data_channel_manager_.RegisterCreateHandlerCallback(
        kActionDataChannelPrefix,
        base::BindRepeating(&ClientSession::CreateActionMessageHandler,
                            base::Unretained(this),
                            std::move(supported_actions)));
  }

  // TODO(crbug.com/40225767): Remove this code when legacy VideoLayout messages
  // are fully deprecated and no longer sent. We already start the monitor in
  // OnConnectionChannelsConnected() so we don't need this block if the legacy
  // message in multi-stream mode is no longer required.
  if (HasCapability(capabilities_, protocol::kMultiStreamCapability)) {
    if (desktop_display_info_.NumDisplays() != 0) {
      // If display info is already known, create the initial video streams.
      // Otherwise they will be created on the next displays-changed message.
      CreatePerMonitorVideoStreams();
    }

    // Query the OS for the display-info on a timer, instead of doing it after
    // every captured frame from multiple capturers.
    auto* monitor = desktop_environment_->GetDisplayInfoMonitor();
    if (monitor) {
      // In the multi-process case, |monitor| will be null and this will be
      // handled instead by DesktopSessionAgent.
      monitor->Start();
    }

    active_display_monitor_ =
        desktop_environment_->CreateActiveDisplayMonitor(base::BindRepeating(
            &ClientSession::OnActiveDisplayChanged, base::Unretained(this)));

    // Re-send the extended layout information so the client has information
    // needed to identify each stream.
    if (desktop_display_info_.NumDisplays() != 0) {
      OnDesktopDisplayChanged(desktop_display_info_.GetVideoLayoutProto());
    }
  }

  data_channel_manager_.OnRegistrationComplete();

  VLOG(1) << "Client capabilities: " << *client_capabilities_;

  desktop_environment_->SetCapabilities(capabilities_);
}

void ClientSession::RequestPairing(
    const protocol::PairingRequest& pairing_request) {
  if (pairing_registry_.get() && pairing_request.has_client_name()) {
    protocol::PairingRegistry::Pairing pairing =
        pairing_registry_->CreatePairing(pairing_request.client_name());
    protocol::PairingResponse pairing_response;
    pairing_response.set_client_id(pairing.client_id());
    pairing_response.set_shared_secret(pairing.shared_secret());
    connection_->client_stub()->SetPairingResponse(pairing_response);
  }
}

void ClientSession::DeliverClientMessage(
    const protocol::ExtensionMessage& message) {
  if (message.has_type()) {
    if (extension_manager_->OnExtensionMessage(message)) {
      return;
    }

    DLOG(INFO) << "Unexpected message received: " << message.type() << ": "
               << message.data();
  }
}

void ClientSession::SelectDesktopDisplay(
    const protocol::SelectDesktopDisplayRequest& select_display) {
  LOG(INFO) << "SelectDesktopDisplay "
            << "'" << select_display.id() << "'";

  if (HasCapability(capabilities_, protocol::kMultiStreamCapability)) {
    // TODO(lambroslambrou): Close the connection with a protocol error,
    // once we are sure the client will not send this request after
    // multi-stream has been negotiated.
    LOG(ERROR) << "SelectDesktopDisplayRequest received after multi-stream is "
                  "enabled.";
    return;
  }

  // Parse the string with the selected display. Note that this request's |id|
  // field is not a monitor ID, but an index into the list of displays (or the
  // special string "all"),
  int new_index = webrtc::kInvalidScreenId;
  if (select_display.id() == "all") {
    new_index = webrtc::kFullDesktopScreenId;
  } else {
    if (!base::StringToInt(select_display.id().c_str(), &new_index)) {
      LOG(ERROR) << "  Unable to parse display index "
                 << "'" << select_display.id() << "'";
      new_index = webrtc::kInvalidScreenId;
    }
    if (!desktop_display_info_.GetDisplayInfo(new_index)) {
      LOG(ERROR) << "  Invalid display index "
                 << "'" << select_display.id() << "'";
      new_index = webrtc::kInvalidScreenId;
    }
  }

  // Don't allow requests for fullscreen if not supported by the current
  // display configuration.
  if (!can_capture_full_desktop_ && new_index == webrtc::kFullDesktopScreenId) {
    LOG(ERROR) << "  Full desktop not supported";
    new_index = webrtc::kInvalidScreenId;
  }
  // Fall back to default capture config if invalid request.
  if (new_index == webrtc::kInvalidScreenId) {
    LOG(ERROR) << "  Invalid display specification, falling back to default";
    new_index = can_capture_full_desktop_ ? webrtc::kFullDesktopScreenId : 0;
  }

  if (selected_display_index_ == new_index) {
    LOG(INFO) << "  Display " << new_index << " is already selected. Ignoring";
    return;
  }

  const DisplayGeometry* oldGeo =
      desktop_display_info_.GetDisplayInfo(selected_display_index_);
  const DisplayGeometry* newGeo =
      desktop_display_info_.GetDisplayInfo(new_index);

  auto& stream = video_streams_.begin()->second;
  if (newGeo) {
    stream->SelectSource(newGeo->id);
  } else if (new_index == webrtc::kFullDesktopScreenId) {
    stream->SelectSource(webrtc::kFullDesktopScreenId);
  } else {
    // This corner-case might occur if fullscreen capture is not supported, and
    // the fallback default of 0 is not a valid index (the list is empty).
    LOG(ERROR) << "  Display geometry not found for index " << new_index;
    return;
  }

  selected_display_index_ = new_index;

  // If the old and new displays are the different sizes, then SelectSource()
  // will trigger an OnVideoSizeChanged() message which will update the mouse
  // filters.
  // However, if the old and new displays are the exact same size, then the
  // video size message will not be generated (because the size of the video
  // has not changed). But we still need to update the mouse clamping filter
  // with the new display origin, so we update that directly.
  if (oldGeo != nullptr && newGeo != nullptr) {
    if (oldGeo->width == newGeo->width && oldGeo->height == newGeo->height) {
      UpdateMouseClampingFilterOffset();
      UpdateFractionalFilterFallback();
    }
  }
}

void ClientSession::ControlPeerConnection(
    const protocol::PeerConnectionParameters& parameters) {
  if (!connection_->peer_connection_controls()) {
    return;
  }
  std::optional<int> min_bitrate_bps;
  std::optional<int> max_bitrate_bps;
  bool set_preferred_bitrates = false;
  if (parameters.has_preferred_min_bitrate_bps()) {
    min_bitrate_bps = parameters.preferred_min_bitrate_bps();
    set_preferred_bitrates = true;
  }
  if (parameters.has_preferred_max_bitrate_bps()) {
    max_bitrate_bps = parameters.preferred_max_bitrate_bps();
    set_preferred_bitrates = true;
  }
  if (set_preferred_bitrates) {
    connection_->peer_connection_controls()->SetPreferredBitrates(
        min_bitrate_bps, max_bitrate_bps);
  }

  if (parameters.request_ice_restart()) {
    connection_->peer_connection_controls()->RequestIceRestart();
  }

  if (parameters.request_sdp_restart()) {
    connection_->peer_connection_controls()->RequestSdpRestart();
  }
}

void ClientSession::SetVideoLayout(const protocol::VideoLayout& video_layout) {
  screen_controls_->SetVideoLayout(video_layout);
}

void ClientSession::OnConnectionAuthenticating() {
  event_handler_->OnSessionAuthenticating(this);
}

void ClientSession::OnConnectionAuthenticated(
    const SessionPolicies* session_policies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!audio_stream_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(video_streams_.empty());

  is_authenticated_ = true;

  desktop_display_info_.Reset();

  if (session_policies) {
    effective_policies_ = *session_policies;
    HOST_LOG << "Connection authenticated with remote session policies: "
             << effective_policies_;
  } else {
    effective_policies_ =
        local_session_policies_provider_->get_local_policies();
    local_session_policy_update_subscription_ =
        local_session_policies_provider_->AddLocalPoliciesChangedCallback(
            base::BindRepeating(&ClientSession::OnLocalSessionPoliciesChanged,
                                weak_factory_.GetWeakPtr()));
    HOST_LOG << "Connection authenticated with local session policies: "
             << effective_policies_;
  }

  base::TimeDelta max_duration =
      effective_policies_.maximum_session_duration.value_or(base::TimeDelta());
  if (max_duration.is_positive()) {
    max_duration_timer_.Start(
        FROM_HERE, max_duration,
        base::BindOnce(&ClientSession::DisconnectSession,
                       base::Unretained(this), ErrorCode::MAX_SESSION_LENGTH));
  }

  // Notify EventHandler.
  event_handler_->OnSessionAuthenticated(this);

  const SessionOptions session_options(
      host_experiment_session_plugin_.configuration());

  connection_->ApplySessionOptions(session_options);
  connection_->ApplyNetworkSettings(
      protocol::NetworkSettings(effective_policies_));

  DesktopEnvironmentOptions options = desktop_environment_options_;
  options.ApplySessionOptions(session_options);
  if (effective_policies_.curtain_required.has_value()) {
    options.set_enable_curtaining(*effective_policies_.curtain_required);
  }
  // Create the desktop environment. Drop the connection if it could not be
  // created for any reason (for instance the curtain could not initialize).
  desktop_environment_ = desktop_environment_factory_->Create(
      weak_factory_.GetWeakPtr(), weak_factory_.GetWeakPtr(), options);
  if (!desktop_environment_) {
    DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
    return;
  }

  // Connect host stub.
  connection_->set_host_stub(this);

  // Collate the set of capabilities to offer the client, if it supports them.
  host_capabilities_ = desktop_environment_->GetCapabilities();
  if (!host_capabilities_.empty()) {
    host_capabilities_.append(" ");
  }
  host_capabilities_.append(extension_manager_->GetCapabilities());
  if (!host_capabilities_.empty()) {
    host_capabilities_.append(" ");
  }
  host_capabilities_.append(protocol::kRtcLogTransferCapability);
  host_capabilities_.append(" ");
  host_capabilities_.append(protocol::kWebrtcIceSdpRestartAction);
  host_capabilities_.append(" ");
  host_capabilities_.append(protocol::kFractionalCoordinatesCapability);
  if (InputInjector::SupportsTouchEvents()) {
    host_capabilities_.append(" ");
    host_capabilities_.append(protocol::kTouchEventsCapability);
  }
  if (effective_policies_.allow_file_transfer.value_or(true)) {
    host_capabilities_.append(" ");
    host_capabilities_.append(protocol::kFileTransferCapability);
  }
  if (effective_policies_.allow_uri_forwarding.value_or(true) &&
      IsRemoteOpenUrlSupported()) {
    host_capabilities_.append(" ");
    host_capabilities_.append(protocol::kRemoteOpenUrlCapability);
  }

  // Create the object that controls the screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();

  // Create the event executor.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Connect the host input stubs.
  connection_->set_input_stub(&disable_input_filter_);
  input_tracker_.set_input_stub(input_injector_.get());

  if (effective_policies_.clipboard_size_bytes.has_value()) {
    int max_size = *effective_policies_.clipboard_size_bytes;

    client_clipboard_filter_.set_max_size(max_size);
    host_clipboard_filter_.set_max_size(max_size);
  }

  // Connect the clipboard stubs.
  connection_->set_clipboard_stub(&host_clipboard_filter_);
  clipboard_echo_filter_.set_host_stub(input_injector_.get());
  clipboard_echo_filter_.set_client_stub(connection_->client_stub());
}

void ClientSession::CreateMediaStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a VideoStream to pump frames from the capturer to the client.
  DCHECK(video_streams_.empty());

  auto video_stream = connection_->StartVideoStream(
      webrtc::kFullDesktopScreenId,
      desktop_environment_->CreateVideoCapturer(webrtc::kFullDesktopScreenId));

  // Create an AudioStream to pump audio from the capturer to the client.
  std::unique_ptr<protocol::AudioSource> audio_capturer =
      desktop_environment_->CreateAudioCapturer();
  if (audio_capturer) {
    audio_stream_ = connection_->StartAudioStream(std::move(audio_capturer));
  }

  video_stream->SetObserver(this);

  // Pause capturing if necessary.
  video_stream->Pause(pause_video_);

  // Set the current target framerate.
  video_stream->SetTargetFramerate(target_framerate_);

  if (event_timestamp_source_for_tests_) {
    video_stream->SetEventTimestampsSource(event_timestamp_source_for_tests_);
  }

  // Store the single video-stream using a key that isn't a valid monitor-id.
  // If multi-stream is enabled, this entry will get removed when the new
  // video-streams are created.
  video_streams_[webrtc::kInvalidScreenId] = std::move(video_stream);
}

void ClientSession::CreatePerMonitorVideoStreams() {
  // Undo any previously-set fallback. When there are multiple streams, all
  // fractional coordinates must specify a screen_id.
  fractional_input_filter_.set_fallback_geometry({});

  // Create new streams for any monitors that don't already have streams.
  for (int i = 0; i < desktop_display_info_.NumDisplays(); i++) {
    auto id = desktop_display_info_.GetDisplayInfo(i)->id;

    if (base::Contains(video_streams_, id)) {
      HOST_LOG << "Video stream for id " << id << " already exists.";
      continue;
    }

    HOST_LOG << "Creating video stream for id " << id;

    auto video_stream = connection_->StartVideoStream(
        id, desktop_environment_->CreateVideoCapturer(id));

    // SetObserver(this) is not called on the new video-stream, because
    // per-monitor resizing should be handled by OnDesktopDisplayChanged()
    // rather than OnVideoSizeChanged(). The latter would send out a legacy
    // (non-extended) video-layout message, which may confuse the client when
    // multi-stream is being used.

    // Pause capturing if necessary.
    video_stream->Pause(pause_video_);

    // Set the current target framerate.
    video_stream->SetTargetFramerate(target_framerate_);

    if (event_timestamp_source_for_tests_) {
      video_stream->SetEventTimestampsSource(event_timestamp_source_for_tests_);
    }

    video_streams_[id] = std::move(video_stream);
  }

  // Delete any streams that no longer have monitors in |desktop_display_info_|.
  // This will also delete any video-stream for the single-stream case, because
  // it is stored with a key chosen to not be a valid monitor ID.
  const auto& displays = desktop_display_info_.displays();
  std::erase_if(video_streams_, [displays](const auto& id_stream_pair) {
    webrtc::ScreenId id = id_stream_pair.first;
    bool keep = base::Contains(
        displays, id, [](const DisplayGeometry& geo) { return geo.id; });
    HOST_LOG << (keep ? "Keeping" : "Removing") << " video stream for id "
             << id;
    return !keep;
  });
}

void ClientSession::OnConnectionChannelsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!channels_connected_);
  channels_connected_ = true;

  // Negotiate capabilities with the client.
  VLOG(1) << "Host capabilities: " << host_capabilities_;
  protocol::Capabilities capabilities;
  capabilities.set_capabilities(host_capabilities_);
  connection_->client_stub()->SetCapabilities(capabilities);

  // Start the event executor.
  input_injector_->Start(CreateClipboardProxy());
  SetDisableInputs(false);

  // Create MouseShapePump to send mouse cursor shape.
  mouse_shape_pump_ = std::make_unique<MouseShapePump>(
      desktop_environment_->CreateMouseCursorMonitor(),
      connection_->client_stub());
  mouse_shape_pump_->SetMouseCursorMonitorCallback(this);
  mouse_shape_pump_->SetCursorCaptureInterval(base::Hertz(target_framerate_));

  // Create KeyboardLayoutMonitor to send keyboard layout.
  // Unretained is sound because callback will never be called after
  // |keyboard_layout_monitor_| has been destroyed, and |connection_| (which
  // owns the client stub) is guaranteed to outlive |keyboard_layout_monitor_|.
  keyboard_layout_monitor_ = desktop_environment_->CreateKeyboardLayoutMonitor(
      base::BindRepeating(&protocol::KeyboardLayoutStub::SetKeyboardLayout,
                          base::Unretained(connection_->client_stub())));
  keyboard_layout_monitor_->Start();

  if (pending_video_layout_message_) {
    connection_->client_stub()->SetVideoLayout(*pending_video_layout_message_);
    pending_video_layout_message_.reset();
  }

  // Query the OS for the display-info on a timer.
  auto* display_info_monitor = desktop_environment_->GetDisplayInfoMonitor();
  if (display_info_monitor) {
    // In the multi-process case, |display_info_monitor| will be null and this
    // will be handled instead by the DesktopSessionAgent.
    display_info_monitor->Start();
  }

  // Notify the event handler that all our channels are now connected.
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnConnectionClosed(protocol::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HOST_LOG << "Client disconnected: " << client_jid_
           << "; error = " << ErrorCodeToString(error);

  // Ignore any further callbacks.
  weak_factory_.InvalidateWeakPtrs();

  // If the client never authenticated then the session failed.
  if (!is_authenticated_) {
    event_handler_->OnSessionAuthenticationFailed(this);
  }

  // ReleaseAll() requires an InputInjector, which might not be present if a
  // connection wasn't established.
  if (input_injector_) {
    // Ensure that any pressed keys or buttons are released.
    input_tracker_.ReleaseAll();

    // Avoid dangling raw_ptr in `input_tracker_` after deleting
    // `input_injector_` below.
    input_tracker_.set_input_stub(nullptr);
  }

  // Stop components access the client, audio or video stubs, which are no
  // longer valid once ConnectionToClient calls OnConnectionClosed().
  audio_stream_.reset();
  mouse_shape_pump_.reset();
  video_streams_.clear();
  keyboard_layout_monitor_.reset();
  client_clipboard_factory_.InvalidateWeakPtrs();
  input_injector_.reset();
  screen_controls_.reset();
  desktop_environment_.reset();

  // Notify the ChromotingHost that this client is disconnected.
  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnTransportProtocolChange(const std::string& protocol) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Transport protocol: " << protocol;
  protocol::TransportInfo transport_info;
  transport_info.set_protocol(protocol);
  connection_->client_stub()->SetTransportInfo(transport_info);
}

void ClientSession::OnRouteChange(const std::string& channel_name,
                                  const protocol::TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  event_handler_->OnSessionRouteChange(this, channel_name, route);
}

void ClientSession::OnIncomingDataChannel(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  data_channel_manager_.OnIncomingDataChannel(channel_name, std::move(pipe));
}

const std::string& ClientSession::client_jid() const {
  return client_jid_;
}

void ClientSession::DisconnectSession(protocol::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_.get());

  max_duration_timer_.Stop();

  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect(error);
}

void ClientSession::OnLocalKeyPressed(uint32_t usb_keycode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_local = remote_input_filter_.LocalKeyPressed(usb_keycode);
  if (is_local && desktop_environment_options_.terminate_upon_input()) {
    LOG(WARNING)
        << "Disconnecting CRD session because local input was detected.";
    DisconnectSession(ErrorCode::OK);
  }
}

void ClientSession::OnLocalPointerMoved(const webrtc::DesktopVector& position,
                                        ui::EventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_local = remote_input_filter_.LocalPointerMoved(position, type);
  if (is_local) {
    if (desktop_environment_options_.terminate_upon_input()) {
      LOG(WARNING)
          << "Disconnecting CRD session because local input was detected.";
      DisconnectSession(ErrorCode::OK);
    } else {
      desktop_and_cursor_composer_notifier_.OnLocalInput();
    }
  }
}

void ClientSession::SetDisableInputs(bool disable_inputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disable_inputs) {
    input_tracker_.ReleaseAll();
  }

  disable_input_filter_.set_enabled(!disable_inputs);
  host_clipboard_filter_.set_enabled(!disable_inputs);
}

uint32_t ClientSession::desktop_session_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(desktop_environment_);
  return desktop_environment_->GetDesktopSessionId();
}

ClientSessionControl* ClientSession::session_control() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

void ClientSession::SetComposeEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetComposeEnabled(enabled);
  }
}

void ClientSession::OnMouseCursor(webrtc::MouseCursor* mouse_cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This method should take ownership of |mouse_cursor|.
  std::unique_ptr<webrtc::MouseCursor> owned_cursor(mouse_cursor);

  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(*owned_cursor)));
  }
}

void ClientSession::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetMouseCursorPosition(position);
  }
}

void ClientSession::BindReceiver(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  session_services_receivers_.Add(this, std::move(receiver));
}

void ClientSession::BindWebAuthnProxy(
    mojo::PendingReceiver<mojom::WebAuthnProxy> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_webauthn_message_handler_) {
    LOG(WARNING)
        << "No WebAuthn message handler is found. Binding request rejected.";
    return;
  }
  remote_webauthn_message_handler_->AddReceiver(std::move(receiver));
}

void ClientSession::BindRemoteUrlOpener(
    mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_open_url_message_handler_) {
    LOG(WARNING) << "No RemoteOpenUrl message handler is found. Binding "
                 << "request rejected.";
    return;
  }
  remote_open_url_message_handler_->AddReceiver(std::move(receiver));
}

#if BUILDFLAG(IS_WIN)
void ClientSession::BindSecurityKeyForwarder(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* extension_session = reinterpret_cast<SecurityKeyExtensionSession*>(
      extension_manager_->FindExtensionSession(
          SecurityKeyExtension::kCapability));
  if (!extension_session) {
    LOG(WARNING) << "Security key extension not found. "
                 << "Binding request rejected.";
    return;
  }
  extension_session->BindSecurityKeyForwarder(std::move(receiver));
}
#endif

void ClientSession::RegisterCreateHandlerCallbackForTesting(
    const std::string& prefix,
    protocol::DataChannelManager::CreateHandlerCallback constructor) {
  data_channel_manager_.RegisterCreateHandlerCallback(prefix,
                                                      std::move(constructor));
}

void ClientSession::SetEventTimestampsSourceForTests(
    scoped_refptr<protocol::InputEventTimestampsSource>
        event_timestamp_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  event_timestamp_source_for_tests_ = event_timestamp_source;
  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetEventTimestampsSource(event_timestamp_source_for_tests_);
  }
}

std::unique_ptr<protocol::ClipboardStub> ClientSession::CreateClipboardProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<protocol::ClipboardThreadProxy>(
      client_clipboard_factory_.GetWeakPtr(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ClientSession::SetMouseClampingFilter(const DisplaySize& size) {
  UpdateMouseClampingFilterOffset();

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS uses Screen DIP coordinates to uniquely position all displays.
  mouse_clamping_filter_.set_output_size(size.WidthAsDips(),
                                         size.HeightAsDips());
#else
  mouse_clamping_filter_.set_output_size(size.WidthAsPixels(),
                                         size.HeightAsPixels());
#endif  // BUILDFLAG(IS_CHROMEOS)

  switch (connection_->session()->config().protocol()) {
    case protocol::SessionConfig::Protocol::ICE:
      mouse_clamping_filter_.set_input_size(size.WidthAsPixels(),
                                            size.HeightAsPixels());
      break;

    case protocol::SessionConfig::Protocol::WEBRTC: {
#if BUILDFLAG(IS_APPLE)
      mouse_clamping_filter_.set_input_size(size.WidthAsPixels(),
                                            size.HeightAsPixels());
#else
      // When using the WebRTC protocol the client sends mouse coordinates in
      // DIPs, while InputInjector expects them in physical pixels.
      // TODO(sergeyu): Fix InputInjector implementations to use DIPs as well.
      mouse_clamping_filter_.set_input_size(size.WidthAsDips(),
                                            size.HeightAsDips());
#endif  // BUILDFLAG(IS_APPLE)
    }
  }
}

void ClientSession::UpdateMouseClampingFilterOffset() {
  if (selected_display_index_ == webrtc::kInvalidScreenId) {
    return;
  }

  webrtc::DesktopVector origin;
  origin = desktop_display_info_.CalcDisplayOffset(selected_display_index_);
  mouse_clamping_filter_.set_output_offset(origin);
}

void ClientSession::OnLocalSessionPoliciesChanged(
    const SessionPolicies& new_policies) {
  DCHECK(local_session_policy_update_subscription_);
  HOST_LOG << "Effective policies have changed. Terminating session.";
  DisconnectSession(ErrorCode::SESSION_POLICIES_CHANGED);
}

void ClientSession::OnVideoSizeChanged(protocol::VideoStream* video_stream,
                                       const webrtc::DesktopSize& size_px,
                                       const webrtc::DesktopVector& dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "ClientSession::OnVideoSizeChanged";
  DisplaySize size =
      DisplaySize::FromPixels(size_px.width(), size_px.height(), dpi.x());
  LOG(INFO) << "  DisplaySize: " << size
            << " (size in pixels: " << size_px.width() << "x"
            << size_px.height() << ")";

  // The first video size message that we receive from WebRtc is the full
  // desktop size (if supported). If full desktop capture is not supported,
  // then this will be the size of the default display.
  if (default_webrtc_desktop_size_.IsEmpty()) {
    default_webrtc_desktop_size_ = size;
    LOG(INFO) << "  display index " << selected_display_index_;
    LOG(INFO) << "  Recording default webrtc capture size "
              << default_webrtc_desktop_size_;
  }
  webrtc_capture_size_ = size;

  SetMouseClampingFilter(size);
  UpdateFractionalFilterFallback();

  // Record default DPI in case a display reports 0 for DPI.
  default_x_dpi_ = dpi.x();
  default_y_dpi_ = dpi.y();
  if (dpi.x() != dpi.y()) {
    LOG(WARNING) << "Mismatch x,y dpi. x=" << dpi.x() << " y=" << dpi.y();
  }

  if (connection_->session()->config().protocol() !=
      protocol::SessionConfig::Protocol::WEBRTC) {
    return;
  }

  // Generate and send VideoLayout message.
  protocol::VideoLayout layout;
  protocol::VideoTrackLayout* video_track = layout.add_video_track();
  video_track->set_position_x(0);
  video_track->set_position_y(0);
  video_track->set_width(size.WidthAsDips());
  video_track->set_height(size.HeightAsDips());
  video_track->set_x_dpi(dpi.x());
  video_track->set_y_dpi(dpi.y());

  // VideoLayout can be sent only after the control channel is connected.
  // TODO(sergeyu): Change client_stub() implementation to allow queuing
  // while connection is being established.
  if (channels_connected_) {
    connection_->client_stub()->SetVideoLayout(layout);
  } else {
    pending_video_layout_message_ =
        std::make_unique<protocol::VideoLayout>(layout);
  }
}

void ClientSession::OnDesktopDisplayChanged(
    std::unique_ptr<protocol::VideoLayout> displays) {
  LOG(INFO) << "ClientSession::OnDesktopDisplayChanged";

  bool multiStreamEnabled =
      HasCapability(capabilities_, protocol::kMultiStreamCapability);

  // Scan display list to calculate the full desktop size.
  int min_x = 0;
  int max_x = 0;
  int min_y = 0;
  int max_y = 0;
  int dpi_x = 0;
  int dpi_y = 0;
  LOG(INFO) << "  Scanning display info... (dips)";
  for (int display_id = 0; display_id < displays->video_track_size();
       display_id++) {
    protocol::VideoTrackLayout track = displays->video_track(display_id);
    LOG(INFO) << "   #" << display_id << " : " << track.position_x() << ","
              << track.position_y() << " " << track.width() << "x"
              << track.height() << " [" << track.x_dpi() << "," << track.y_dpi()
              << "], screen_id=" << track.screen_id();
    if (dpi_x == 0) {
      dpi_x = track.x_dpi();
    }
    if (dpi_y == 0) {
      dpi_y = track.y_dpi();
    }

    int x = track.position_x();
    int y = track.position_y();
    min_x = std::min(x, min_x);
    min_y = std::min(y, min_y);
    max_x = std::max(x + track.width(), max_x);
    max_y = std::max(y + track.height(), max_y);
  }

  // TODO(garykac): Investigate why these DPI values are 0 for some users.
  if (dpi_x == 0) {
    dpi_x = default_x_dpi_;
  }
  if (dpi_y == 0) {
    dpi_y = default_y_dpi_;
  }

  // Calc desktop scaled geometry (in DIPs)
  // See comment in OnVideoSizeChanged() for details.
  const webrtc::DesktopSize size(max_x - min_x, max_y - min_y);

  // If this is our first message, then we need to determine if the current
  // display configuration supports capturing the entire desktop.
  LOG(INFO) << "    Webrtc desktop size " << default_webrtc_desktop_size_;
  if (selected_display_index_ == webrtc::kInvalidScreenId) {
#if BUILDFLAG(IS_APPLE)
    // On MacOS, there are situations where webrtc cannot capture the entire
    // desktop (e.g, when there are displays with different DPIs). We detect
    // this situation by comparing the full desktop size (calculated above
    // from the displays) and the size of the initial webrtc capture (which
    // defaults to the full desktop if supported).
    if (size.width() == default_webrtc_desktop_size_.WidthAsDips() &&
        size.height() == default_webrtc_desktop_size_.HeightAsDips()) {
      LOG(INFO) << "    Full desktop capture supported.";
      can_capture_full_desktop_ = true;
    } else {
      LOG(INFO)
          << "    This configuration does not support full desktop capture.";
      can_capture_full_desktop_ = false;
    }
#elif BUILDFLAG(IS_CHROMEOS)
    can_capture_full_desktop_ = false;
#else
    // Windows/Linux can capture full desktop if multiple displays.
    can_capture_full_desktop_ = true;
#endif  // BUILDFLAG(IS_APPLE)
  }

  // Generate and send VideoLayout message.
  protocol::VideoLayout layout;
  layout.set_supports_full_desktop_capture(can_capture_full_desktop_);
  if (displays->has_primary_screen_id()) {
    layout.set_primary_screen_id(displays->primary_screen_id());
  }
  protocol::VideoTrackLayout* video_track;

  // For single-stream clients, the first layout must be the current webrtc
  // capture size. This is required because we reuse the same message for both
  // VideoSizeChanged (which is used to scale mouse coordinates) and
  // DisplayDesktopChanged. Multi-stream clients will ignore the legacy layout
  // message, except that the width/height must be non-zero (the first
  // display-changed event may occur before |webrtc_capture_size_| becomes
  // non-zero).
  video_track = layout.add_video_track();
  video_track->set_position_x(0);
  video_track->set_position_y(0);
  video_track->set_width(
      multiStreamEnabled ? 1 : webrtc_capture_size_.WidthAsDips());
  video_track->set_height(
      multiStreamEnabled ? 1 : webrtc_capture_size_.HeightAsDips());
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);
  LOG(INFO) << "  Webrtc capture size (DIPS) = 0,0 "
            << default_webrtc_desktop_size_;

  // Add raw geometry for entire desktop (in DIPs).
  video_track = layout.add_video_track();
  video_track->set_position_x(0);
  video_track->set_position_y(0);
  video_track->set_width(size.width());
  video_track->set_height(size.height());
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);
  LOG(INFO) << "  Full Desktop (DIPS) = 0,0 " << size.width() << "x"
            << size.height() << " [" << dpi_x << "," << dpi_y << "]";

  // Add a VideoTrackLayout entry for each separate display.
  desktop_display_info_.Reset();
  for (int display_id = 0; display_id < displays->video_track_size();
       display_id++) {
    protocol::VideoTrackLayout display = displays->video_track(display_id);
    desktop_display_info_.AddDisplayFrom(display);

    video_track = layout.add_video_track();
    video_track->CopyFrom(display);
    if (multiStreamEnabled) {
      video_track->set_media_stream_id(
          protocol::WebrtcVideoStream::StreamNameForId(display.screen_id()));
    }

    LOG(INFO) << "  Display " << display_id << " = " << display.position_x()
              << "," << display.position_y() << " " << display.width() << "x"
              << display.height() << " [" << display.x_dpi() << ","
              << display.y_dpi() << "], screen_id=" << display.screen_id();
  }

  // Set the display index, if this is the first message being processed or if
  // the selected display no longer exists.
  if (!IsValidDisplayIndex(selected_display_index_)) {
    if (can_capture_full_desktop_) {
      selected_display_index_ = webrtc::kFullDesktopScreenId;
    } else {
      // Select the default display.
      protocol::SelectDesktopDisplayRequest req;
      req.set_id("0");
      SelectDesktopDisplay(req);
    }
  }

  // We need to update the input filters whenever the displays change.
  fractional_input_filter_.set_video_layout(*displays);
  UpdateFractionalFilterFallback();
  DisplaySize display_size =
      DisplaySize::FromPixels(size.width(), size.height(), default_x_dpi_);
  SetMouseClampingFilter(display_size);

  connection_->client_stub()->SetVideoLayout(layout);

  // If multi-stream is enabled, create and remove video-streams to match the
  // new list of displays.
  if (multiStreamEnabled) {
    CreatePerMonitorVideoStreams();
  }
}

void ClientSession::OnDesktopAttached(uint32_t session_id) {
  if (remote_webauthn_message_handler_) {
    // On Windows, only processes running on an attached desktop session can
    // bind ChromotingHostServices, so we notify the extension that it might be
    // able to connect now.
    remote_webauthn_message_handler_->NotifyWebAuthnStateChange();
  }
}

void ClientSession::OnDesktopDetached() {
  // Clear ChromotingSessionServices receivers and all other receivers brokered
  // by ChromotingSessionServices, as they are scoped to desktop session that
  // is being detached.
  // TODO(yuweih): If we decide to start the IPC server per remote session, then
  // we may just stop the server here instead, which will automatically
  // disconnect all ongoing IPCs.
  session_services_receivers_.Clear();
  if (remote_webauthn_message_handler_) {
    remote_webauthn_message_handler_->ClearReceivers();
    remote_webauthn_message_handler_->NotifyWebAuthnStateChange();
  }
  if (remote_open_url_message_handler_) {
    remote_open_url_message_handler_->ClearReceivers();
  }
}

void ClientSession::CreateFileTransferMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  // FileTransferMessageHandler manages its own lifetime and is tied to the
  // lifetime of |pipe|. Once |pipe| is closed, this instance will be cleaned
  // up.
  new FileTransferMessageHandler(channel_name, std::move(pipe),
                                 desktop_environment_->CreateFileOperations());
}

void ClientSession::CreateRtcLogTransferMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  new FileTransferMessageHandler(
      channel_name, std::move(pipe),
      std::make_unique<RtcLogFileOperations>(connection_.get()));
}

void ClientSession::CreateActionMessageHandler(
    std::vector<ActionRequest::Action> capabilities,
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  std::unique_ptr<ActionExecutor> action_executor =
      desktop_environment_->CreateActionExecutor();
  if (!action_executor) {
    return;
  }

  // ActionMessageHandler manages its own lifetime and is tied to the lifetime
  // of |pipe|. Once |pipe| is closed, this instance will be cleaned up.
  new ActionMessageHandler(channel_name, capabilities, std::move(pipe),
                           std::move(action_executor));
}

void ClientSession::CreateRemoteOpenUrlMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  // RemoteOpenUrlMessageHandler manages its own lifetime and is tied to the
  // lifetime of |pipe|. Once |pipe| is closed, this instance will be cleaned
  // up.
  auto* unowned_handler =
      new RemoteOpenUrlMessageHandler(channel_name, std::move(pipe));
  remote_open_url_message_handler_ = unowned_handler->GetWeakPtr();
}

void ClientSession::CreateUrlForwarderControlMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  // UrlForwarderControlMessageHandler manages its own lifetime and is tied to
  // the lifetime of |pipe|. Once |pipe| is closed, this instance will be
  // cleaned up.
  new UrlForwarderControlMessageHandler(
      desktop_environment_->CreateUrlForwarderConfigurator(), channel_name,
      std::move(pipe));
}

void ClientSession::CreateRemoteWebAuthnMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  // RemoteWebAuthnMessageHandler manages its own lifetime and is tied to the
  // lifetime of |pipe|. Once |pipe| is closed, this instance will be cleaned
  // up.
  auto* unowned_handler = new RemoteWebAuthnMessageHandler(
      channel_name, std::move(pipe),
      desktop_environment_->CreateRemoteWebAuthnStateChangeNotifier());
  remote_webauthn_message_handler_ = unowned_handler->GetWeakPtr();
}

bool ClientSession::IsValidDisplayIndex(webrtc::ScreenId index) const {
  return index == webrtc::kFullDesktopScreenId ||
         desktop_display_info_.GetDisplayInfo(index) != nullptr;
}

void ClientSession::BoostFramerateOnInput(
    base::TimeDelta capture_interval,
    base::TimeDelta boost_duration,
    bool& mouse_button_down,
    protocol::ObservingInputFilter::Event event) {
  // Boost the framerate when we see input which is likely to trigger a change
  // on the screen. This includes key, text, and touch events as well as mouse
  // scroll or mouse moves when a button is down.
  auto* mouse_event_ptr =
      absl::get_if<std::reference_wrapper<const protocol::MouseEvent>>(&event);
  if (mouse_event_ptr) {
    const protocol::MouseEvent& mouse_event = mouse_event_ptr->get();
    if (!mouse_button_down && !mouse_event.has_button() &&
        !mouse_event.has_wheel_delta_x() && !mouse_event.has_wheel_delta_y()) {
      return;
    }

    if (mouse_event.has_button()) {
      // The |button| field is only set when the state changes so we must store
      // the current value so we know whether to boost the framerate when we see
      // a mouse move event.
      mouse_button_down = mouse_event.button_down();
    }
  }

  for (const auto& [_, video_stream] : video_streams_) {
    // TODO(joedow): Consider boosting the capture rate for the active desktop
    // instead of all desktops in multi-stream mode.
    video_stream->BoostFramerate(capture_interval, boost_duration);
  }
}

void ClientSession::OnActiveDisplayChanged(webrtc::ScreenId display) {
  protocol::ActiveDisplay active_display;
  active_display.set_screen_id(display);
  connection_->client_stub()->SetActiveDisplay(active_display);
}

void ClientSession::UpdateFractionalFilterFallback() {
  if (!IsValidDisplayIndex(selected_display_index_)) {
    return;
  }

  webrtc::DesktopSize new_size;
  if (selected_display_index_ == webrtc::kFullDesktopScreenId) {
#if BUILDFLAG(IS_APPLE)
    // On macOS, for full-desktop capture, the capturer's current frame size
    // should be used. This is because the capturer may revert to capturing from
    // the default display instead of the full desktop. This could happen if all
    // monitors had matching DPIs and full-desktop-capture was previously
    // supported, but a monitor mode was changed such that the DPIs no longer
    // match.
    new_size = {webrtc_capture_size_.WidthAsDips(),
                webrtc_capture_size_.HeightAsDips()};
#else
    // For other platforms, use the video-layout, as the rectangles are already
    // in the correct units (pixels/DIPs) for input-injection.
    webrtc::DesktopRect rect;
    for (int i = 0; i < desktop_display_info_.NumDisplays(); i++) {
      const DisplayGeometry* geo = desktop_display_info_.GetDisplayInfo(i);
      rect.UnionWith(webrtc::DesktopRect::MakeXYWH(geo->x, geo->y, geo->width,
                                                   geo->height));
    }
    new_size = rect.size();
#endif  // BUILDFLAG(IS_APPLE)
  } else {
    const DisplayGeometry* geo =
        desktop_display_info_.GetDisplayInfo(selected_display_index_);

    new_size = webrtc::DesktopSize(geo->width, geo->height);
  }

  // The logic for input-injection offsets is dependent on the OS, and is
  // implemented in DesktopDisplayInfo::CalcDisplayOffset().
  webrtc::DesktopVector offset =
      desktop_display_info_.CalcDisplayOffset(selected_display_index_);
  fractional_input_filter_.set_fallback_geometry(
      webrtc::DesktopRect::MakeOriginSize(offset, new_size));
}

}  // namespace remoting
