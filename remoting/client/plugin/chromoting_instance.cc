// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_instance.h"

#include <nacl_io/nacl_io.h>
#include <sys/mount.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "crypto/random.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/ssl_server_socket.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/private/uma_private.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "remoting/base/constants.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/input/normalizing_input_filter_cros.h"
#include "remoting/client/input/normalizing_input_filter_mac.h"
#include "remoting/client/input/normalizing_input_filter_win.h"
#include "remoting/client/plugin/pepper_audio_player.h"
#include "remoting/client/plugin/pepper_main_thread_task_runner.h"
#include "remoting/client/plugin/pepper_mouse_locker.h"
#include "remoting/client/plugin/pepper_port_allocator_factory.h"
#include "remoting/client/plugin/pepper_url_request.h"
#include "remoting/client/plugin/pepper_video_renderer_2d.h"
#include "remoting/client/plugin/pepper_video_renderer_3d.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/delegating_signal_strategy.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/rtc_base/helpers.h"
#include "third_party/webrtc/rtc_base/network.h"
#include "url/gurl.h"

namespace remoting {

namespace {

// Default DPI to assume for old clients that use notifyClientResolution.
const int kDefaultDPI = 96;

// Size of the random seed blob used to initialize RNG in libjingle. OpenSSL
// needs at least 32 bytes of entropy (see
// http://wiki.openssl.org/index.php/Random_Numbers), but stores 1039 bytes of
// state, so we initialize it with 1k or random data.
const int kRandomSeedSize = 1024;

// The connection times and duration values are stored in UMA custom-time
// histograms, that are log-scaled by default. The histogram specifications are
// based off values seen over a recent 7-day period.
// The connection times histograms are in milliseconds and the connection
// duration histograms are in minutes.
const char kTimeToAuthenticateHistogram[] =
    "Chromoting.Connections.Times.ToAuthenticate";
const char kTimeToConnectHistogram[] = "Chromoting.Connections.Times.ToConnect";
const char kClosedSessionDurationHistogram[] =
    "Chromoting.Connections.Durations.Closed";
const char kFailedSessionDurationHistogram[] =
    "Chromoting.Connections.Durations.Failed";
const int kConnectionTimesHistogramMinMs = 1;
const int kConnectionTimesHistogramMaxMs = 30000;
const int kConnectionTimesHistogramBuckets = 50;
const int kConnectionDurationHistogramMinMinutes = 1;
const int kConnectionDurationHistogramMaxMinutes = 24 * 60;
const int kConnectionDurationHistogramBuckets = 50;

// Input event latency is expected to be below 10ms.
const char kInputEventLatencyHistogram[] = "Chromoting.Input.EventLatency";
const int kInputEventLatencyHistogramMinUs = 1;
const int kInputEventLatencyHistogramMaxUs = 10000;
const int kInputEventLatencyHistogramBuckets = 50;

// Update perf stats in the UI every second.
const int kUIStatsUpdatePeriodSeconds = 1;

// TODO(sergeyu): Ideally we should just pass ErrorCode to the webapp
// and let it handle it, but it would be hard to fix it now because
// client plugin and webapp versions may not be in sync. It should be
// easy to do after we are finished moving the client plugin to NaCl.
std::string ConnectionErrorToString(protocol::ErrorCode error) {
  // Values returned by this function must match the
  // remoting.ClientSession.Error enum in JS code.
  switch (error) {
    case protocol::OK:
      return "NONE";

    case protocol::PEER_IS_OFFLINE:
      return "HOST_IS_OFFLINE";

    case protocol::SESSION_REJECTED:
    case protocol::AUTHENTICATION_FAILED:
      return "SESSION_REJECTED";

    case protocol::INVALID_ACCOUNT:
      return "INVALID_ACCOUNT";

    case protocol::INCOMPATIBLE_PROTOCOL:
      return "INCOMPATIBLE_PROTOCOL";

    case protocol::HOST_OVERLOAD:
      return "HOST_OVERLOAD";

    case protocol::MAX_SESSION_LENGTH:
      return "MAX_SESSION_LENGTH";

    case protocol::HOST_CONFIGURATION_ERROR:
      return "HOST_CONFIGURATION_ERROR";

    case protocol::CHANNEL_CONNECTION_ERROR:
    case protocol::SIGNALING_ERROR:
    case protocol::SIGNALING_TIMEOUT:
    case protocol::UNKNOWN_ERROR:
      return "NETWORK_FAILURE";
    default:
      return "UNKNOWN";
  }
}

PP_Instance g_logging_instance = 0;
base::LazyInstance<base::Lock>::Leaky g_logging_lock =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      initialized_(false),
      plugin_task_runner_(new PepperMainThreadTaskRunner()),
      context_(plugin_task_runner_.get()),
      touch_input_scaler_(&mouse_input_filter_),
      key_mapper_(&touch_input_scaler_),
      input_handler_(&input_tracker_),
      cursor_setter_(this),
      empty_cursor_filter_(&cursor_setter_),
      text_input_controller_(this),
      use_async_pin_dialog_(false),
      weak_factory_(this) {
  // In NaCl global resources need to be initialized differently because they
  // are not shared with Chrome.
  thread_task_runner_handle_.reset(
      new base::ThreadTaskRunnerHandle(plugin_task_runner_));
  thread_wrapper_ =
      jingle_glue::JingleThreadWrapper::WrapTaskRunner(plugin_task_runner_);

  // Register a global log handler.
  ChromotingInstance::RegisterLogMessageHandler();

  nacl_io_init_ppapi(pp_instance, pp::Module::Get()->get_browser_interface());
  mount("", "/etc", "memfs", 0, "");
  mount("", "/usr", "memfs", 0, "");

  // Register for mouse, wheel and keyboard events.
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);

