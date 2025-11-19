// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_CAPTURE_STREAM_MANAGER_H_
#define REMOTING_HOST_LINUX_GNOME_CAPTURE_STREAM_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "remoting/host/linux/gnome_display_config_monitor.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// A class that allows for adding and removing a pipewire stream, and
// associating it with the screen ID.
//
// The screen ID is computed from the connector name of the virtual display.
// While mutter internally knows the connector name of the pipewire stream,
// it is not included in any of the results of the org.gnome.Mutter.ScreenCast
// D-Bus API. There is a mutter bug to make it happen:
// https://gitlab.gnome.org/GNOME/mutter/-/issues/3812
//
// For now, this class just watches for changes of the display config and tries
// to associate a newly created stream with a monitor that we have not seen
// before. We try to create streams one at a time, which minimizes the
// possibility of race conditions, but virtual or physical monitors that are
// not managed by this class could potentially still cause issues if they are
// added during stream creation.
class GnomeCaptureStreamManager final : public CaptureStreamManager {
 public:
  // Inherit overloads from the base class.
  using CaptureStreamManager::GetActiveStreams;
  using CaptureStreamManager::GetStream;

  GnomeCaptureStreamManager();
  GnomeCaptureStreamManager& operator=(const GnomeCaptureStreamManager&) =
      delete;
  ~GnomeCaptureStreamManager() override;

  // CaptureStreamManager implementation:
  [[nodiscard]] Observer::Subscription AddObserver(Observer* observer) override;
  base::WeakPtr<CaptureStream> GetStream(webrtc::ScreenId screen_id) override;
  void AddVirtualStream(const ScreenResolution& initial_resolution,
                        AddStreamCallback callback) override;
  void RemoveVirtualStream(webrtc::ScreenId screen_id) override;
  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
  GetActiveStreams() override;

  // Initializes the stream manager. Must be call once before calling other
  // methods of this class. `connection` must outlive `this`.
  void Init(GDBusConnectionRef* connection,
            base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor,
            gvariant::ObjectPath screencast_session_path);

  // Returns a weak pointer to this object.
  base::WeakPtr<GnomeCaptureStreamManager> GetWeakPtr();

 private:
  struct AddStreamRequest {
    struct VirtualStreamInfo {
      ScreenResolution initial_resolution;
    };

    struct MonitorStreamInfo {
      std::string connector;
    };

    AddStreamRequest();
    AddStreamRequest(AddStreamRequest&&);
    AddStreamRequest(VirtualStreamInfo virtual_stream_info,
                     AddStreamCallback callback);
    AddStreamRequest(MonitorStreamInfo monitor_stream_info,
                     AddStreamCallback callback);
    ~AddStreamRequest();

    // See documentation of CaptureStreamManager about virtual stream vs monitor
    // stream.

    // If set, the Mutter RecordVirtual() D-Bus API will be called.
    std::optional<VirtualStreamInfo> virtual_stream_info;

    // If set, the Mutter RecordMonitor() D-Bus API will be called.
    std::optional<MonitorStreamInfo> monitor_stream_info;

    // Called after the stream is added to the active streams list and is ready
    // to be used.
    AddStreamCallback callback;
  };

  struct StreamInfo {
    StreamInfo();
    StreamInfo(StreamInfo&&);
    StreamInfo& operator=(StreamInfo&&);
    ~StreamInfo();

    std::unique_ptr<PipewireCaptureStream> stream;
    gvariant::ObjectPath stream_path;
    bool is_virtual_stream = true;
    bool is_deleting = false;
  };

  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckAddStreamResultAndContinue(
      void (GnomeCaptureStreamManager::*success_method)(SuccessType),
      String&& error_context);

  void RemoveStream(webrtc::ScreenId screen_id, bool can_remove_monitor_stream);

  void RemoveObserver(Observer* observer);

  // Adds a new stream if `pending_add_stream_requests_` is non-empty, otherwise
  // do nothing. Must be called when there is no pending stream.
  void MaybeAddStreamForCurrentRequest();

  // Adds monitor streams for monitors in `last_seen_display_config_` that are
  // not in `streams_` and not in `pending_add_stream_requests_`.
  void MaybeAddMonitorStreams();

  // Remove virtual and monitor streams that are no longer in
  // `last_seen_display_config_` and not being deleted.
  void RemoveInvalidStreams();

  // Runs the current AddStreamCallback, removes it from
  // `pending_add_stream_requests_`, then adds another stream if
  // `pending_add_stream_requests_` is still not empty.
  void RunCurrentAddStreamCallback(AddStreamResult result);

  void OnAddStreamError(std::string_view what, Loggable why);
  void OnStreamCreated(std::tuple<gvariant::ObjectPath> args);
  void OnStreamParameters(GVariantRef<"a{sv}"> parameters);
  void OnStreamStarted(std::tuple<> args);
  void OnStreamStopped(webrtc::ScreenId screen_id,
                       base::expected<std::tuple<>, Loggable> result);
  void OnPipeWireStreamAdded(std::string mapping_id,
                             std::tuple<std::uint32_t> args);
  void OnGnomeDisplayConfigChanged(const GnomeDisplayConfig& config);

  // Associates the pending stream with `screen_id`, then calls
  // RunCurrentAddStreamCallback() with the screen ID.
  void AssociatePendingStream(webrtc::ScreenId screen_id);

  // Sets whether damage region should be used for each active stream based on
  // the desktop geometry.
  void SetUseDamageRegion();

  raw_ptr<GDBusConnectionRef> connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath screencast_session_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GnomeDisplayConfigMonitor::Subscription>
      monitors_changed_subscription_;
  // nullopt if not display config has been loaded yet.
  std::optional<GnomeDisplayConfig> last_seen_display_config_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<webrtc::ScreenId, StreamInfo> streams_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Queue to allow streams to be added one at a time, which is crucial to
  // ensure the stream is associated with the correct screen ID.
  base::circular_deque<AddStreamRequest> pending_add_stream_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The stream that is being created. It is only non-null during stream
  // creation, then it will either be moved to `streams_`, or reset in case of
  // failure.
  std::unique_ptr<PipewireCaptureStream> pending_stream_
      GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath pending_stream_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      pending_stream_added_signal_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GnomeCaptureStreamManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_CAPTURE_STREAM_MANAGER_H_
