// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/base/capabilities.h"
#include "remoting/base/constants.h"
#include "remoting/base/errors.h"
#include "remoting/base/fifo_buffer.h"
#include "remoting/base/ipc_fifo_buffer.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging.h"
#include "remoting/base/session_options.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/action_message_handler.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/file_transfer/file_transfer_message_handler.h"
#include "remoting/host/file_transfer/rtc_log_file_operations.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/host_extension_session_manager.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/mouse_shape_pump.h"
#include "remoting/host/remote_open_url/remote_open_url_constants.h"
#include "remoting/host/remote_open_url/remote_open_url_message_handler.h"
#include "remoting/host/remote_open_url/remote_open_url_util.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/remote_open_url/url_forwarder_control_message_handler.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/security_key/security_key_data_channel_handler.h"
#include "remoting/host/security_key/security_key_extension.h"
#include "remoting/host/security_key/security_key_extension_session.h"
#include "remoting/host/terminal_session_manager.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"
#include "remoting/host/webauthn/remote_webauthn_message_handler.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/audio_sample_info.h"
#include "remoting/protocol/audio_stream.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_thread_proxy.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/data_channel_manager.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_event_timestamps.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/keyboard_layout_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/observing_input_filter.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/peer_connection_controls.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/webrtc_video_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "ui/events/types/event_type.h"

namespace {

constexpr char kRtcLogTransferDataChannelPrefix[] = "rtc-log-transfer-";

constexpr base::TimeDelta kDefaultBoostCaptureInterval = base::Milliseconds(5);
constexpr base::TimeDelta kDefaultBoostDuration = base::Milliseconds(50);

std::string_view PixelTypeToString(
    remoting::protocol::VideoLayout::PixelType pixel_type) {
  switch (pixel_type) {
    case remoting::protocol::VideoLayout_PixelType_LOGICAL:
      return "DIPs";
    case remoting::protocol::VideoLayout_PixelType_PHYSICAL:
      return "Physical pixels";
    default:
      return "Unknown pixel type";
  }
}

void LogVideoTrack(int index,
                   const remoting::protocol::VideoTrackLayout& track) {
  HOST_LOG << "  track " << index << ": "
           << "id="
           << (track.has_screen_id() ? base::NumberToString(track.screen_id())
                                     : "[none]")
           << ", name='" << track.display_name()
           << "', pos=" << track.position_x() << "," << track.position_y()
           << ", " << track.width() << "x" << track.height() << ", dpi=["
           << track.x_dpi() << "," << track.y_dpi() << "]";
}

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
      host_clipboard_filter_(clipboard_echo_filter_.host_filter()),
      client_clipboard_filter_(clipboard_echo_filter_.client_filter()),
      client_clipboard_factory_(&client_clipboard_filter_),
      input_pipeline_(&coordinate_converter_, this),
      pairing_registry_(pairing_registry),
      connection_(std::move(connection)),
      client_jid_(connection_->session()->jid()),
      local_session_policies_provider_(local_session_policies_provider) {
  connection_->session()->AddPlugin(&host_experiment_session_plugin_);
  connection_->SetEventHandler(this);

  HostExtensionSessionManager::HostExtensions all_extensions = extensions;
  bool gnubby_policy_enabled = true;
  if (local_session_policies_provider) {
    gnubby_policy_enabled =
        local_session_policies_provider->get_local_policies()
            .allow_gnubby_forwarding.value_or(true);
  }
  // TODO(b/517007701): Create SecurityKeyAuthHandler after authentication once
  // we have completed the data channel migration.
  if (desktop_environment_options.enable_security_key() &&
      gnubby_policy_enabled) {
    security_key_auth_handler_ = SecurityKeyAuthHandler::Create(this);
    if (security_key_auth_handler_) {
      security_key_extension_ = std::make_unique<SecurityKeyExtension>(
          security_key_auth_handler_->GetWeakPtr());
      all_extensions.push_back(security_key_extension_.get());
    }
  }

  // Create a manager for the configured extensions, if any.
  extension_manager_ =
      std::make_unique<HostExtensionSessionManager>(all_extensions, this);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  // LocalMouseInputMonitorWin and LocalPointerInputMonitorChromeos filter out
  // an echo of the injected input before it reaches `remote_input_filter_`.
  input_pipeline_.remote_input_filter()->SetExpectLocalEcho(false);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
}

ClientSession::~ClientSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!audio_stream_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!terminal_session_manager_);
  DCHECK(video_streams_.empty());
}