  // Disable the client-side IME in Chrome.
  text_input_controller_.SetTextInputType(PP_TEXTINPUT_TYPE_NONE);

  // Resister this instance to handle debug log messsages.
  RegisterLoggingInstance();

  // Initialize random seed for libjingle. It's necessary only with OpenSSL.
  char random_seed[kRandomSeedSize];
  crypto::RandBytes(random_seed, sizeof(random_seed));
  rtc::InitRandom(random_seed, sizeof(random_seed));

  // Send hello message.
  PostLegacyJsonMessage("hello", std::make_unique<base::DictionaryValue>());
}

ChromotingInstance::~ChromotingInstance() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // Disconnect the client.
  Disconnect();

  // Unregister this instance so that debug log messages will no longer be sent
  // to it. This will stop all logging in all Chromoting instances.
  UnregisterLoggingInstance();

  // Stopping the context shuts down all chromoting threads.
  context_.Stop();
}

bool ChromotingInstance::Init(uint32_t argc,
                              const char* argn[],
                              const char* argv[]) {
  CHECK(!initialized_);
  initialized_ = true;

  VLOG(1) << "Started ChromotingInstance::Init";

  // Start all the threads.
  context_.Start();

  // Initialize ThreadPool. ThreadPoolInstance::StartWithDefaultParams() doesn't
  // work on NACL.
  base::ThreadPoolInstance::Create("RemotingChromeApp");
  constexpr int kForegroundMaxThreads = 3;
  base::ThreadPoolInstance::Get()->Start({kForegroundMaxThreads});

  return true;
}

void ChromotingInstance::HandleMessage(const pp::Var& message) {
  if (!message.is_string()) {
    LOG(ERROR) << "Received a message that is not a string.";
    return;
  }

  std::unique_ptr<base::Value> json = base::JSONReader::ReadDeprecated(
      message.AsString(), base::JSON_ALLOW_TRAILING_COMMAS);
  base::DictionaryValue* message_dict = nullptr;
  std::string method;
  base::DictionaryValue* data = nullptr;
  if (!json.get() || !json->GetAsDictionary(&message_dict) ||
      !message_dict->GetString("method", &method) ||
      !message_dict->GetDictionary("data", &data)) {
    LOG(ERROR) << "Received invalid message:" << message.AsString();
    return;
  }

  if (method == "connect") {
    UpdateNetConfigAndConnect(*data);
  } else if (method == "disconnect") {
    HandleDisconnect(*data);
  } else if (method == "incomingIq") {
    HandleOnIncomingIq(*data);
  } else if (method == "releaseAllKeys") {
    HandleReleaseAllKeys(*data);
  } else if (method == "injectKeyEvent") {
    HandleInjectKeyEvent(*data);
  } else if (method == "remapKey") {
    HandleRemapKey(*data);
  } else if (method == "trapKey") {
    HandleTrapKey(*data);
  } else if (method == "sendClipboardItem") {
    HandleSendClipboardItem(*data);
  } else if (method == "notifyClientResolution") {
    HandleNotifyClientResolution(*data);
  } else if (method == "videoControl") {
    HandleVideoControl(*data);
  } else if (method == "pauseAudio") {
    HandlePauseAudio(*data);
  } else if (method == "useAsyncPinDialog") {
    use_async_pin_dialog_ = true;
  } else if (method == "onPinFetched") {
    HandleOnPinFetched(*data);
  } else if (method == "onThirdPartyTokenFetched") {
    HandleOnThirdPartyTokenFetched(*data);
  } else if (method == "requestPairing") {
    HandleRequestPairing(*data);
  } else if (method == "extensionMessage") {
    HandleExtensionMessage(*data);
  } else if (method == "allowMouseLock") {
    HandleAllowMouseLockMessage();
  } else if (method == "sendMouseInputWhenUnfocused") {
    HandleSendMouseInputWhenUnfocused();
  } else if (method == "delegateLargeCursors") {
    HandleDelegateLargeCursors();
  } else if (method == "enableDebugRegion") {
    HandleEnableDebugRegion(*data);
  } else if (method == "enableTouchEvents") {
    HandleEnableTouchEvents(*data);
  } else if (method == "enableStuckModifierKeyDetection") {
    HandleEnableStuckModifierKeyDetection(*data);
  }
}

void ChromotingInstance::DidChangeFocus(bool has_focus) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (!IsConnected())
    return;

  input_handler_.DidChangeFocus(has_focus);
  if (mouse_locker_)
    mouse_locker_->DidChangeFocus(has_focus);
}

