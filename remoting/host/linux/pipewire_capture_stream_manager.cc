// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_capture_stream_manager.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_ScreenCast.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

using gvariant::Boxed;

constexpr char kScreenCastBusName[] = "org.gnome.Mutter.ScreenCast";

}  // namespace

PipewireCaptureStreamManager::AddStreamRequest::AddStreamRequest() = default;
PipewireCaptureStreamManager::AddStreamRequest::AddStreamRequest(
    AddStreamRequest&&) = default;
PipewireCaptureStreamManager::AddStreamRequest::AddStreamRequest(
    const ScreenResolution& initial_resolution,
    AddStreamCallback callback)
    : initial_resolution(initial_resolution), callback(std::move(callback)) {}
PipewireCaptureStreamManager::AddStreamRequest::~AddStreamRequest() = default;

PipewireCaptureStreamManager::StreamInfo::StreamInfo() = default;
PipewireCaptureStreamManager::StreamInfo::StreamInfo(StreamInfo&&) = default;
PipewireCaptureStreamManager::StreamInfo&
PipewireCaptureStreamManager::StreamInfo::operator=(StreamInfo&&) = default;
PipewireCaptureStreamManager::StreamInfo::~StreamInfo() = default;

PipewireCaptureStreamManager::PipewireCaptureStreamManager() = default;
PipewireCaptureStreamManager::~PipewireCaptureStreamManager() = default;

PipewireCaptureStreamManager::Observer::Subscription
PipewireCaptureStreamManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
  return Observer::Subscription(base::BindOnce(
      &PipewireCaptureStreamManager::RemoveObserver, GetWeakPtr(), observer));
}

void PipewireCaptureStreamManager::Init(
    GDBusConnectionRef* connection,
    base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor,
    gvariant::ObjectPath screencast_session_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection);
  DCHECK(display_config_monitor);

  connection_ = connection;
  screencast_session_path_ = std::move(screencast_session_path);

  if (display_config_monitor) {
    monitors_changed_subscription_ = display_config_monitor->AddCallback(
        base::BindRepeating(
            &PipewireCaptureStreamManager::OnGnomeDisplayConfigChanged,
            GetWeakPtr()),
        /*call_with_current_config=*/true);
  }
}

base::WeakPtr<CaptureStream> PipewireCaptureStreamManager::GetStream(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& it = streams_.find(screen_id);
  if (it == streams_.end()) {
    return nullptr;
  }
  return it->second.stream->GetWeakPtr();
}

void PipewireCaptureStreamManager::AddStream(
    const ScreenResolution& initial_resolution,
    AddStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_add_stream_requests_.emplace(initial_resolution, std::move(callback));

  if (!last_seen_display_config_.has_value()) {
    // We can't safely start adding the stream if we haven't received the
    // initial display config, since otherwise we wouldn't tell which monitor is
    // newly added.
    HOST_LOG << "Stream will be added after initial display config is loaded.";
  } else if (pending_add_stream_requests_.size() > 1) {
    HOST_LOG << "Stream will be added after pending stream is added.";
  } else {
    MaybeAddStreamForCurrentRequest();
  }
}

void PipewireCaptureStreamManager::RemoveStream(webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = streams_.find(screen_id);
  if (it == streams_.end()) {
    LOG(ERROR) << "Cannot find stream for screen ID: " << screen_id;
    return;
  }
  // The virtual monitor will not be removed until the screencast Stop() method
  // is called.
  connection_->Call<org_gnome_Mutter_ScreenCast_Stream::Stop>(
      kScreenCastBusName, it->second.stream_path, std::tuple(),
      base::BindOnce(&PipewireCaptureStreamManager::OnStreamStopped,
                     GetWeakPtr(), screen_id));
}

base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
PipewireCaptureStreamManager::GetActiveStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>> output;
  for (auto& [screen_id, stream_info] : streams_) {
    output[screen_id] = stream_info.stream->GetWeakPtr();
  }
  return output;
}

base::WeakPtr<PipewireCaptureStreamManager>
PipewireCaptureStreamManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PipewireCaptureStreamManager::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

