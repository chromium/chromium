// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CAPTURE_STREAM_MANAGER_H_
#define REMOTING_HOST_LINUX_CAPTURE_STREAM_MANAGER_H_

#include <functional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class CaptureStream;
class ScreenResolution;

// An interface for a class that allows for adding and removing a pipewire
// stream, and associating it with the screen ID.
//
// This interface manages two types of streams: "virtual stream", for which this
// interface manages the creation and deletion of the corresponding virtual
// monitor, and "monitor stream", for which the corresponding monitor is either
// physical, or virtual but managed by something other than this interface or
// process.
//
// Callers can create a new virtual stream and its corresponding virtual monitor
// by calling AddVirtualStream(), and delete it and its virtual monitor by
// calling RemoveVirtualStream(). If a new monitor that is not managed by this
// interface is found, a corresponding monitor stream for said monitor will be
// created, and OnPipewireCaptureStreamAdded() will be called; the monitor
// stream will be removed and OnPipewireCaptureStreamRemoved() will be called
// once the corresponding monitor no longer exists. Callers cannot remove a
// monitor stream with this interface.
class CaptureStreamManager {
 public:
  using AddStreamResult =
      base::expected<base::WeakPtr<CaptureStream>, std::string>;
  using AddStreamCallback = base::OnceCallback<void(AddStreamResult)>;

  // An interface for observing stream additions and removals.
  class Observer : public base::CheckedObserver {
   public:
    using Subscription = base::ScopedClosureRunner;

    virtual void OnPipewireCaptureStreamAdded(
        base::WeakPtr<CaptureStream> stream) {}
    virtual void OnPipewireCaptureStreamRemoved(webrtc::ScreenId screen_id) {}

   protected:
    ~Observer() override = default;
  };

  virtual ~CaptureStreamManager() = default;

  // Adds an observer. Discarding the returned subscription will result in the
  // removal of the observer.
  [[nodiscard]] virtual Observer::Subscription AddObserver(
      Observer* observer) = 0;

  // Returns the stream associated with `screen_id`. A non-null result will only
  // be returned if the AddStreamCallback passed to the AddVirtualStream()
  // method has been called.
  virtual base::WeakPtr<CaptureStream> GetStream(
      webrtc::ScreenId screen_id) = 0;

  base::WeakPtr<const CaptureStream> GetStream(
      webrtc::ScreenId screen_id) const {
    return const_cast<CaptureStreamManager*>(this)->GetStream(screen_id);
  }

  // Adds a new virtual stream and creates the corresponding virtual monitor
  // with the specified initial resolution. `callback` is called once the stream
  // is successfully added or failed to be added.
  virtual void AddVirtualStream(const ScreenResolution& initial_resolution,
                                AddStreamCallback callback) = 0;

  // Removes a virtual stream and destroys its corresponding virtual monitor.
  // This method cannot be used to remove a monitor stream.
  virtual void RemoveVirtualStream(webrtc::ScreenId screen_id) = 0;

  // Returns all active streams.
  virtual base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
  GetActiveStreams() = 0;

  base::flat_map<webrtc::ScreenId, base::WeakPtr<const CaptureStream>>
  GetActiveStreams() const {
    return base::MakeFlatMap<webrtc::ScreenId,
                             base::WeakPtr<const CaptureStream>>(
        const_cast<CaptureStreamManager*>(this)->GetActiveStreams(),
        std::less<>(), [](const auto& pair) {
          return std::make_pair(pair.first, pair.second);
        });
  }
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CAPTURE_STREAM_MANAGER_H_