void ChromotingInstance::DidChangeView(const pp::View& view) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  plugin_view_ = view;
  webrtc::DesktopSize size(
      webrtc::DesktopSize(view.GetRect().width(), view.GetRect().height()));
  mouse_input_filter_.set_input_size(size);
  touch_input_scaler_.set_input_size(size);

  if (video_renderer_)
    video_renderer_->OnViewChanged(view);
}

bool ChromotingInstance::HandleInputEvent(const pp::InputEvent& event) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (!IsConnected())
    return false;

  PP_TimeTicks latency =
      pp::Module::Get()->core()->GetTimeTicks() - event.GetTimeStamp();
  pp::UMAPrivate uma(this);
  uma.HistogramCustomTimes(
      kInputEventLatencyHistogram, static_cast<int64_t>(latency * 1000000),
      kInputEventLatencyHistogramMinUs, kInputEventLatencyHistogramMaxUs,
      kInputEventLatencyHistogramBuckets);

  return input_handler_.HandleInputEvent(event);
}

void ChromotingInstance::OnVideoDecodeError() {
  Disconnect();

  // Assume that the decoder failure was caused by the host not encoding video
  // correctly and report it as a protocol error.
  // TODO(sergeyu): Consider using a different error code in case the decoder
  // error was caused by some other problem.
  OnConnectionState(protocol::ConnectionToHost::FAILED,
                    protocol::INCOMPATIBLE_PROTOCOL);
}

void ChromotingInstance::OnVideoFirstFrameReceived() {
  PostLegacyJsonMessage("onFirstFrameReceived",
                        std::make_unique<base::DictionaryValue>());
}

void ChromotingInstance::OnVideoFrameDirtyRegion(
    const webrtc::DesktopRegion& dirty_region) {
  std::unique_ptr<base::ListValue> rects_value(new base::ListValue());
  for (webrtc::DesktopRegion::Iterator i(dirty_region); !i.IsAtEnd();
       i.Advance()) {
    const webrtc::DesktopRect& rect = i.rect();
    std::unique_ptr<base::ListValue> rect_value(new base::ListValue());
    rect_value->AppendInteger(rect.left());
    rect_value->AppendInteger(rect.top());
    rect_value->AppendInteger(rect.width());
    rect_value->AppendInteger(rect.height());
    rects_value->Append(std::move(rect_value));
  }

  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->Set("rects", std::move(rects_value));
  PostLegacyJsonMessage("onDebugRegion", std::move(data));
}

void ChromotingInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  pp::UMAPrivate uma(this);

  switch (state) {
    case protocol::ConnectionToHost::INITIALIZING:
      NOTREACHED();
      break;
    case protocol::ConnectionToHost::CONNECTING:
      connection_started_time = base::TimeTicks::Now();
      break;
    case protocol::ConnectionToHost::AUTHENTICATED:
      connection_authenticated_time_ = base::TimeTicks::Now();
      uma.HistogramCustomTimes(
          kTimeToAuthenticateHistogram,
          (connection_authenticated_time_ - connection_started_time)
              .InMilliseconds(),
          kConnectionTimesHistogramMinMs, kConnectionTimesHistogramMaxMs,
          kConnectionTimesHistogramBuckets);
      break;
    case protocol::ConnectionToHost::CONNECTED:
      connection_connected_time_ = base::TimeTicks::Now();
      uma.HistogramCustomTimes(
          kTimeToConnectHistogram,
          (connection_connected_time_ - connection_authenticated_time_)
              .InMilliseconds(),
          kConnectionTimesHistogramMinMs, kConnectionTimesHistogramMaxMs,
          kConnectionTimesHistogramBuckets);
      break;
    case protocol::ConnectionToHost::CLOSED:
      if (!connection_connected_time_.is_null()) {
        uma.HistogramCustomTimes(
            kClosedSessionDurationHistogram,
            (base::TimeTicks::Now() - connection_connected_time_)
                .InMilliseconds(),
            kConnectionDurationHistogramMinMinutes,
            kConnectionDurationHistogramMaxMinutes,
            kConnectionDurationHistogramBuckets);
      }
      break;
    case protocol::ConnectionToHost::FAILED:
      if (!connection_connected_time_.is_null()) {
        uma.HistogramCustomTimes(
            kFailedSessionDurationHistogram,
            (base::TimeTicks::Now() - connection_connected_time_)
                .InMilliseconds(),
            kConnectionDurationHistogramMinMinutes,
            kConnectionDurationHistogramMaxMinutes,
            kConnectionDurationHistogramBuckets);
      }
      break;
  }

  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("state", protocol::ConnectionToHost::StateToString(state));
  data->SetString("error", ConnectionErrorToString(error));
  PostLegacyJsonMessage("onConnectionStatus", std::move(data));
}

void ChromotingInstance::FetchThirdPartyToken(
    const std::string& host_public_key,
    const std::string& token_url,
    const std::string& scope,
    const protocol::ThirdPartyTokenFetchedCallback& token_fetched_callback) {
  // Once the Session object calls this function, it won't continue the
  // authentication until the callback is called (or connection is canceled).
  // So, it's impossible to reach this with a callback already registered.
  DCHECK(third_party_token_fetched_callback_.is_null());
  third_party_token_fetched_callback_ = token_fetched_callback;
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("tokenUrl", token_url);
  data->SetString("hostPublicKey", host_public_key);
  data->SetString("scope", scope);
  PostLegacyJsonMessage("fetchThirdPartyToken", std::move(data));
}

