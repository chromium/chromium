// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_CAPTURE_DEVICES_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_CAPTURE_DEVICES_H_

#include "content/common/content_export.h"
#include "media/base/video_facing.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

// This is a singleton class, used to get Audio/Video devices, it must be
// called in UI thread.
class CONTENT_EXPORT  MediaCaptureDevices {
 public:
  // Get singleton instance of MediaCaptureDevices.
  static MediaCaptureDevices* GetInstance();

  // Return all Audio/Video devices.
  virtual const blink::MediaStreamDevices& GetAudioCaptureDevices() = 0;
  virtual const blink::MediaStreamDevices& GetVideoCaptureDevices() = 0;

  virtual void AddVideoCaptureObserver(
      media::VideoCaptureObserver* observer) = 0;
  virtual void RemoveAllVideoCaptureObservers() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class MediaCaptureDevicesImpl;
  MediaCaptureDevices() {}
  virtual ~MediaCaptureDevices() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_CAPTURE_DEVICES_H_
