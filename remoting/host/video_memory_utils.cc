// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_memory_utils.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/memory/writable_shared_memory_region.h"
#endif

namespace remoting {

//////////////////////////////////////////////////////////////////////////////
// SharedVideoMemory

// static
std::unique_ptr<SharedVideoMemory> SharedVideoMemory::Create(
    size_t size,
    int id,
    base::OnceClosure on_deleted_callback) {
  webrtc::SharedMemory::Handle handle = webrtc::SharedMemory::kInvalidHandle;
#if BUILDFLAG(IS_WIN)
  // webrtc::ScreenCapturer uses webrtc::SharedMemory::handle() only on
  // windows. This handle must be writable. A WritableSharedMemoryRegion is
  // created, and then it is converted to read-only.  On the windows platform,
  // it happens to be the case that converting a region to read-only does not
  // change the status of existing handles. This is not true on all other
  // platforms, so please don't emulate this behavior!
  base::WritableSharedMemoryRegion region =
      base::WritableSharedMemoryRegion::Create(size);
  if (!region.IsValid()) {
    return nullptr;
  }
  base::WritableSharedMemoryMapping mapping = region.Map();
  // Converting |region| to read-only will close its associated handle, so we
  // must duplicate it into the handle used for |webrtc::ScreenCapturer|.
  HANDLE process = ::GetCurrentProcess();
  BOOL success =
      ::DuplicateHandle(process, region.UnsafeGetPlatformHandle(), process,
                        &handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!success) {
    return nullptr;
  }
  base::ReadOnlySharedMemoryRegion read_only_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
#else
  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(size);
  base::ReadOnlySharedMemoryRegion read_only_region =
      std::move(region_mapping.region);
  base::WritableSharedMemoryMapping mapping = std::move(region_mapping.mapping);
#endif
  if (!mapping.IsValid()) {
    return nullptr;
  }
  // The SharedVideoMemory ctor is private, so std::make_unique can't be
  // used.
  return base::WrapUnique(
      new SharedVideoMemory(std::move(read_only_region), std::move(mapping),
                            handle, id, std::move(on_deleted_callback)));
}

SharedVideoMemory::~SharedVideoMemory() {
  std::move(on_deleted_callback_).Run();
}

SharedVideoMemory::SharedVideoMemory(base::ReadOnlySharedMemoryRegion region,
                                     base::WritableSharedMemoryMapping mapping,
                                     webrtc::SharedMemory::Handle handle,
                                     int id,
                                     base::OnceClosure on_deleted_callback)
    : SharedMemory(mapping.memory(), mapping.size(), handle, id),
      on_deleted_callback_(std::move(on_deleted_callback))
#if BUILDFLAG(IS_WIN)
      ,
      writable_handle_(handle)
#endif
{
  region_ = std::move(region);
  mapping_ = std::move(mapping);
}

//////////////////////////////////////////////////////////////////////////////
// SharedVideoMemoryFactory

SharedVideoMemoryFactory::SharedVideoMemoryFactory(
    SharedMemoryCreatedCallback shared_memory_created_callback,
    SharedMemoryReleasedCallback shared_memory_released_callback)
    : shared_memory_created_callback_(
          std::move(shared_memory_created_callback)),
      shared_memory_released_callback_(
          std::move(shared_memory_released_callback)) {}

SharedVideoMemoryFactory::~SharedVideoMemoryFactory() = default;

std::unique_ptr<webrtc::SharedMemory>
SharedVideoMemoryFactory::CreateSharedMemory(size_t size) {
  base::OnceClosure release_buffer_callback =
      base::BindOnce(shared_memory_released_callback_, next_shared_buffer_id_);
  std::unique_ptr<SharedVideoMemory> buffer = SharedVideoMemory::Create(
      size, next_shared_buffer_id_, std::move(release_buffer_callback));
  if (buffer) {
    // |next_shared_buffer_id_| starts from 1 and incrementing it by 2 makes
    // sure it is always odd and therefore zero is never used as a valid
    // buffer ID.
    //
    // It is very unlikely (though theoretically possible) to allocate the
    // same ID for two different buffers due to integer overflow. It should
    // take about a year of allocating 100 new buffers every second.
    // Practically speaking it never happens.
    next_shared_buffer_id_ += 2;

    shared_memory_created_callback_.Run(
        buffer->id(), buffer->region().Duplicate(), buffer->size());
  }

  return buffer;
}

//////////////////////////////////////////////////////////////////////////////
// IpcSharedBufferCore

IpcSharedBufferCore::IpcSharedBufferCore(
    int id,
    base::ReadOnlySharedMemoryRegion region)
    : id_(id) {
  mapping_ = region.Map();
  if (!mapping_.IsValid()) {
    LOG(ERROR) << "Failed to map a shared buffer: id=" << id
               << ", size=" << region.GetSize();
  }
  // After being mapped, |region| is no longer needed and can be discarded.
}

IpcSharedBufferCore::~IpcSharedBufferCore() = default;

//////////////////////////////////////////////////////////////////////////////
// IpcSharedBuffer

IpcSharedBuffer::IpcSharedBuffer(scoped_refptr<IpcSharedBufferCore> core)
    : SharedMemory(const_cast<void*>(core->memory()),
                   core->size(),
                   0,
                   core->id()),
      core_(core) {}

IpcSharedBuffer::~IpcSharedBuffer() = default;

}  // namespace remoting