void ChromotingInstance::OnConnectionReady(bool ready) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetBoolean("ready", ready);
  PostLegacyJsonMessage("onConnectionReady", std::move(data));
}

void ChromotingInstance::OnRouteChanged(const std::string& channel_name,
                                        const protocol::TransportRoute& route) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("channel", channel_name);
  data->SetString("connectionType",
                  protocol::TransportRoute::GetTypeString(route.type));
  PostLegacyJsonMessage("onRouteChanged", std::move(data));
}

void ChromotingInstance::SetCapabilities(const std::string& capabilities) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("capabilities", capabilities);
  PostLegacyJsonMessage("setCapabilities", std::move(data));
}

void ChromotingInstance::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("clientId", pairing_response.client_id());
  data->SetString("sharedSecret", pairing_response.shared_secret());
  PostLegacyJsonMessage("pairingResponse", std::move(data));
}

void ChromotingInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("type", message.type());
  data->SetString("data", message.data());
  PostLegacyJsonMessage("extensionMessage", std::move(data));
}

void ChromotingInstance::SetDesktopSize(const webrtc::DesktopSize& size,
                                        const webrtc::DesktopVector& dpi) {
  DCHECK(!dpi.is_zero());

  mouse_input_filter_.set_output_size(size);
  touch_input_scaler_.set_output_size(size);

  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("width", size.width());
  data->SetInteger("height", size.height());
  data->SetInteger("x_dpi", dpi.x());
  data->SetInteger("y_dpi", dpi.y());
  PostLegacyJsonMessage("onDesktopSize", std::move(data));
}

void ChromotingInstance::FetchSecretFromDialog(
    bool pairing_supported,
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  // Once the Session object calls this function, it won't continue the
  // authentication until the callback is called (or connection is canceled).
  // So, it's impossible to reach this with a callback already registered.
  DCHECK(secret_fetched_callback_.is_null());
  secret_fetched_callback_ = secret_fetched_callback;
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetBoolean("pairingSupported", pairing_supported);
  PostLegacyJsonMessage("fetchPin", std::move(data));
}

void ChromotingInstance::FetchSecretFromString(
    const std::string& shared_secret,
    bool pairing_supported,
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  secret_fetched_callback.Run(shared_secret);
}

protocol::ClipboardStub* ChromotingInstance::GetClipboardStub() {
  // TODO(sergeyu): Move clipboard handling to a separate class.
  // crbug.com/138108
  return this;
}

protocol::CursorShapeStub* ChromotingInstance::GetCursorShapeStub() {
  return &empty_cursor_filter_;
}

void ChromotingInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("mimeType", event.mime_type());
  data->SetString("item", event.data());
  PostLegacyJsonMessage("injectClipboardItem", std::move(data));
}

void ChromotingInstance::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  // If the delegated cursor is empty then stop rendering a DOM cursor.
  if (IsCursorShapeEmpty(cursor_shape)) {
    PostChromotingMessage("unsetCursorShape", pp::VarDictionary());
    return;
  }

  // Cursor is not empty, so pass it to JS to render.
  const int kBytesPerPixel = sizeof(uint32_t);
  const size_t buffer_size =
      cursor_shape.height() * cursor_shape.width() * kBytesPerPixel;

  pp::VarArrayBuffer array_buffer(buffer_size);
  void* dst = array_buffer.Map();
  memcpy(dst, cursor_shape.data().data(), buffer_size);
  array_buffer.Unmap();

  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("width"), cursor_shape.width());
  dictionary.Set(pp::Var("height"), cursor_shape.height());
  dictionary.Set(pp::Var("hotspotX"), cursor_shape.hotspot_x());
  dictionary.Set(pp::Var("hotspotY"), cursor_shape.hotspot_y());
  dictionary.Set(pp::Var("data"), array_buffer);
  PostChromotingMessage("setCursorShape", dictionary);
}

