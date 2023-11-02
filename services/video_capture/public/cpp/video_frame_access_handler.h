// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_FRAME_ACCESS_HANDLER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_FRAME_ACCESS_HANDLER_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// A referernce counted map of buffer IDs <-> ScopedAccessPermissions. This
// lives on the sender side and the reference counting ensures that the map
// stays alive until all mojo pipes are disconnected, i.e. until all
// VideoFrameAccessHandlerImpl are destroyed.
class ScopedAccessPermissionMap
    : public base::RefCountedThreadSafe<ScopedAccessPermissionMap> {
 public:
  // Creates a new map with an associated VideoFrameAccessHandlerImpl that is
  // sent to |video_frame_handler|'s OnFrameAccessHandlerReady().
  static scoped_refptr<ScopedAccessPermissionMap>
  CreateMapAndSendVideoFrameAccessHandlerReady(
      mojo::Remote<mojom::VideoFrameHandler>& video_frame_handler);

  ScopedAccessPermissionMap();

  void InsertAccessPermission(
      int32_t buffer_id,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          scoped_access_permission);

  void EraseAccessPermission(int32_t buffer_id);

 private:
  friend class base::RefCountedThreadSafe<ScopedAccessPermissionMap>;
  ~ScopedAccessPermissionMap();

  std::map<
      int32_t,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>>
      scoped_access_permissions_by_buffer_id_;
};

// The implementation of mojom::VideoFrameAccessHandler lives on the sender side
// and handles buffers being released through IPC calls.
class VideoFrameAccessHandlerImpl : public mojom::VideoFrameAccessHandler {
 public:
  explicit VideoFrameAccessHandlerImpl(
      scoped_refptr<ScopedAccessPermissionMap> scoped_access_permission_map);
  ~VideoFrameAccessHandlerImpl() override;

  void OnFinishedConsumingBuffer(int32_t buffer_id) override;

 private:
  const scoped_refptr<ScopedAccessPermissionMap> scoped_access_permission_map_;
};

// A reference counted object owning a
// mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>. This lives on
// the receiver side and keeps the mojo pipe open while being alive. While the
// mojo pipe is open, the mojom::VideoFrameAccessHandler implementation (e.g.
// VideoFrameAccessHandlerImpl) is also kept alive on the sender side.
class VideoFrameAccessHandlerRemote
    : public base::RefCounted<VideoFrameAccessHandlerRemote> {
 public:
  explicit VideoFrameAccessHandlerRemote(
      mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>
          frame_access_handler);

  mojom::VideoFrameAccessHandler* operator->();

 private:
  friend class base::RefCounted<VideoFrameAccessHandlerRemote>;
  ~VideoFrameAccessHandlerRemote();

  mojo::Remote<mojom::VideoFrameAccessHandler> frame_access_handler_;
};

// Forwards OnFinishedConsumingBuffer() calls to VideoFrameAccessHandlerRemote.
// This can be used by adapters that act as the middleman between a producer of
// frames and mojom::VideoFrameHandler.
class VideoFrameAccessHandlerForwarder : public mojom::VideoFrameAccessHandler {
 public:
  // Creates a new forwarder that is sent to |video_frame_handler|'s
  // OnFrameAccessHandlerReady().
  static void CreateForwarderAndSendVideoFrameAccessHandlerReady(
      mojo::Remote<mojom::VideoFrameHandler>& video_frame_handler,
      scoped_refptr<VideoFrameAccessHandlerRemote> video_frame_handler_remote);

  explicit VideoFrameAccessHandlerForwarder(
      scoped_refptr<VideoFrameAccessHandlerRemote> video_frame_handler_remote);
  ~VideoFrameAccessHandlerForwarder() override;

  void OnFinishedConsumingBuffer(int32_t buffer_id) override;

 private:
  const scoped_refptr<VideoFrameAccessHandlerRemote>
      video_frame_handler_remote_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_FRAME_ACCESS_HANDLER_H_