void ClientSession::NotifyClientResolution(
    const protocol::ClientResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (resolution.width_pixels() < 0 || resolution.height_pixels() < 0) {
    LOG(ERROR) << "Bad ClientResolution: " << resolution.width_pixels() << "x"
               << resolution.height_pixels();
    return;
  }
  HOST_LOG << "Received ClientResolution (width=" << resolution.width_pixels()
           << ", height=" << resolution.height_pixels()
           << ", x_dpi=" << resolution.x_dpi()
           << ", y_dpi=" << resolution.y_dpi() << ", screen_id="
           << (resolution.has_screen_id()
                   ? base::NumberToString(resolution.screen_id())
                   : "[none]")
           << ")";

  if (!screen_controls_) {
    return;
  }

  webrtc::DesktopSize client_size(resolution.width_pixels(),
                                  resolution.height_pixels());

  // TODO(joedow): Determine if other platforms support desktop scaling.
  webrtc::DesktopVector dpi_vector{kDefaultDpi, kDefaultDpi};
#if BUILDFLAG(IS_WIN)
  // Matching the client DPI is only supported on Windows when curtained.
  if (effective_policies_.curtain_required.value_or(false)) {
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
      input_pipeline_.observing_input_filter()->ClearInputEventCallback();
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

      // Unretained is sound as this instance owns `input_pipeline_`.
      input_pipeline_.observing_input_filter()->SetInputEventCallback(
          base::BindRepeating(&ClientSession::BoostFramerateOnInput,
                              base::Unretained(this), capture_interval,
                              boost_duration, base::OwnedRef(false)));
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

  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(
        base::BindOnce(&ClientSession::SetCapabilities,
                       weak_factory_.GetWeakPtr(), capabilities));
    return;
  }

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

  if (HasCapability(capabilities_, protocol::kMicrophoneRemotingCapability) &&
      !audio_injector_) {
    CreateAudioInjectorAndBuffer();
  }

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

  if (HasCapability(capabilities_, protocol::kSecurityKeyV2Capability)) {
    data_channel_manager_.RegisterCreateHandlerCallback(
        SecurityKeyDataChannelHandler::kChannelName,
        base::BindRepeating(&ClientSession::CreateSecurityKeyDataChannelHandler,
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

  host_cursor_rendered_by_client_ = HasCapability(
      capabilities_, protocol::kClientRenderedHostCursorCapability);
  if (host_cursor_rendered_by_client_ && cursor_visible_) {
    // OnCursorVisibilityChanged(true) could have been called with
    // `host_cursor_rendered_by_client_` being false, e.g., if the IT2ME
    // helpee moves the cursor before the session is connected, so we call it
    // again with the updated boolean, which updates MouseShapePump to send the
    // cursor position to the client.
    OnCursorVisibilityChanged(true);
    // OnCursorVisibilityChanged(true) does not hide the host-rendered cursor if
    // `host_cursor_rendered_by_client_` is true, so we need to call
    // SetComposeEnabledOnVideoStreams(false) to explicitly hide it.
    SetComposeEnabledOnVideoStreams(false);
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

  // Multi-stream is enabled on all platforms so this protocol request is no
  // longer meaningful.
  LOG(WARNING) << "Ignoring deprecated SelectDesktopDisplayRequest.";
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
  for (int i = 0; i < video_layout.video_track_size(); i++) {
    const auto& track = video_layout.video_track(i);
    if (track.width() < 0 || track.height() < 0) {
      LOG(ERROR) << "Bad VideoLayout for track " << track.screen_id() << ": "
                 << track.width() << "x" << track.height();
      return;
    }
  }
  HOST_LOG << "Received VideoLayout ("
           << PixelTypeToString(video_layout.pixel_type()) << ", primary_id="
           << (video_layout.has_primary_screen_id()
                   ? base::NumberToString(video_layout.primary_screen_id())
                   : "[none]")
           << ")";
  for (int i = 0; i < video_layout.video_track_size(); i++) {
    LogVideoTrack(i, video_layout.video_track(i));
  }
  screen_controls_->SetVideoLayout(video_layout);
}

void ClientSession::ControlTerminal(
    const protocol::TerminalControl& terminal_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasCapability(capabilities_, protocol::kTerminalModeCapability)) {
    return;
  }
  if (!terminal_session_manager_) {
    terminal_session_manager_ = std::make_unique<TerminalSessionManager>();
  }

  if (terminal_control.has_create_request()) {
    // Create a new terminal session and store the ID. We'll use this ID to
    // identify the terminal session when sending output to the client. Bind the
    // callbacks to the weak factory to ensure that the callbacks are not
    // called after the client session is disconnected.
    int32_t id = terminal_session_manager_->CreateTerminal(
        base::BindRepeating(&ClientSession::SendTerminalOutput,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&ClientSession::OnTerminalExited,
                       weak_factory_.GetWeakPtr()));

    protocol::TerminalControl response;
    auto* create_response = response.mutable_create_response();
    if (id != -1) {
      create_response->set_terminal_id(id);
    } else {
      create_response->mutable_error()->set_reason(
          protocol::TerminalControl::CreateTerminalResponse::Error::FAILED);
    }
    connection_->client_stub()->DeliverTerminalControl(response);

  } else if (terminal_control.has_terminal_input()) {
    const auto& input = terminal_control.terminal_input();
    terminal_session_manager_->WriteTerminal(input.terminal_id(),
                                             input.input());

  } else if (terminal_control.has_resize_terminal()) {
    const auto& resize = terminal_control.resize_terminal();
    terminal_session_manager_->ResizeTerminal(resize.terminal_id(),
                                              resize.width(), resize.height());

  } else if (terminal_control.has_remove_request()) {
    int32_t terminal_id = terminal_control.remove_request().terminal_id();
    terminal_session_manager_->CloseTerminal(terminal_id);
  }
}

void ClientSession::SendTerminalOutput(int32_t terminal_id,
                                       const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol::TerminalControl response;
  auto* output = response.mutable_terminal_output();
  output->set_terminal_id(terminal_id);
  output->set_output(data);
  connection_->client_stub()->DeliverTerminalControl(response);
}

void ClientSession::OnTerminalExited(int32_t terminal_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol::TerminalControl response;
  response.mutable_close_terminal()->set_terminal_id(terminal_id);
  connection_->client_stub()->DeliverTerminalControl(response);
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

  std::optional<ErrorCode> validation_result =
      event_handler_->OnSessionPoliciesReceived(effective_policies_);

  if (validation_result.has_value()) {
    // TODO: crbug.com/382334458 - Include error details and location in the
    // validation result.
    std::string error_details = base::StringPrintf(
        "Session policies disallowed by validator. Error code: %d",
        static_cast<int>(*validation_result));
    DisconnectSession(*validation_result, error_details, FROM_HERE);
    return;
  }

  base::TimeDelta max_duration =
      effective_policies_.maximum_session_duration.value_or(base::TimeDelta());
  if (max_duration.is_positive()) {
    max_duration_timer_.Start(
        FROM_HERE, max_duration,
        base::BindOnce(&ClientSession::DisconnectSession,
                       base::Unretained(this), ErrorCode::MAX_SESSION_LENGTH,
                       "Maximum session duration has been reached.",
                       FROM_HERE));
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
  // `allow_webauthn_forwarding` should not override the existing value for
  // `enable_remote_webauthn` if it was not enabled for this connection mode.
  if (options.enable_remote_webauthn() &&
      effective_policies_.allow_webauthn_forwarding.has_value()) {
    options.set_enable_remote_webauthn(
        *effective_policies_.allow_webauthn_forwarding);
  }
  if (options.enable_security_key() &&
      effective_policies_.allow_gnubby_forwarding.has_value()) {
    options.set_enable_security_key(
        *effective_policies_.allow_gnubby_forwarding);
  }
  // Create the desktop environment.
  // Note: The handlers for various other events use the created desktop
  // environment. Since those events may occur before the desktop environment
  // creation has finished, each such event handler must include a prologue to
  // check if the desktop environment has been created, and add itself to a list
  // of deferred handlers if not.
  // TODO(rkjnsn): During a future refactor, see if this can be improved. E.g.,
  // perhaps ensuring at a higher layer that additional events don't occur until
  // the ClientSession is ready, or using co_await (once approved in Chromium)
  // to wait for the desktop environment more simply and safely when it is used.
  desktop_environment_factory_->Create(
      weak_factory_.GetWeakPtr(), weak_factory_.GetWeakPtr(), options,
      base::BindOnce(&ClientSession::OnDesktopEnvironmentCreated,
                     weak_factory_.GetWeakPtr()));
}

void ClientSession::CreateMediaStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(base::BindOnce(
        &ClientSession::CreateMediaStreams, weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(video_streams_.empty());

  AudioPlaybackMode audio_playback_mode =
      desktop_environment_options_.audio_playback_mode();
  if (audio_playback_mode == AudioPlaybackMode::kRemoteAndLocal ||
      audio_playback_mode == AudioPlaybackMode::kRemoteOnly) {
    // Create an AudioStream to pump audio from the capturer to the client.
    std::unique_ptr<AudioCapturer> audio_capturer =
        desktop_environment_->CreateAudioCapturer();
    if (audio_capturer) {
#if BUILDFLAG(IS_CHROMEOS)
      audio_capturer->SetAudioPlaybackMode(audio_playback_mode);
#endif
      audio_stream_ = connection_->StartAudioStream(std::move(audio_capturer));
    }
  }

  // Single-stream is no longer supported on any platform, so create the
  // per-monitor streams immediately.
  CreatePerMonitorVideoStreams();
}

void ClientSession::CreatePerMonitorVideoStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create new streams for any monitors that don't already have streams.
  for (int i = 0; i < desktop_display_info_.NumDisplays(); i++) {
    auto id = desktop_display_info_.GetDisplayInfo(i)->id;

    if (video_streams_.contains(id)) {
      HOST_LOG << "Video stream for id " << id << " already exists.";
      continue;
    }

    HOST_LOG << "Creating video stream for id " << id;

    auto video_capturer = desktop_environment_->CreateVideoCapturer(id);
    if (!video_capturer) {
      LOG(WARNING) << "Cannot create video capturer for id " << id;
      continue;
    }
    auto video_stream =
        connection_->StartVideoStream(id, std::move(video_capturer));

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
    bool keep = std::ranges::contains(
        displays, id, [](const DisplayGeometry& geo) { return geo.id; });
    HOST_LOG << (keep ? "Keeping" : "Removing") << " video stream for id "
             << id;
    return !keep;
  });
}

void ClientSession::OnConnectionChannelsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(
        base::BindOnce(&ClientSession::OnConnectionChannelsConnected,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(!channels_connected_);
  channels_connected_ = true;

  if (pending_audio_writer_) {
    connection_->SetAudioWriter(std::move(pending_audio_writer_));
  }

  // Negotiate capabilities with the client.
  VLOG(1) << "Host capabilities: " << host_capabilities_;
  protocol::Capabilities capabilities;
  capabilities.set_capabilities(host_capabilities_);
  connection_->client_stub()->SetCapabilities(capabilities);

  // Start the event executor.
  // TODO: crbug.com/406740794 - Decouple clipboard and input controls.
  // Clipboard synchronization and remote input are controlled via two separate
  // policies. Currently the code has them intertwined together and it is hard
  // to disable one without disabling the other. These should be separated.
  if (effective_policies_.allow_remote_input.value_or(true)) {
    input_injector_->Start(CreateClipboardProxy());
    SetDisableInputs(false);
  } else {
    SetDisableInputs(true);
  }

  // Create MouseShapePump to send mouse cursor shape.
  mouse_shape_pump_ = std::make_unique<MouseShapePump>(
      desktop_environment_->CreateMouseCursorMonitor(),
      connection_->client_stub());
  mouse_shape_pump_->SetMouseCursorMonitorCallback(this);
  mouse_shape_pump_->SetCursorCaptureInterval(base::Hertz(target_framerate_));
  mouse_shape_pump_->SetSendCursorPositionToClient(
      host_cursor_rendered_by_client_ && cursor_visible_);

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
    input_pipeline_.input_tracker()->ReleaseAll();

    // Avoid dangling raw_ptr in `input_pipeline_` after deleting
    // `input_injector_` below.
    input_pipeline_.SetInputStub(nullptr);
  }

  // Stop components access the client, audio or video stubs, which are no
  // longer valid once ConnectionToClient calls OnConnectionClosed().
  audio_stream_.reset();
  audio_injector_.reset();
  mouse_shape_pump_.reset();
  video_streams_.clear();
  keyboard_layout_monitor_.reset();
  client_clipboard_factory_.InvalidateWeakPtrs();
  input_injector_.reset();
  screen_controls_.reset();
  desktop_environment_.reset();
  terminal_session_manager_.reset();

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

void ClientSession::OnIncomingAudioFormatChanged(
    const protocol::AudioSampleInfo& info,
    base::OnceCallback<void(bool)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_injector_) {
    audio_injector_->SetSampleInfo(info, std::move(done));
  } else {
    if (pending_audio_format_ack_callback_) {
      std::move(pending_audio_format_ack_callback_).Run(false);
    }
    pending_audio_sample_info_ = info;
    pending_audio_format_ack_callback_ = std::move(done);
  }
}

const std::string& ClientSession::client_jid() const {
  return client_jid_;
}

void ClientSession::DisconnectSession(ErrorCode error,
                                      std::string_view error_details,
                                      const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_.get());

  max_duration_timer_.Stop();

  // This triggers OnConnectionClosed(), and the session may be destroyed
  // as the result, so this call must be the last in this method.
  connection_->Disconnect(error, error_details, error_location);
}

void ClientSession::OnLocalKeyPressed(std::uint32_t usb_keycode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_local =
      input_pipeline_.remote_input_filter()->LocalKeyPressed(usb_keycode);
  if (is_local && desktop_environment_options_.terminate_upon_input()) {
    DisconnectSession(
        ErrorCode::OK,
        "Disconnecting CRD session because local keyboard input was detected.",
        FROM_HERE);
  }
}

void ClientSession::OnLocalPointerMoved(const webrtc::DesktopVector& position,
                                        ui::EventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_local =
      input_pipeline_.remote_input_filter()->LocalPointerMoved(position, type);
  if (is_local) {
    if (desktop_environment_options_.terminate_upon_input()) {
      DisconnectSession(
          ErrorCode::OK,
          "Disconnecting CRD session because local mouse input was detected.",
          FROM_HERE);
    } else {
      input_pipeline_.cursor_visibility_notifier()->OnLocalInput();
    }
  }
}

void ClientSession::SetDisableInputs(bool disable_inputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disable_inputs) {
    input_pipeline_.input_tracker()->ReleaseAll();
  }

  input_pipeline_.disable_input_filter()->set_enabled(!disable_inputs);
  host_clipboard_filter_.set_enabled(!disable_inputs);
}

void ClientSession::OnSessionServicesClientConnected(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  session_services_receivers_.Add(this, std::move(receiver));
}

ClientSessionControl* ClientSession::session_control() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

void ClientSession::OnCursorVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_visible_ = visible;
  if (host_cursor_rendered_by_client_) {
    if (mouse_shape_pump_) {
      mouse_shape_pump_->SetSendCursorPositionToClient(cursor_visible_);
    }
  } else {
    SetComposeEnabledOnVideoStreams(visible);
  }
}