void ChromotingInstance::HandleConnect(const base::DictionaryValue& data) {
  protocol::ClientAuthenticationConfig client_auth_config;

  std::string local_jid;
  std::string host_jid;
  std::string host_public_key;
  if (!data.GetString("hostJid", &host_jid) ||
      !data.GetString("hostPublicKey", &host_public_key) ||
      !data.GetString("localJid", &local_jid) ||
      !data.GetString("hostId", &client_auth_config.host_id)) {
    LOG(ERROR) << "Invalid connect() data.";
    return;
  }

  data.GetString("clientPairingId", &client_auth_config.pairing_client_id);
  data.GetString("clientPairedSecret", &client_auth_config.pairing_secret);

  if (use_async_pin_dialog_) {
    client_auth_config.fetch_secret_callback = base::Bind(
        &ChromotingInstance::FetchSecretFromDialog, weak_factory_.GetWeakPtr());
  } else {
    std::string shared_secret;
    if (!data.GetString("sharedSecret", &shared_secret)) {
      LOG(ERROR) << "sharedSecret not specified in connect().";
      return;
    }
    client_auth_config.fetch_secret_callback =
        base::Bind(&ChromotingInstance::FetchSecretFromString, shared_secret);
  }

  client_auth_config.fetch_third_party_token_callback =
      base::Bind(&ChromotingInstance::FetchThirdPartyToken,
                 weak_factory_.GetWeakPtr(), host_public_key);

  // Read the list of capabilities, if any.
  std::string capabilities;
  if (data.HasKey("capabilities")) {
    if (!data.GetString("capabilities", &capabilities)) {
      LOG(ERROR) << "Invalid connect() data.";
      return;
    }
  }

  // Read and parse list of experiments.
  std::string experiments;
  std::vector<std::string> experiments_list;
  if (data.GetString("experiments", &experiments)) {
    experiments_list = base::SplitString(
        experiments, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }

  VLOG(0) << "Connecting to " << host_jid << ". Local jid: " << local_jid
          << ".";

  std::string key_filter;
  if (!data.GetString("keyFilter", &key_filter)) {
    NOTREACHED();
    normalizing_input_filter_.reset(new protocol::InputFilter(&key_mapper_));
  } else if (key_filter == "mac") {
    normalizing_input_filter_.reset(
        new NormalizingInputFilterMac(&key_mapper_));
  } else if (key_filter == "cros") {
    normalizing_input_filter_.reset(
        new NormalizingInputFilterCros(&key_mapper_));
  } else if (key_filter == "windows") {
    normalizing_input_filter_.reset(
        new NormalizingInputFilterWin(&key_mapper_));
  } else {
    DCHECK(key_filter.empty());
    normalizing_input_filter_.reset(new protocol::InputFilter(&key_mapper_));
  }
  input_tracker_.set_input_stub(normalizing_input_filter_.get());

  // Try initializing 3D video renderer.
  video_renderer_.reset(new PepperVideoRenderer3D());
  video_renderer_->SetPepperContext(this, this);
  if (!video_renderer_->Initialize(context_, &perf_tracker_)) {
    video_renderer_.reset();
  }

  // If we didn't initialize 3D renderer then use the 2D renderer.
  if (!video_renderer_) {
    LOG(WARNING)
        << "Failed to initialize 3D renderer. Using 2D renderer instead.";
    video_renderer_.reset(new PepperVideoRenderer2D());
    video_renderer_->SetPepperContext(this, this);
    if (!video_renderer_->Initialize(context_, &perf_tracker_)) {
      video_renderer_.reset();
    }
  }

  CHECK(video_renderer_);

  perf_tracker_.SetUpdateUmaCallbacks(
      base::Bind(&ChromotingInstance::UpdateUmaCustomHistogram,
                 weak_factory_.GetWeakPtr(), true),
      base::Bind(&ChromotingInstance::UpdateUmaCustomHistogram,
                 weak_factory_.GetWeakPtr(), false),
      base::Bind(&ChromotingInstance::UpdateUmaEnumHistogram,
                 weak_factory_.GetWeakPtr()));

  if (!plugin_view_.is_null())
    video_renderer_->OnViewChanged(plugin_view_);

  if (!audio_player_) {
    audio_player_.reset(new PepperAudioPlayer(this));
  }

  client_.reset(new ChromotingClient(&context_, this, video_renderer_.get(),
                                     audio_player_->GetWeakPtr()));
  std::string host_experiment_config;
  if (data.GetString("hostConfiguration", &host_experiment_config)) {
    client_->set_host_experiment_config(host_experiment_config);
  }

  // Setup the signal strategy.
  signal_strategy_.reset(new DelegatingSignalStrategy(
      SignalingAddress(local_jid), plugin_task_runner_,
      base::Bind(&ChromotingInstance::SendOutgoingIq,
                 weak_factory_.GetWeakPtr())));

  // Create TransportContext.
  transport_context_ = new protocol::TransportContext(
      signal_strategy_.get(),
      std::make_unique<PepperPortAllocatorFactory>(
          this, base::BindRepeating(&ChromotingInstance::SendNetworkInfo,
                                    base::Unretained(this))),
      std::make_unique<PepperUrlRequestFactory>(this),
      protocol::NetworkSettings(protocol::NetworkSettings::NAT_TRAVERSAL_FULL),
      protocol::TransportRole::CLIENT);

  std::unique_ptr<protocol::CandidateSessionConfig> config =
      protocol::CandidateSessionConfig::CreateDefault();
  if (std::find(experiments_list.begin(), experiments_list.end(), "vp9") !=
      experiments_list.end()) {
    config->set_vp9_experiment_enabled(true);
  }
  if (std::find(experiments_list.begin(), experiments_list.end(), "h264") !=
      experiments_list.end()) {
    config->set_h264_experiment_enabled(true);
  }
  client_->set_protocol_config(std::move(config));

  // Kick off the connection.
  client_->Start(signal_strategy_.get(), client_auth_config, transport_context_,
                 host_jid, capabilities);

  // Connect the input pipeline to the protocol stub.
  mouse_input_filter_.set_input_stub(client_->input_stub());
  if (!plugin_view_.is_null()) {
    webrtc::DesktopSize size(plugin_view_.GetRect().width(),
                             plugin_view_.GetRect().height());
    mouse_input_filter_.set_input_size(size);
    touch_input_scaler_.set_input_size(size);
  }

  // Start timer that periodically sends perf stats.
  stats_update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kUIStatsUpdatePeriodSeconds),
      base::Bind(&ChromotingInstance::UpdatePerfStatsInUI,
                 base::Unretained(this)));
}

