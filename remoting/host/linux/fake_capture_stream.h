// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_FAKE_CAPTURE_STREAM_H_
#define REMOTING_HOST_LINUX_FAKE_CAPTURE_STREAM_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "remoting/host/linux/capture_stream.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class FakeCaptureStream : public CaptureStream {
 public:
  FakeCaptureStream();
  ~FakeCaptureStream() override;

  MOCK_METHOD(void,
              SetPipeWireStream,
              (std::uint32_t pipewire_node,
               const webrtc::DesktopSize& initial_resolution,
               std::string_view mapping_id,
               int pipewire_fd),
              (override));
  MOCK_METHOD(void, StartVideoCapture, (), (override));
  MOCK_METHOD(void,
              SetCallback,
              (base::WeakPtr<webrtc::DesktopCapturer::Callback> callback),
              (override));
  MOCK_METHOD(void, SetUseDamageRegion, (bool use_damage_region), (override));
  MOCK_METHOD(void, SetMaxFrameRate, (std::uint32_t frame_rate), (override));
  MOCK_METHOD(std::unique_ptr<webrtc::MouseCursor>,
              CaptureCursor,
              (),
              (override));
  MOCK_METHOD(std::optional<webrtc::DesktopVector>,
              CaptureCursorPosition,
              (),
              (override));

  MOCK_METHOD(std::string_view, mapping_id, (), (const, override));

  CursorObserver::Subscription AddCursorObserver(
      CursorObserver* observer) override;

  void SetResolution(const webrtc::DesktopSize& new_resolution) override;

  const webrtc::DesktopSize& resolution() const override;

  void set_screen_id(webrtc::ScreenId screen_id) override;

  webrtc::ScreenId screen_id() const override;

  base::WeakPtr<CaptureStream> GetWeakPtr() override;

  template <typename Method, typename... Args>
    requires std::
        invocable<Method, CursorObserver*, CaptureStream*, const Args&...>
      void NotifyCursorObservers(Method method, const Args&... args) {
    cursor_observers.Notify(method, this, args...);
  }

  base::ObserverList<CursorObserver> cursor_observers;

 private:
  void RemoveCursorObserver(CursorObserver* observer);

  webrtc::DesktopSize resolution_;
  webrtc::ScreenId screen_id_;
  base::WeakPtrFactory<FakeCaptureStream> weak_ptr_factory_{this};
};

class FakeCaptureStreamManager : public CaptureStreamManager {
 public:
  FakeCaptureStreamManager();
  ~FakeCaptureStreamManager() override;

  Observer::Subscription AddObserver(Observer* observer) override;

  void AddVirtualStream(const ScreenResolution& initial_resolution,
                        AddStreamCallback callback) override;

  base::WeakPtr<CaptureStream> AddVirtualStream(
      webrtc::ScreenId screen_id,
      const webrtc::DesktopSize& resolution);

  void RemoveVirtualStream(webrtc::ScreenId screen_id) override;

  base::WeakPtr<CaptureStream> GetStream(webrtc::ScreenId screen_id) override;

  FakeCaptureStream* GetFakeStream(webrtc::ScreenId screen_id);

  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
  GetActiveStreams() override;

  base::WeakPtr<CaptureStreamManager> GetWeakPtr();

  // The screen ID to be assigned for the new stream added via the overridden
  // AddStream method.
  std::optional<webrtc::ScreenId> next_screen_id;

  base::ObserverList<Observer> observers;

 private:
  void RemoveObserver(Observer* observer);

  base::flat_map<webrtc::ScreenId, std::unique_ptr<FakeCaptureStream>> streams_;
  base::WeakPtrFactory<FakeCaptureStreamManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_FAKE_CAPTURE_STREAM_H_
