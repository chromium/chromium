// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_MEMORY_UTILS_H_
#define REMOTING_HOST_VIDEO_MEMORY_UTILS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif  // BUILDFLAG(IS_WIN)

// Helper classes to implement sharing of captured video frames over an IPC
// channel.
namespace remoting {

// webrtc::SharedMemory implementation that creates a
// base::ReadOnlySharedMemoryRegion along with a writable mapping.
// The writable mapping is used by the real video-capturer in the Desktop
// process, and the read-only region is sent over the IPC channel.
class SharedVideoMemory : public webrtc::SharedMemory {
 public:
  static std::unique_ptr<SharedVideoMemory>
  Create(size_t size, int id, base::OnceClosure on_deleted_callback);

  SharedVideoMemory(const SharedVideoMemory&) = delete;
  SharedVideoMemory& operator=(const SharedVideoMemory&) = delete;

  ~SharedVideoMemory() override;

  const base::ReadOnlySharedMemoryRegion& region() const { return region_; }

 private:
  SharedVideoMemory(base::ReadOnlySharedMemoryRegion region,
                    base::WritableSharedMemoryMapping mapping,
                    webrtc::SharedMemory::Handle handle,
                    int id,
                    base::OnceClosure on_deleted_callback);

  base::OnceClosure on_deleted_callback_;
  base::ReadOnlySharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
#if BUILDFLAG(IS_WIN)
  // Owns the handle passed to the base class which is used by
  // webrtc::ScreenCapturer.
  base::win::ScopedHandle writable_handle_;
#endif
};

// A webrtc::SharedMemoryFactory implementation which creates
// SharedVideoMemory objects for the real video-capturer in the Desktop
// process. It notifies callbacks when memory is created and released.
class SharedVideoMemoryFactory : public webrtc::SharedMemoryFactory {
 public:
  using SharedMemoryCreatedCallback = base::RepeatingCallback<
      void(int id, base::ReadOnlySharedMemoryRegion, uint32_t size)>;
  using SharedMemoryReleasedCallback = base::RepeatingCallback<void(int id)>;

  SharedVideoMemoryFactory(
      SharedMemoryCreatedCallback shared_memory_created_callback,
      SharedMemoryReleasedCallback shared_memory_released_callback);

  SharedVideoMemoryFactory(const SharedVideoMemoryFactory&) = delete;
  SharedVideoMemoryFactory& operator=(const SharedVideoMemoryFactory&) = delete;

  ~SharedVideoMemoryFactory() override;

  std::unique_ptr<webrtc::SharedMemory> CreateSharedMemory(
      size_t size) override;

 private:
  int next_shared_buffer_id_ = 1;
  SharedMemoryCreatedCallback shared_memory_created_callback_;
  SharedMemoryReleasedCallback shared_memory_released_callback_;
};

// A wrapper for read-only memory received over IPC. This creates and stores a
// ReadOnlySharedMemoryMapping for the received memory-region.
class IpcSharedBufferCore
    : public base::RefCountedThreadSafe<IpcSharedBufferCore> {
 public:
  IpcSharedBufferCore(int id, base::ReadOnlySharedMemoryRegion region);

  IpcSharedBufferCore(const IpcSharedBufferCore&) = delete;
  IpcSharedBufferCore& operator=(const IpcSharedBufferCore&) = delete;

  int id() const { return id_; }
  size_t size() const { return mapping_.size(); }
  const void* memory() const { return mapping_.memory(); }

 private:
  ~IpcSharedBufferCore();

  friend class base::RefCountedThreadSafe<IpcSharedBufferCore>;

  int id_;
  base::ReadOnlySharedMemoryMapping mapping_;
};

// A webrtc::SharedMemory implementation which wraps an IpcSharedBufferCore.
// This is used for the `shared_memory` field of a webrtc::DesktopFrame in the
// Network process.
class IpcSharedBuffer : public webrtc::SharedMemory {
 public:
  explicit IpcSharedBuffer(scoped_refptr<IpcSharedBufferCore> core);

  IpcSharedBuffer(const IpcSharedBuffer&) = delete;
  IpcSharedBuffer& operator=(const IpcSharedBuffer&) = delete;

  ~IpcSharedBuffer() override;

 private:
  scoped_refptr<IpcSharedBufferCore> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_MEMORY_UTILS_H_