void ChromotingInstance::HandleDisconnect(const base::DictionaryValue& data) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  Disconnect();
}

void ChromotingInstance::HandleOnIncomingIq(const base::DictionaryValue& data) {
  std::string iq;
  if (!data.GetString("iq", &iq)) {
    LOG(ERROR) << "Invalid incomingIq() data.";
    return;
  }

  // Just ignore the message if it's received before Connect() is called. It's
  // likely to be a leftover from a previous session, so it's safe to ignore it.
  if (signal_strategy_)
    signal_strategy_->GetIncomingMessageCallback().Run(iq);
}

void ChromotingInstance::HandleReleaseAllKeys(
    const base::DictionaryValue& data) {
  if (IsConnected())
    input_tracker_.ReleaseAll();
}

void ChromotingInstance::HandleInjectKeyEvent(
    const base::DictionaryValue& data) {
  int usb_keycode = 0;
  bool is_pressed = false;
  if (!data.GetInteger("usbKeycode", &usb_keycode) ||
      !data.GetBoolean("pressed", &is_pressed)) {
    LOG(ERROR) << "Invalid injectKeyEvent.";
    return;
  }

  protocol::KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(is_pressed);

  // Inject after the KeyEventMapper, so the event won't get mapped or trapped.
  if (IsConnected())
    touch_input_scaler_.InjectKeyEvent(event);
}

void ChromotingInstance::HandleRemapKey(const base::DictionaryValue& data) {
  int from_keycode = 0;
  int to_keycode = 0;
  if (!data.GetInteger("fromKeycode", &from_keycode) ||
      !data.GetInteger("toKeycode", &to_keycode)) {
    LOG(ERROR) << "Invalid remapKey.";
    return;
  }

  key_mapper_.RemapKey(from_keycode, to_keycode);
}

void ChromotingInstance::HandleTrapKey(const base::DictionaryValue& data) {
  int keycode = 0;
  bool trap = false;
  if (!data.GetInteger("keycode", &keycode) ||
      !data.GetBoolean("trap", &trap)) {
    LOG(ERROR) << "Invalid trapKey.";
    return;
  }

  key_mapper_.TrapKey(keycode, trap);
}

void ChromotingInstance::HandleSendClipboardItem(
    const base::DictionaryValue& data) {
  std::string mime_type;
  std::string item;
  if (!data.GetString("mimeType", &mime_type) ||
      !data.GetString("item", &item)) {
    LOG(ERROR) << "Invalid sendClipboardItem data.";
    return;
  }
  if (!IsConnected()) {
    return;
  }
  protocol::ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(item);
  client_->clipboard_forwarder()->InjectClipboardEvent(event);
}

void ChromotingInstance::HandleNotifyClientResolution(
    const base::DictionaryValue& data) {
  int width = 0;
  int height = 0;
  int x_dpi = kDefaultDPI;
  int y_dpi = kDefaultDPI;
  if (!data.GetInteger("width", &width) ||
      !data.GetInteger("height", &height) ||
      !data.GetInteger("x_dpi", &x_dpi) || !data.GetInteger("y_dpi", &y_dpi) ||
      width <= 0 || height <= 0 || x_dpi <= 0 || y_dpi <= 0) {
    LOG(ERROR) << "Invalid notifyClientResolution.";
    return;
  }

  if (!IsConnected()) {
    return;
  }

  protocol::ClientResolution client_resolution;
  client_resolution.set_x_dpi(x_dpi);
  client_resolution.set_y_dpi(y_dpi);
  client_resolution.set_dips_width((width * kDefaultDPI) / x_dpi);
  client_resolution.set_dips_height((height * kDefaultDPI) / y_dpi);

  // Include the legacy width & height in physical pixels for use by older
  // hosts.
  client_resolution.set_width_deprecated(width);
  client_resolution.set_height_deprecated(height);

  client_->host_stub()->NotifyClientResolution(client_resolution);
}

void ChromotingInstance::HandleVideoControl(const base::DictionaryValue& data) {
  protocol::VideoControl video_control;
  bool pause_video = false;
  if (data.GetBoolean("pause", &pause_video)) {
    video_control.set_enable(!pause_video);
    perf_tracker_.OnPauseStateChanged(pause_video);
  }
  bool lossless_encode = false;
  if (data.GetBoolean("losslessEncode", &lossless_encode)) {
    video_control.set_lossless_encode(lossless_encode);
  }
  bool lossless_color = false;
  if (data.GetBoolean("losslessColor", &lossless_color)) {
    video_control.set_lossless_color(lossless_color);
  }
  if (!IsConnected()) {
    return;
  }
  client_->host_stub()->ControlVideo(video_control);
}

void ChromotingInstance::HandlePauseAudio(const base::DictionaryValue& data) {
  bool pause = false;
  if (!data.GetBoolean("pause", &pause)) {
    LOG(ERROR) << "Invalid pauseAudio.";
    return;
  }
  if (!IsConnected()) {
    return;
  }
  protocol::AudioControl audio_control;
  audio_control.set_enable(!pause);
  client_->host_stub()->ControlAudio(audio_control);
}
void ChromotingInstance::HandleOnPinFetched(const base::DictionaryValue& data) {
  std::string pin;
  if (!data.GetString("pin", &pin)) {
    LOG(ERROR) << "Invalid onPinFetched.";
    return;
  }
  if (!secret_fetched_callback_.is_null()) {
    std::move(secret_fetched_callback_).Run(pin);
  } else {
    LOG(WARNING) << "Ignored OnPinFetched received without a pending fetch.";
  }
}

