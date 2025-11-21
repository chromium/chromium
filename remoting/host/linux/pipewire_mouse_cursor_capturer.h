// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/linux/capture_stream.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/proto/coordinates.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

// A class that allows incarnations of PipewireMouseCursorMonitor to capture
// mouse cursor shapes and positions, and get the latest cursor shape before
// it is created.
class PipewireMouseCursorCapturer : public CaptureStreamManager::Observer,
                                    public CaptureStream::CursorObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    using Subscription = base::ScopedClosureRunner;

    virtual void OnCursorShapeChanged(PipewireMouseCursorCapturer* capturer) {}
    virtual void OnCursorPositionChanged(
        PipewireMouseCursorCapturer* capturer) {}
  };

  explicit PipewireMouseCursorCapturer(
      std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor,
      base::WeakPtr<CaptureStreamManager> stream_manager);
  ~PipewireMouseCursorCapturer() override;

  // Discarding the subscription object will remove the observer.
  [[nodiscard]] Observer::Subscription AddObserver(Observer* observer);

  // Returns a shared copy of the latest cursor, or nullptr if it's not
  // available.
  std::unique_ptr<webrtc::MouseCursor> GetLatestCursor();

  // Returns the latest global cursor position, or nullopt if it's not
  // available.
  const std::optional<webrtc::DesktopVector>& GetLatestGlobalCursorPosition();

  // Returns the latest fractional cursor position, or nullopt if it's not
  // available.
  const std::optional<protocol::FractionalCoordinate>&
  GetLatestFractionalCursorPosition();

  base::WeakPtr<PipewireMouseCursorCapturer> GetWeakPtr();

 private:
  friend class PipewireMouseCursorCapturerTest;

  struct MonitorInfo {
    double scale;
    int left;
    int top;
    int width;
    int height;
  };

  // CaptureStreamManager::Observer implementation.
  void OnPipewireCaptureStreamAdded(
      base::WeakPtr<CaptureStream> stream) override;
  void OnPipewireCaptureStreamRemoved(webrtc::ScreenId screen_id) override;

  // CaptureStream::CursorObserver implementation.
  void OnCursorShapeChanged(CaptureStream* stream) override;
  void OnCursorPositionChanged(CaptureStream* stream) override;

  void OnDisplayInfo();
  void RemoveObserver(Observer* observer);

  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<webrtc::SharedDesktopFrame> latest_cursor_frame_
      GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::DesktopVector latest_cursor_hotspot_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<webrtc::DesktopVector> latest_global_cursor_position_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<protocol::FractionalCoordinate>
      latest_fractional_cursor_position_ GUARDED_BY_CONTEXT(sequence_checker_);

 private:
  std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  CaptureStreamManager::Observer::Subscription stream_manager_subscription_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<webrtc::ScreenId, CaptureStream::CursorObserver::Subscription>
      stream_subscriptions_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<webrtc::ScreenId, MonitorInfo> monitors_
      GUARDED_BY_CONTEXT(sequence_checker_);
  DesktopDisplayInfo::PixelType pixel_type_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PipewireMouseCursorCapturer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_
