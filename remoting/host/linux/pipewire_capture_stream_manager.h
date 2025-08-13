// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_MANAGER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
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
class PipewireCaptureStreamManager final {
 public:
  using AddStreamResult =
      base::expected<base::WeakPtr<PipewireCaptureStream>, std::string>;
  using AddStreamCallback = base::OnceCallback<void(AddStreamResult)>;

  // An interface for observing stream additions and removals.
  class Observer : public base::CheckedObserver {
   public:
    using Subscription = base::ScopedClosureRunner;

    virtual void OnPipewireCaptureStreamAdded(
        base::WeakPtr<PipewireCaptureStream> stream) {}
    virtual void OnPipewireCaptureStreamRemoved(webrtc::ScreenId screen_id) {}

   protected:
    Observer() = default;
  };

  PipewireCaptureStreamManager();
  PipewireCaptureStreamManager(const PipewireCaptureStreamManager&) = delete;
  PipewireCaptureStreamManager& operator=(const PipewireCaptureStreamManager&) =
      delete;
  ~PipewireCaptureStreamManager();

  // Adds an observer. Discarding the returned subscription will result in the
  // removal of the observer.
  [[nodiscard]] Observer::Subscription AddObserver(Observer* observer);

  // Initializes the stream manager. Must be call once before calling other
  // methods of this class. `connection` must outlive `this`.
  void Init(GDBusConnectionRef* connection,
            base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client,
            gvariant::ObjectPath screencast_session_path);

  // Returns the stream associated with `screen_id`. A non-null result will only
  // be returned if the AddStreamCallback passed to the AddStream() method has
  // been called.
  base::WeakPtr<PipewireCaptureStream> GetStream(
      webrtc::ScreenId screen_id) const;

  // Adds a new PipewireCaptureStream and creates the corresponding virtual
  // display with the specified initial resolution. `callback` is called once
  // the stream is successfully added or failed to be added.
  void AddStream(const ScreenResolution& initial_resolution,
                 AddStreamCallback callback);

  // Removes a stream and destroys the corresponding virtual display.
  void RemoveStream(webrtc::ScreenId screen_id);

  // Returns all active streams.
  base::flat_map<webrtc::ScreenId, base::WeakPtr<PipewireCaptureStream>>
  GetActiveStreams() const;

  // Returns a weak pointer to this object.
  base::WeakPtr<PipewireCaptureStreamManager> GetWeakPtr();

 private:
  struct AddStreamRequest {
    AddStreamRequest();
    AddStreamRequest(AddStreamRequest&&);
    AddStreamRequest(const ScreenResolution& initial_resolution,
                     AddStreamCallback callback);
    ~AddStreamRequest();

    ScreenResolution initial_resolution;
    AddStreamCallback callback;
  };

  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckAddStreamResultAndContinue(
      void (PipewireCaptureStreamManager::*success_method)(SuccessType),
      String&& error_context);

  void RemoveObserver(Observer* observer);

  // Adds a new stream if `pending_add_stream_requests_` is non-empty, otherwise
  // do nothing. Must be called when there is no pending stream.
  void MaybeAddStreamForCurrentRequest();

  // Runs the current AddStreamCallback, removes it from
  // `pending_add_stream_requests_`, then adds another stream if
  // `pending_add_stream_requests_` is still not empty.
  void RunCurrentAddStreamCallback(AddStreamResult result);

  void OnAddStreamError(std::string_view what, Loggable why);
  void OnStreamCreated(std::tuple<gvariant::ObjectPath> args);
  void OnStreamParameters(GVariantRef<"a{sv}"> parameters);
  void OnStreamStarted(std::tuple<> args);
  void OnPipeWireStreamAdded(std::string mapping_id,
                             std::tuple<std::uint32_t> args);
  void QueryDisplayInfo();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);

  // Associates the pending stream with `screen_id`, then calls
  // RunCurrentAddStreamCallback() with the screen ID.
  void AssociatePendingStream(webrtc::ScreenId screen_id);

  raw_ptr<GDBusConnectionRef> connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath screencast_session_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GnomeDisplayConfigDBusClient::Subscription>
      monitors_changed_subscription_;
  // nullopt if not display config has been loaded yet.
  std::optional<GnomeDisplayConfig> last_seen_display_config_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<webrtc::ScreenId, std::unique_ptr<PipewireCaptureStream>>
      streams_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Queue to allow streams to be added one at a time, which is crucial to
  // ensure the stream is associated with the correct screen ID.
  base::queue<AddStreamRequest> pending_add_stream_requests_
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
  base::WeakPtrFactory<PipewireCaptureStreamManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_MANAGER_H_