void ClientSession::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(*mouse_cursor)));
  }
}

void ClientSession::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (host_cursor_rendered_by_client_) {
    // The following code is for updating the cursor position in
    // DesktopAndCursorComposer. If the host cursor is rendered by the client,
    // then we don't need to do that.
    return;
  }

  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetMouseCursorPosition(position);
  }
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
  OnSecurityKeyConnection(std::move(receiver));
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

void ClientSession::OnDesktopEnvironmentCreated(
    std::unique_ptr<DesktopEnvironment> desktop_environment) {
  // Drop the connection if it could not be created for any reason (for instance
  // the curtain could not initialize).
  if (!desktop_environment) {
    DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR,
                      "Failed to create desktop environment.", FROM_HERE);
    return;
  }
  desktop_environment_ = std::move(desktop_environment);

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

  host_capabilities_.append(" ");
  host_capabilities_.append(protocol::kClientRenderedHostCursorCapability);
  if (security_key_auth_handler_) {
    host_capabilities_.append(" ");
    host_capabilities_.append(protocol::kSecurityKeyV2Capability);
  }

  host_capabilities_.append(" ");
  host_capabilities_.append(protocol::kTerminalModeCapability);

  // Create the object that controls the screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();

  // Create the event executor.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Connect the host input stubs.
  connection_->set_input_stub(&input_pipeline_);
  input_pipeline_.SetInputStub(input_injector_.get());

  if (effective_policies_.clipboard_size_bytes.has_value()) {
    int max_size = *effective_policies_.clipboard_size_bytes;

    client_clipboard_filter_.set_max_size(max_size);
    host_clipboard_filter_.set_max_size(max_size);
  }

  // Connect the clipboard stubs.
  connection_->set_clipboard_stub(&host_clipboard_filter_);
  clipboard_echo_filter_.set_host_stub(input_injector_.get());
  clipboard_echo_filter_.set_client_stub(connection_->client_stub());

  // Execute any pending events that require the desktop environment.
  for (auto& callback : desktop_environment_ready_callbacks_) {
    std::move(callback).Run();
  }
  desktop_environment_ready_callbacks_.clear();
}

void ClientSession::CreateAudioInjectorAndBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<IpcFifoBufferWriter> writer;
  std::unique_ptr<IpcFifoBufferReader> reader;

  // Please see the documentation for DesktopEnvironment::CreateAudioInjector,
  // which explains why we always use IpcFifoBuffer.
  if (CreateIpcFifoBuffer(kDefaultFifoBufferCapacity, writer, reader)) {
    audio_injector_ =
        desktop_environment_->CreateAudioInjector(std::move(reader));
    if (audio_injector_) {
      if (pending_audio_sample_info_) {
        base::OnceCallback<void(bool)> done =
            pending_audio_format_ack_callback_
                ? std::move(pending_audio_format_ack_callback_)
                : base::DoNothing();
        audio_injector_->SetSampleInfo(*pending_audio_sample_info_,
                                       std::move(done));
        pending_audio_sample_info_.reset();
      }
      audio_injector_->Start(weak_factory_.GetWeakPtr());
      if (channels_connected_) {
        connection_->SetAudioWriter(std::move(writer));
      } else {
        pending_audio_writer_ = std::move(writer);
      }
    }
  }
}

void ClientSession::OnLocalSessionPoliciesChanged(
    const SessionPolicies& new_policies) {
  DCHECK(local_session_policy_update_subscription_);
  DisconnectSession(ErrorCode::SESSION_POLICIES_CHANGED,
                    "Effective policies have changed. Terminating session.",
                    FROM_HERE);
}

