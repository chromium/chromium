// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_IMPL_H_

#include "base/memory/singleton.h"
#include "content/public/browser/media_capture_devices.h"
#include "media/base/video_facing.h"

namespace content {

class MediaCaptureDevicesImpl : public MediaCaptureDevices {
 public:
  static MediaCaptureDevicesImpl* GetInstance();

  MediaCaptureDevicesImpl(const MediaCaptureDevicesImpl&) = delete;
  MediaCaptureDevicesImpl& operator=(const MediaCaptureDevicesImpl&) = delete;

  // Overriden from MediaCaptureDevices
  const blink::MediaStreamDevices& GetAudioCaptureDevices() override;
  const blink::MediaStreamDevices& GetVideoCaptureDevices() override;
  void AddVideoCaptureObserver(media::VideoCaptureObserver* observer) override;
  void RemoveAllVideoCaptureObservers() override;

  // Called by MediaStreamManager to notify the change of media capture
  // devices, these 2 methods are called in IO thread.
  void OnAudioCaptureDevicesChanged(const blink::MediaStreamDevices& devices);
  void OnVideoCaptureDevicesChanged(const blink::MediaStreamDevices& devices);

 private:
  friend struct base::DefaultSingletonTraits<MediaCaptureDevicesImpl>;
  MediaCaptureDevicesImpl();
  ~MediaCaptureDevicesImpl() override;

  void UpdateAudioDevicesOnUIThread(const blink::MediaStreamDevices& devices);
  void UpdateVideoDevicesOnUIThread(const blink::MediaStreamDevices& devices);

  // Flag to indicate if device enumeration has been done/doing.
  // Only accessed on UI thread.
  bool devices_enumerated_;

  // A list of cached audio capture devices.
  blink::MediaStreamDevices audio_devices_;

  // A list of cached video capture devices.
  blink::MediaStreamDevices video_devices_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_IMPL_H_