template <typename SuccessType, typename String>
GDBusConnectionRef::CallCallback<SuccessType>
PipewireCaptureStreamManager::CheckAddStreamResultAndContinue(
    void (PipewireCaptureStreamManager::*success_method)(SuccessType),
    String&& error_context) {
  return base::BindOnce(
      [](base::WeakPtr<PipewireCaptureStreamManager> that,
         decltype(success_method) success_method,
         std::string_view error_context,
         base::expected<SuccessType, Loggable> result) {
        if (!that) {
          return;
        }
        if (result.has_value()) {
          (that.get()->*success_method)(std::move(result).value());
        } else {
          that->OnAddStreamError(error_context, std::move(result).error());
        }
      },
      GetWeakPtr(), success_method, std::forward<String>(error_context));
}

void PipewireCaptureStreamManager::MaybeAddStreamForCurrentRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(last_seen_display_config_.has_value());
  DCHECK(!pending_stream_);

  if (pending_add_stream_requests_.empty()) {
    return;
  }

  // Include the cursor in the Pipewire stream metadata.
  constexpr std::uint32_t kCursorModeMetadata = 2;

  pending_stream_ = std::make_unique<PipewireCaptureStream>();
  connection_->Call<org_gnome_Mutter_ScreenCast_Session::RecordVirtual>(
      kScreenCastBusName, screencast_session_path_,
      std::tuple{std::array{
          std::pair{"cursor-mode", GVariantFrom(Boxed{kCursorModeMetadata})},
          std::pair{"is-platform", GVariantFrom(Boxed{true})}}},
      CheckAddStreamResultAndContinue(
          &PipewireCaptureStreamManager::OnStreamCreated,
          "Failed to record virtual monitor"));
}

void PipewireCaptureStreamManager::RunCurrentAddStreamCallback(
    AddStreamResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_add_stream_requests_.empty());

  pending_stream_.reset();
  auto callback = std::move(pending_add_stream_requests_.front().callback);
  pending_add_stream_requests_.pop();
  auto stream = result.value_or(nullptr);
  std::move(callback).Run(std::move(result));
  if (stream) {
    observers_.Notify(&Observer::OnPipewireCaptureStreamAdded, stream);
  }
  MaybeAddStreamForCurrentRequest();
}

void PipewireCaptureStreamManager::OnAddStreamError(
    std::string_view error_message,
    Loggable error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunCurrentAddStreamCallback(base::unexpected(
      base::StrCat({error_message, ": ", error_context.ToString()})));
}
void PipewireCaptureStreamManager::OnStreamCreated(
    std::tuple<gvariant::ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_stream_);
  HOST_LOG << "Starting initial monitor stream";
  std::tie(pending_stream_path_) = args;

  connection_->GetProperty<org_gnome_Mutter_ScreenCast_Stream::Parameters>(
      kScreenCastBusName, pending_stream_path_,
      CheckAddStreamResultAndContinue(
          &PipewireCaptureStreamManager::OnStreamParameters,
          "Failed to retrieve stream parameters"));
}

void PipewireCaptureStreamManager::OnStreamParameters(
    GVariantRef<"a{sv}"> parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_stream_);
  gchar* param_str = g_variant_print(parameters.raw(), true);
  HOST_LOG << "Stream parameters: " << param_str;
  g_free(param_str);

  auto maybe_boxed_mapping_id = parameters.LookUp("mapping-id");
  if (!maybe_boxed_mapping_id.has_value()) {
    RunCurrentAddStreamCallback(
        base::unexpected("mapping-id stream parameter not present"));
    return;
  }
  std::string mapping_id;
  auto destructure_result = maybe_boxed_mapping_id->TryDestructure(mapping_id);
  if (!destructure_result.has_value()) {
    RunCurrentAddStreamCallback(base::unexpected(
        base::StrCat({" Failed to retrieve mapping-id stream parameter: ",
                      destructure_result.error().ToString()})));
    return;
  }
  // Note that both OnStreamStarted and OnPipeWireStreamAdded may invoke
  // the AddStreamCallback, but the former only does so on error and the latter
  // unsubscribes from the signal, meaning that it is guaranteed only to
  // be called once per stream.
  pending_stream_added_signal_ = connection_->SignalSubscribe<
      org_gnome_Mutter_ScreenCast_Stream::PipeWireStreamAdded>(
      kScreenCastBusName, pending_stream_path_,
      base::BindRepeating(&PipewireCaptureStreamManager::OnPipeWireStreamAdded,
                          GetWeakPtr(), std::move(mapping_id)));
  connection_->Call<org_gnome_Mutter_ScreenCast_Stream::Start>(
      kScreenCastBusName, pending_stream_path_, std::tuple(),
      CheckAddStreamResultAndContinue(
          &PipewireCaptureStreamManager::OnStreamStarted,
          "Failed to start monitor stream"));
}