void ChromotingInstance::HandleOnThirdPartyTokenFetched(
    const base::DictionaryValue& data) {
  std::string token;
  std::string shared_secret;
  if (!data.GetString("token", &token) ||
      !data.GetString("sharedSecret", &shared_secret)) {
    LOG(ERROR) << "Invalid onThirdPartyTokenFetched data.";
    return;
  }
  if (!third_party_token_fetched_callback_.is_null()) {
    std::move(third_party_token_fetched_callback_).Run(token, shared_secret);
  } else {
    LOG(WARNING) << "Ignored OnThirdPartyTokenFetched without a pending fetch.";
  }
}

void ChromotingInstance::HandleRequestPairing(
    const base::DictionaryValue& data) {
  std::string client_name;
  if (!data.GetString("clientName", &client_name)) {
    LOG(ERROR) << "Invalid requestPairing";
    return;
  }
  if (!IsConnected()) {
    return;
  }
  protocol::PairingRequest pairing_request;
  pairing_request.set_client_name(client_name);
  client_->host_stub()->RequestPairing(pairing_request);
}

void ChromotingInstance::HandleExtensionMessage(
    const base::DictionaryValue& data) {
  std::string type;
  std::string message_data;
  if (!data.GetString("type", &type) ||
      !data.GetString("data", &message_data)) {
    LOG(ERROR) << "Invalid extensionMessage.";
    return;
  }
  if (!IsConnected()) {
    return;
  }
  protocol::ExtensionMessage message;
  message.set_type(type);
  message.set_data(message_data);
  client_->host_stub()->DeliverClientMessage(message);
}

void ChromotingInstance::HandleAllowMouseLockMessage() {
  // Create the mouse lock handler and route cursor shape messages through it.
  mouse_locker_.reset(new PepperMouseLocker(
      this,
      base::Bind(&PepperInputHandler::set_send_mouse_move_deltas,
                 base::Unretained(&input_handler_)),
      &cursor_setter_));
  empty_cursor_filter_.set_cursor_stub(mouse_locker_.get());
}

void ChromotingInstance::HandleSendMouseInputWhenUnfocused() {
  input_handler_.set_send_mouse_input_when_unfocused(true);
}

void ChromotingInstance::HandleDelegateLargeCursors() {
  cursor_setter_.set_delegate_stub(this);
}

void ChromotingInstance::HandleEnableDebugRegion(
    const base::DictionaryValue& data) {
  bool enable = false;
  if (!data.GetBoolean("enable", &enable)) {
    LOG(ERROR) << "Invalid enableDebugRegion.";
    return;
  }

  video_renderer_->EnableDebugDirtyRegion(enable);
}

void ChromotingInstance::HandleEnableTouchEvents(
    const base::DictionaryValue& data) {
  bool enable = false;
  if (!data.GetBoolean("enable", &enable)) {
    LOG(ERROR) << "Invalid enableTouchEvents.";
    return;
  }

  if (enable) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_TOUCH);
  } else {
    ClearInputEventRequest(PP_INPUTEVENT_CLASS_TOUCH);
  }
}

void ChromotingInstance::HandleEnableStuckModifierKeyDetection(
    const base::DictionaryValue& data) {
  bool enable = false;
  if (!data.GetBoolean("enable", &enable)) {
    LOG(ERROR) << "Invalid enableStuckModifierKeyDetection.";
    return;
  }

  input_handler_.set_detect_stuck_modifiers(enable);
}

void ChromotingInstance::Disconnect() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  VLOG(0) << "Disconnecting from host.";

  // Disconnect the input pipeline and teardown the connection.
  mouse_input_filter_.set_input_stub(nullptr);
  client_.reset();
  video_renderer_.reset();
  audio_player_.reset();
  stats_update_timer_.Stop();
  transport_context_ = nullptr;
}

void ChromotingInstance::UpdateNetConfigAndConnect(
    const base::DictionaryValue& data) {
  OnNetConfigUpdated(data);
}

void ChromotingInstance::OnNetConfigUpdated(
    std::unique_ptr<base::DictionaryValue> data) {
  HandleConnect(*data);
}

void ChromotingInstance::PostChromotingMessage(const std::string& method,
                                               const pp::VarDictionary& data) {
  pp::VarDictionary message;
  message.Set(pp::Var("method"), pp::Var(method));
  message.Set(pp::Var("data"), data);
  PostMessage(message);
}

void ChromotingInstance::PostLegacyJsonMessage(
    const std::string& method,
    std::unique_ptr<base::DictionaryValue> data) {
  base::DictionaryValue message;
  message.SetString("method", method);
  message.Set("data", std::move(data));

  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  PostMessage(pp::Var(message_json));
}

