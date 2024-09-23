// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"

namespace gpu {
class Buffer;

class GPU_GLES2_EXPORT ServiceFontManager
    : public base::RefCountedThreadSafe<ServiceFontManager> {
 public:
  class GPU_GLES2_EXPORT Client {
   public:
    virtual ~Client() {}
    virtual scoped_refptr<Buffer> GetShmBuffer(uint32_t shm_id) = 0;
    virtual void ReportProgress() = 0;
  };

  ServiceFontManager(Client* client, bool disable_oopr_debug_crash_dump);
  void Destroy();

  bool Deserialize(const volatile uint8_t* memory,
                   uint32_t memory_size,
                   std::vector<SkDiscardableHandleId>* locked_handles);
  bool Unlock(const std::vector<SkDiscardableHandleId>& handles);
  SkStrikeClient* strike_client() { return strike_client_.get(); }
  bool disable_oopr_debug_crash_dump() const {
    return disable_oopr_debug_crash_dump_;
  }

 private:
  friend class base::RefCountedThreadSafe<ServiceFontManager>;
  class SkiaDiscardableManager;

  ~ServiceFontManager();

  bool AddHandle(SkDiscardableHandleId handle_id,
                 ServiceDiscardableHandle handle)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool DeleteHandle(SkDiscardableHandleId handle_id);

  base::Lock lock_;

  raw_ptr<Client> client_ GUARDED_BY(lock_);
  const base::PlatformThreadId client_thread_id_;
  std::unique_ptr<SkStrikeClient> strike_client_;

  class Handle {
   public:
    explicit Handle(ServiceDiscardableHandle handle)
        : handle_(std::move(handle)) {}
    void Unlock() {
      --ref_count_;
      handle_.Unlock();
    }
    void Lock() { ++ref_count_; }
    bool Delete() { return handle_.Delete(); }
    int ref_count() const { return ref_count_; }
    bool IsLocked() const { return handle_.IsLockedForTesting(); }

   private:
    ServiceDiscardableHandle handle_;
    // ref count hold by GPU service.
    int ref_count_ = 0;
  };
  base::flat_map<SkDiscardableHandleId, Handle> discardable_handle_map_
      GUARDED_BY(lock_);
  bool destroyed_ GUARDED_BY(lock_) = false;
  const bool disable_oopr_debug_crash_dump_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_FONT_MANAGER_H_