void PipewireCaptureStreamManager::OnStreamStarted(std::tuple<> args) {
  // Do nothing. Still need to wait for PipeWire-stream-added signal.
}

void PipewireCaptureStreamManager::OnStreamStopped(
    webrtc::ScreenId screen_id,
    base::expected<std::tuple<>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to stop stream: " << result.error();
    return;
  }
  if (streams_.erase(screen_id) != 0) {
    observers_.Notify(&Observer::OnPipewireCaptureStreamRemoved, screen_id);
  }
}

void PipewireCaptureStreamManager::OnPipeWireStreamAdded(
    std::string mapping_id,
    std::tuple<std::uint32_t> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_stream_);
  DCHECK(!pending_add_stream_requests_.empty());

  // Ensure method is only run this once per stream.
  pending_stream_added_signal_.reset();

  pending_stream_->SetPipeWireStream(
      get<0>(args),
      pending_add_stream_requests_.front().initial_resolution.dimensions(),
      mapping_id, webrtc::kInvalidPipeWireFd);
  // Start capturing now, which creates the virtual monitor and allows the
  // video capturer to be created.
  pending_stream_->StartVideoCapture();
}

void PipewireCaptureStreamManager::OnGnomeDisplayConfigChanged(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // See comment in AddStream().
  if (!last_seen_display_config_.has_value()) {
    DCHECK(!pending_stream_);
    last_seen_display_config_ = config;
    if (!pending_add_stream_requests_.empty()) {
      HOST_LOG << "Adding stream after initial display config is loaded.";
      MaybeAddStreamForCurrentRequest();
    }
    SetUseDamageRegion();
    return;
  }
  if (!pending_stream_) {
    last_seen_display_config_ = config;
    SetUseDamageRegion();
    return;
  }

  GnomeDisplayConfig previous_config = std::move(*last_seen_display_config_);
  last_seen_display_config_ = config;

  std::vector<webrtc::ScreenId> new_screen_ids;
  for (const auto& [name, monitor] : last_seen_display_config_->monitors) {
    if (!previous_config.monitors.contains(name)) {
      new_screen_ids.push_back(GnomeDisplayConfig::GetScreenId(name));
    }
  }
  if (new_screen_ids.empty()) {
    LOG(WARNING)
        << "No new screen ID to be associated with the pending stream.";
    return;
  }
  if (new_screen_ids.size() > 1) {
    LOG(WARNING) << "Multiple new screen IDs are found. The lowest screen "
                 << "ID will be associated with the pending stream.";
    // Ensure that screen IDs less than `screen_id_adjustment` will be chosen
    // over the other IDs.
    std::sort(new_screen_ids.begin(), new_screen_ids.end());
  }
  AssociatePendingStream(new_screen_ids.front());
}

void PipewireCaptureStreamManager::AssociatePendingStream(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!streams_.contains(screen_id));

  if (!pending_stream_) {
    LOG(WARNING) << "There is no pending stream to associate with.";
    return;
  }
  HOST_LOG << "Associating pending stream with screen ID " << screen_id;
  pending_stream_->set_screen_id(screen_id);
  auto weak_ptr = pending_stream_->GetWeakPtr();
  StreamInfo info;
  info.stream = std::move(pending_stream_);
  info.stream_path = std::move(pending_stream_path_);
  streams_[screen_id] = std::move(info);

  SetUseDamageRegion();
  RunCurrentAddStreamCallback(base::ok(weak_ptr));
}

void PipewireCaptureStreamManager::SetUseDamageRegion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& [screen_id, stream_info] : streams_) {
    const auto monitor_it = last_seen_display_config_->FindMonitor(screen_id);
    if (monitor_it == last_seen_display_config_->monitors.end()) {
      LOG(ERROR) << "Cannot find monitor for screen ID " << screen_id;
      continue;
    }
    // Given mutter's bug with the reported damage region, it is only safe to
    // enable damage region if the monitor is at the top-left corner with
    // 100% scaling.
    // See: https://gitlab.gnome.org/GNOME/mutter/-/issues/4269
    bool use_damage_region = monitor_it->second.x == 0 &&
                             monitor_it->second.y == 0 &&
                             monitor_it->second.scale == 1.0;
    stream_info.stream->SetUseDamageRegion(use_damage_region);
  }
}

}  // namespace remoting
