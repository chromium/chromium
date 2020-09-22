// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DXGI_DEVICE_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DXGI_DEVICE_MANAGER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "media/capture/capture_export.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDXGIDeviceManager
    : public base::RefCounted<VideoCaptureDXGIDeviceManager> {
 public:
  // Returns a VideoCaptureDXGIDeviceManager with associated D3D device set, or
  // nullptr on failure.
  static scoped_refptr<VideoCaptureDXGIDeviceManager> Create();

  // Associates a new D3D device with the DXGI Device Manager
  bool ResetDevice();

  // Registers this manager in capture engine attributes.
  void RegisterInCaptureEngineAttributes(IMFAttributes* attributes);

  // Registers this manager with a media source
  void RegisterWithMediaSource(
      Microsoft::WRL::ComPtr<IMFMediaSource> media_source);

 protected:
  friend class base::RefCounted<VideoCaptureDXGIDeviceManager>;
  VideoCaptureDXGIDeviceManager(
      Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
      UINT d3d_device_reset_token);
  virtual ~VideoCaptureDXGIDeviceManager();

  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager_;
  UINT d3d_device_reset_token_ = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DXGI_DEVICE_MANAGER_H_