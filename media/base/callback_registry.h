// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CALLBACK_REGISTRY_H_
#define MEDIA_BASE_CALLBACK_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

// A class that keeps a callback registered. The callback will be unregistered
// upon destruction of this object.
class CallbackRegistration {
 public:
  CallbackRegistration() = default;
  virtual ~CallbackRegistration() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackRegistration);
};

template <typename Sig>
class CallbackRegistry;

// A helper class that can register, unregister callbacks, and notify registered
// callbacks. This class is thread safe: all methods can be called on any
// thread. The CallbackRegistry must outlive all CallbackRegistrations returned
// by Register().
// TODO(xhwang): This class is similar to base::CallbackList, but is simpler,
// and provides thread safty. Consider merging these two.
template <typename... Args>
class CallbackRegistry<void(Args...)> {
 public:
  using CallbackType = base::RepeatingCallback<void(Args...)>;

  CallbackRegistry() = default;
  ~CallbackRegistry() = default;

  std::unique_ptr<CallbackRegistration> Register(CallbackType cb)
      WARN_UNUSED_RESULT {
    base::AutoLock lock(lock_);
    DCHECK(cb);
    uint32_t registration_id = ++next_registration_id_;
    DVLOG(1) << __func__ << ": registration_id = " << registration_id;

    // Use BindToCurrentLoop so that the callbacks are always posted to the
    // thread where Register() is called. Also, this helps avoid reentrancy
    // and deadlock issues, e.g. Register() is called in one of the callbacks.
    callbacks_[registration_id] = BindToCurrentLoop(std::move(cb));

    return std::make_unique<RegistrationImpl>(this, registration_id);
  }

  void Notify(Args&&... args) {
    DVLOG(1) << __func__;
    base::AutoLock lock(lock_);
    for (auto const& entry : callbacks_)
      entry.second.Run(std::forward<Args>(args)...);
  }

 private:
  class RegistrationImpl : public CallbackRegistration {
   public:
    RegistrationImpl(CallbackRegistry<void(Args...)>* registry,
                     uint32_t registration_id)
        : registry_(registry), registration_id_(registration_id) {}

    ~RegistrationImpl() override { registry_->Unregister(registration_id_); }

   private:
    CallbackRegistry<void(Args...)>* registry_ = nullptr;
    uint32_t registration_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RegistrationImpl);
  };

  void Unregister(uint32_t registration_id) {
    DVLOG(1) << __func__ << ": registration_id = " << registration_id;
    base::AutoLock lock(lock_);
    size_t num_callbacks_removed = callbacks_.erase(registration_id);
    DCHECK_EQ(num_callbacks_removed, 1u);
  }

  base::Lock lock_;
  uint32_t next_registration_id_ GUARDED_BY(lock_) = 0;
  std::map<uint32_t, CallbackType> callbacks_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(CallbackRegistry);
};

using ClosureRegistry = CallbackRegistry<void()>;

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_REGISTRY_H_