void ClientSession::OnDesktopDisplayChanged(
    std::unique_ptr<protocol::VideoLayout> displays) {
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(
        base::BindOnce(&ClientSession::OnDesktopDisplayChanged,
                       weak_factory_.GetWeakPtr(), std::move(displays)));
    return;
  }

  HOST_LOG << "ClientSession::OnDesktopDisplayChanged";

  // Scan display list to calculate the full desktop size.
  int min_x = 0;
  int max_x = 0;
  int min_y = 0;
  int max_y = 0;
  int dpi_x = 0;
  int dpi_y = 0;
  std::string_view dips_or_physical_pixels =
      PixelTypeToString(displays->pixel_type());
  HOST_LOG << "Scanning display info... (" << dips_or_physical_pixels
           << ", primary_id="
           << (displays->has_primary_screen_id()
                   ? base::NumberToString(displays->primary_screen_id())
                   : "[none]")
           << ")";
  for (int display_id = 0; display_id < displays->video_track_size();
       display_id++) {
    const protocol::VideoTrackLayout& track = displays->video_track(display_id);
    LogVideoTrack(display_id, track);
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

  // Calc desktop scaled geometry
  const webrtc::DesktopSize size(max_x - min_x, max_y - min_y);

  // Generate and send VideoLayout message.
  protocol::VideoLayout layout;
  if (displays->has_pixel_type()) {
    layout.set_pixel_type(displays->pixel_type());
  }

  if (displays->has_primary_screen_id()) {
    layout.set_primary_screen_id(displays->primary_screen_id());
  }
  protocol::VideoTrackLayout* video_track;

  // The first two tracks form part of the legacy layout message for
  // single-stream clients. Multi-stream clients will ignore the legacy layout
  // message, except that the width/height must be non-zero.
  video_track = layout.add_video_track();
  video_track->set_position_x(0);
  video_track->set_position_y(0);
  video_track->set_width(1);
  video_track->set_height(1);
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);

  // Add raw geometry for entire desktop.
  video_track = layout.add_video_track();
  video_track->set_position_x(0);
  video_track->set_position_y(0);
  video_track->set_width(size.width());
  video_track->set_height(size.height());
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);
  HOST_LOG << "Full Desktop (" << dips_or_physical_pixels << ") = 0,0 "
           << size.width() << "x" << size.height() << ", dpi=[" << dpi_x << ","
           << dpi_y << "]";

  desktop_display_info_.CopyFromVideoLayoutProto(*displays);

  // Add a VideoTrackLayout entry for each separate display.
  for (int display_id = 0; display_id < displays->video_track_size();
       display_id++) {
    protocol::VideoTrackLayout display = displays->video_track(display_id);
    video_track = layout.add_video_track();
    video_track->CopyFrom(display);
    video_track->set_media_stream_id(
        protocol::WebrtcVideoStream::StreamNameForId(display.screen_id()));

    LogVideoTrack(display_id, display);
  }

  // We need to update the coordinate converter whenever the displays change.
  coordinate_converter_.set_video_layout(*displays);

  connection_->client_stub()->SetVideoLayout(layout);

  // Create and remove video-streams to match the new list of displays.
  CreatePerMonitorVideoStreams();
}

