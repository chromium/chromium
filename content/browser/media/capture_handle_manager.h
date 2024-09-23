// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_HANDLE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_HANDLE_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "media/mojo/mojom/capture_handle.mojom.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

// This class lives on the UI thread.
// TODO(crbug.com/40181897): Document this class.
class CONTENT_EXPORT CaptureHandleManager {
 public:
  using DeviceCaptureHandleChangeCallback = base::RepeatingCallback<void(
      const std::string& label,
      blink::mojom::MediaStreamType type,
      media::mojom::CaptureHandlePtr capture_handle)>;

  CaptureHandleManager();
  CaptureHandleManager(const CaptureHandleManager&) = delete;
  CaptureHandleManager& operator=(const CaptureHandleManager&) = delete;
  virtual ~CaptureHandleManager();

  // This method should be called after starting a video capture session of
  // a MediaStreamDevice, to subscribe for its CaptureHandle changes.
  // The permission to observe the CaptureHandle depends on |capturer|'s origin.
  void OnTabCaptureStarted(
      const std::string& label,
      const blink::MediaStreamDevice& captured_device,
      GlobalRenderFrameHostId capturer,
      DeviceCaptureHandleChangeCallback handle_change_callback);

  // Stops tracking a previously tracked capture session.
  void OnTabCaptureStopped(const std::string& label,
                           const blink::MediaStreamDevice& captured_device);

  // Should be called when devices change. It is essentially equivalent to
  // calling OnTabCaptureStopped() on all sessions with the right |label|,
  // then calling OnTabCaptureStarted() on all |new_devices|.
  void OnTabCaptureDevicesUpdated(
      const std::string& label,
      blink::mojom::StreamDevicesSetPtr new_stream_devices,
      GlobalRenderFrameHostId capturer,
      DeviceCaptureHandleChangeCallback handle_change_callback);

 private:
  // Tracks a single captured WebContents and informs its capturer of changes
  // to the CaptureHandle.
  class Observer;

  // Uniquely identifies a capture session.
  struct CaptureKey {
    bool operator<(const CaptureKey& other) const {
      return std::tie(label, type) < std::tie(other.label, other.type);
    }

    std::string label;
    blink::mojom::MediaStreamType type;
  };

  // All the information this manager holds about a given capture.
  struct CaptureInfo {
    CaptureInfo(std::unique_ptr<Observer> observer,
                media::mojom::CaptureHandlePtr last_capture_handle,
                DeviceCaptureHandleChangeCallback callback);
    ~CaptureInfo();

    std::unique_ptr<Observer> observer;
    media::mojom::CaptureHandlePtr last_capture_handle;
    DeviceCaptureHandleChangeCallback callback;
  };

  // Called by Observer whenever a new CaptureHandle config is observed.
  // Allows CaptureHandleManager to decide if a corresponding
  // DeviceCaptureHandleChangeCallback should be called.
  void OnCaptureHandleConfigUpdate(
      const std::string& label,
      blink::mojom::MediaStreamType type,
      media::mojom::CaptureHandlePtr capture_handle);

  // Maps each capture session to everything we hold about it - the observer
  // that tracks it, the last reported CaptureHandle, etc.
  std::map<CaptureKey, std::unique_ptr<CaptureInfo>> captures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_HANDLE_MANAGER_H_
