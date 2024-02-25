// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "media/base/video_frame_converter.h"
#include "media/capture/video_capture_types.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "third_party/blink/public/common/media/video_capture.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {
class PepperPlatformVideoCapture;
class RendererPpapiHostImpl;

class PepperVideoCaptureHost : public ppapi::host::ResourceHost {
 public:
  PepperVideoCaptureHost(RendererPpapiHostImpl* host,
                         PP_Instance instance,
                         PP_Resource resource);

  PepperVideoCaptureHost(const PepperVideoCaptureHost&) = delete;
  PepperVideoCaptureHost& operator=(const PepperVideoCaptureHost&) = delete;

  ~PepperVideoCaptureHost() override;

  bool Init();

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // These methods are called by PepperPlatformVideoCapture only.

  // Called when video capture is initialized. We can start
  // video capture if |succeeded| is true.
  void OnInitialized(bool succeeded);

  // Called when video capture has started successfully.
  void OnStarted();

  // Called when video capture has stopped. There will be no more
  // frames delivered.
  void OnStopped();

  // Called when video capture has paused.
  void OnPaused();

  // Called when video capture cannot be started because of an error.
  void OnError();

  // Called when a video frame is ready.
  void OnFrameReady(scoped_refptr<media::VideoFrame> frame);

 private:
  int32_t OnOpen(ppapi::host::HostMessageContext* context,
                 const std::string& device_id,
                 const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                 uint32_t buffer_count);
  int32_t OnStartCapture(ppapi::host::HostMessageContext* context);
  int32_t OnReuseBuffer(ppapi::host::HostMessageContext* context,
                        uint32_t buffer);
  int32_t OnStopCapture(ppapi::host::HostMessageContext* context);
  int32_t OnClose(ppapi::host::HostMessageContext* context);

  int32_t StopCapture();
  int32_t Close();
  void PostErrorReply();
  void AllocBuffers(const gfx::Size& resolution, int frame_rate);
  void ReleaseBuffers();
  void SendStatus();

  void SetRequestedInfo(const PP_VideoCaptureDeviceInfo_Dev& device_info,
                        uint32_t buffer_count);

  void DetachPlatformVideoCapture();

  bool SetStatus(PP_VideoCaptureStatus_Dev status, bool forced);

  std::unique_ptr<PepperPlatformVideoCapture> platform_video_capture_;

  // Buffers of video frame.
  struct BufferInfo {
    BufferInfo();
    BufferInfo(const BufferInfo& other);
    ~BufferInfo();

    bool in_use;
    raw_ptr<void> data;
    scoped_refptr<PPB_Buffer_Impl> buffer;
  };

  raw_ptr<RendererPpapiHostImpl> renderer_ppapi_host_;

  gfx::Size alloc_size_;
  std::vector<BufferInfo> buffers_;
  size_t buffer_count_hint_;

  media::VideoCaptureParams video_capture_params_;

  PP_VideoCaptureStatus_Dev status_;

  ppapi::host::ReplyMessageContext open_reply_context_;

  PepperDeviceEnumerationHostHelper enumeration_helper_;

  media::VideoFrameConverter frame_converter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_CAPTURE_HOST_H_