// This method is used by multi-process hosts, and single-process hosts via
// OnAudioInjectorConsumersChanged.
void ClientSession::OnMicrophoneControl(
    const protocol::MicrophoneControl& control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (channels_connected_) {
    connection_->client_stub()->ControlMicrophone(control);
  }
}

// This method is used by single-process hosts.
void ClientSession::OnAudioInjectorConsumersChanged(bool has_consumers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  protocol::MicrophoneControl control;
  control.set_enable(has_consumers);
  OnMicrophoneControl(control);
}

void ClientSession::OnDesktopAttached() {
  if (remote_webauthn_message_handler_) {
    // On Windows, only processes running on an attached desktop session can
    // bind ChromotingHostServices, so we notify the extension that it might be
    // able to connect now.
    remote_webauthn_message_handler_->NotifyWebAuthnStateChange();
  }

  if (HasCapability(capabilities_, protocol::kMicrophoneRemotingCapability) &&
      !audio_injector_) {
    CreateAudioInjectorAndBuffer();
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

  audio_injector_.reset();
}

void ClientSession::OnSecurityKeyConnection(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool allow_gnubby =
      desktop_environment_options_.enable_security_key() &&
      effective_policies_.allow_gnubby_forwarding.value_or(true);

  if (!security_key_auth_handler_) {
    LOG(WARNING) << "Security key forwarding is not supported. Binding request "
                    "rejected.";
    return;
  }
  if (!allow_gnubby) {
    LOG(WARNING) << "Security key forwarding is disabled by policy or option. "
                 << "Binding request rejected.";
    return;
  }
  security_key_auth_handler_->BindSecurityKeyForwarder(std::move(receiver));
}

void ClientSession::CreateFileTransferMessageHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(base::BindOnce(
        &ClientSession::CreateFileTransferMessageHandler,
        weak_factory_.GetWeakPtr(), channel_name, std::move(pipe)));
    return;
  }
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
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(base::BindOnce(
        &ClientSession::CreateActionMessageHandler, weak_factory_.GetWeakPtr(),
        std::move(capabilities), channel_name, std::move(pipe)));
    return;
  }
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
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(base::BindOnce(
        &ClientSession::CreateUrlForwarderControlMessageHandler,
        weak_factory_.GetWeakPtr(), channel_name, std::move(pipe)));
    return;
  }
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
  if (!desktop_environment_) {
    desktop_environment_ready_callbacks_.push_back(base::BindOnce(
        &ClientSession::CreateRemoteWebAuthnMessageHandler,
        weak_factory_.GetWeakPtr(), channel_name, std::move(pipe)));
    return;
  }
  // RemoteWebAuthnMessageHandler manages its own lifetime and is tied to the
  // lifetime of |pipe|. Once |pipe| is closed, this instance will be cleaned
  // up.
  auto* unowned_handler = new RemoteWebAuthnMessageHandler(
      channel_name, std::move(pipe),
      desktop_environment_->CreateRemoteWebAuthnStateChangeNotifier());
  remote_webauthn_message_handler_ = unowned_handler->GetWeakPtr();
}

