// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_ACTIVITY_FLAGS_H_
#define GPU_COMMAND_BUFFER_COMMON_ACTIVITY_FLAGS_H_

#include "base/atomicops.h"
#include "gpu/gpu_export.h"
#include "mojo/public/cpp/system/buffer.h"

namespace gpu {

// Base class for GpuProcessActivityFlags and GpuProcessHostActivityFlags,
// can not be used directly.
class GPU_EXPORT ActivityFlagsBase {
 public:
  enum Flag : uint32_t { FLAG_LOADING_PROGRAM_BINARY = 0x1 };

 protected:
  ActivityFlagsBase();
  ActivityFlagsBase(ActivityFlagsBase&& other);
  ~ActivityFlagsBase();

  void Initialize(mojo::ScopedSharedBufferHandle handle);
  const mojo::SharedBufferHandle& handle() const { return handle_.get(); }
  bool is_initialized() const { return handle().is_valid(); }

 protected:
  volatile base::subtle::Atomic32* AsAtomic();

 private:
  mojo::ScopedSharedBufferHandle handle_;
  mojo::ScopedSharedBufferMapping mapping_;
};

// Provides write-only access to activity flags for the gpu process. Each gpu
// process has a singleton GpuProcessActivityFlags retreived via GetInstance().
//
// Note that we currently assume that the GPU process never sets/unsets flags
// from multiple threads at the same time. This is true with our current
// single-flag approach, but may need adjustment if additional flags are added.
class GPU_EXPORT GpuProcessActivityFlags : public ActivityFlagsBase {
 public:
  class ScopedSetFlag {
   public:
    ScopedSetFlag(GpuProcessActivityFlags* activity_flags, Flag flag)
        : activity_flags_(activity_flags), flag_(flag) {
      activity_flags_->SetFlag(flag_);
    }
    ~ScopedSetFlag() { activity_flags_->UnsetFlag(flag_); }

   private:
    GpuProcessActivityFlags* activity_flags_;
    Flag flag_;
  };

  GpuProcessActivityFlags();
  GpuProcessActivityFlags(GpuProcessActivityFlags&& other);
  GpuProcessActivityFlags(mojo::ScopedSharedBufferHandle handle);

 private:
  void SetFlag(Flag flag);
  void UnsetFlag(Flag flag);
};

// Provides read-only access to activity flags. Creating a new
// GpuProcessHostActivityFlags will initialize a new mojo shared buffer. The
// handle to this buffer should be passed to the GPU process via CloneHandle.
// The GPU process will then populate flags, which can be read via this class.
class GPU_EXPORT GpuProcessHostActivityFlags : public ActivityFlagsBase {
 public:
  GpuProcessHostActivityFlags();

  bool IsFlagSet(Flag flag);
  mojo::ScopedSharedBufferHandle CloneHandle() {
    return handle().Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);
  }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_ACTIVITY_FLAGS_H_