void ChromotingInstance::SendTrappedKey(uint32_t usb_keycode, bool pressed) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("usbKeycode", usb_keycode);
  data->SetBoolean("pressed", pressed);
  PostLegacyJsonMessage("trappedKeyEvent", std::move(data));
}

void ChromotingInstance::SendOutgoingIq(const std::string& iq) {
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("iq", iq);
  PostLegacyJsonMessage("sendOutgoingIq", std::move(data));
}

void ChromotingInstance::UpdatePerfStatsInUI() {
  // Fetch performance stats from the VideoRenderer and send them to the client
  // for display to users.
  std::unique_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetDouble("videoBandwidth", perf_tracker_.video_bandwidth());
  data->SetDouble("videoFrameRate", perf_tracker_.video_frame_rate());
  data->SetDouble("captureLatency", perf_tracker_.video_capture_ms().Average());
  data->SetDouble("maxCaptureLatency", perf_tracker_.video_capture_ms().Max());
  data->SetDouble("encodeLatency", perf_tracker_.video_encode_ms().Average());
  data->SetDouble("maxEncodeLatency", perf_tracker_.video_encode_ms().Max());
  data->SetDouble("decodeLatency", perf_tracker_.video_decode_ms().Average());
  data->SetDouble("maxDecodeLatency", perf_tracker_.video_decode_ms().Max());
  data->SetDouble("renderLatency", perf_tracker_.video_paint_ms().Average());
  data->SetDouble("maxRenderLatency", perf_tracker_.video_paint_ms().Max());
  data->SetDouble("roundtripLatency", perf_tracker_.round_trip_ms().Average());
  data->SetDouble("maxRoundtripLatency", perf_tracker_.round_trip_ms().Max());
  PostLegacyJsonMessage("onPerfStats", std::move(data));
}

// static
void ChromotingInstance::RegisterLogMessageHandler() {
  base::AutoLock lock(g_logging_lock.Get());

  // Set up log message handler.
  // This is not thread-safe so we need it within our lock.
  logging::SetLogMessageHandler(&LogToUI);
}

void ChromotingInstance::RegisterLoggingInstance() {
  base::AutoLock lock(g_logging_lock.Get());
  g_logging_instance = pp_instance();
}

void ChromotingInstance::UnregisterLoggingInstance() {
  base::AutoLock lock(g_logging_lock.Get());

  // Don't unregister unless we're the currently registered instance.
  if (pp_instance() != g_logging_instance)
    return;

  // Unregister this instance for logging.
  g_logging_instance = 0;
}

// static
bool ChromotingInstance::LogToUI(int severity,
                                 const char* file,
                                 int line,
                                 size_t message_start,
                                 const std::string& str) {
  PP_LogLevel log_level = PP_LOGLEVEL_ERROR;
  switch (severity) {
    case logging::LOG_INFO:
      log_level = PP_LOGLEVEL_TIP;
      break;
    case logging::LOG_WARNING:
      log_level = PP_LOGLEVEL_WARNING;
      break;
    case logging::LOG_ERROR:
    case logging::LOG_FATAL:
      log_level = PP_LOGLEVEL_ERROR;
      break;
  }

  PP_Instance pp_instance = 0;
  {
    base::AutoLock lock(g_logging_lock.Get());
    if (g_logging_instance)
      pp_instance = g_logging_instance;
  }
  if (pp_instance) {
    const PPB_Console* console = reinterpret_cast<const PPB_Console*>(
        pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
    if (console)
      console->Log(pp_instance, log_level, pp::Var(str).pp_var());
  }

  // If this is a fatal message the log handler is going to crash after this
  // function returns. In that case sleep for 1 second, Otherwise the plugin
  // may crash before the message is delivered to the console.
  if (severity == logging::LOG_FATAL)
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));

  return false;
}

bool ChromotingInstance::IsConnected() {
  return client_ &&
         (client_->connection_state() == protocol::ConnectionToHost::CONNECTED);
}

void ChromotingInstance::UpdateUmaEnumHistogram(
    const std::string& histogram_name,
    int64_t value,
    int histogram_max) {
  pp::UMAPrivate uma(this);
  uma.HistogramEnumeration(histogram_name, value, histogram_max);
}

void ChromotingInstance::UpdateUmaCustomHistogram(
    bool is_custom_counts_histogram,
    const std::string& histogram_name,
    int64_t value,
    int histogram_min,
    int histogram_max,
    int histogram_buckets) {
  pp::UMAPrivate uma(this);

  if (is_custom_counts_histogram) {
    uma.HistogramCustomCounts(histogram_name, value, histogram_min,
                              histogram_max, histogram_buckets);
  } else {
    uma.HistogramCustomTimes(histogram_name, value, histogram_min,
                             histogram_max, histogram_buckets);
  }
}

void ChromotingInstance::SendNetworkInfo() {
  if (!transport_context_)
    return;
  rtc::NetworkManager* network_manager = transport_context_->network_manager();
  if (!network_manager)
    return;

  rtc::NetworkManager::NetworkList network_list;
  network_manager->GetNetworks(&network_list);

  std::set<std::string> network_names;
  for (const auto* network : network_list) {
    network_names.insert(network->name());
  }
  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("interfaceCount"),
                 static_cast<int32_t>(network_names.size()));
  PostChromotingMessage("networkInfo", dictionary);
}

}  // namespace remoting