void ClientSession::CreateSecurityKeyDataChannelHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!security_key_auth_handler_) {
    LOG(WARNING) << "Security key auth handler not active.";
    return;
  }

  // Create a callback to destroy the legacy signaling extension session.
  // This will be invoked by the data channel handler once it has successfully
  // connected and registered its own callback, avoiding a race condition
  // where requests are dropped.
  base::OnceClosure takeover_callback =
      base::BindOnce(&ClientSession::DestroySecurityKeyExtensionSession,
                     weak_factory_.GetWeakPtr());

  // Instantiate the data channel handler.
  // It binds directly to the handler and registers its own callback, cleanly
  // taking over.
  new SecurityKeyDataChannelHandler(std::move(pipe),
                                    security_key_auth_handler_->GetWeakPtr(),
                                    std::move(takeover_callback));
}

void ClientSession::DestroySecurityKeyExtensionSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Destroying legacy security key extension session (takeover).";
  extension_manager_->RemoveExtensionSession(SecurityKeyExtension::kCapability);
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
      std::get_if<std::reference_wrapper<const protocol::MouseEvent>>(&event);
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

void ClientSession::SetComposeEnabledOnVideoStreams(bool enabled) {
  for (const auto& [_, video_stream] : video_streams_) {
    video_stream->SetComposeEnabled(enabled);
  }
}

}  // namespace remoting
