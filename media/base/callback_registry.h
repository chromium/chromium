// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CALLBACK_REGISTRY_H_
#define MEDIA_BASE_CALLBACK_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/thread_annotations.h"

namespace media {

// A class that keeps a callback registered. The callback will be unregistered
// upon destruction of this object.
class CallbackRegistration {
 public:
  CallbackRegistration() = default;

  CallbackRegistration(const CallbackRegistration&) = delete;
  CallbackRegistration& operator=(const CallbackRegistration&) = delete;

  virtual ~CallbackRegistration() = default;
};

template <typename Sig>
class CallbackRegistry;

// A helper class that can register, unregister callbacks, and notify registered
// callbacks. This class is thread safe: all methods can be called on any
// thread. The CallbackRegistry must outlive all CallbackRegistrations returned
// by Register().
// TODO(xhwang): This class is similar to base::RepeatingCallbackList, but is
// simpler, and provides thread safety. Consider merging these two.
template <typename... Args>
class CallbackRegistry<void(Args...)> {
 public:
  using CallbackType = base::RepeatingCallback<void(Args...)>;

  CallbackRegistry() = default;

  CallbackRegistry(const CallbackRegistry&) = delete;
  CallbackRegistry& operator=(const CallbackRegistry&) = delete;

  ~CallbackRegistry() = default;

  [[nodiscard]] std::unique_ptr<CallbackRegistration> Register(
      CallbackType cb) {
    base::AutoLock lock(lock_);
    DCHECK(cb);
    uint32_t registration_id = ++next_registration_id_;
    DVLOG(1) << __func__ << ": registration_id = " << registration_id;

    // Use base::BindPostTaskToCurrentDefault so that the callbacks are always
    // posted to the thread where Register() is called. Also, this helps avoid
    // reentrancy and deadlock issues, e.g. Register() is called in one of the
    // callbacks.
    callbacks_[registration_id] =
        base::BindPostTaskToCurrentDefault(std::move(cb));

    return std::make_unique<RegistrationImpl>(this, registration_id);
  }

  void Notify(Args&&... args) {
    DVLOG(1) << __func__;
    base::AutoLock lock(lock_);
    for (auto const& [key_id, callback] : callbacks_)
      callback.Run(std::forward<Args>(args)...);
  }

 private:
  class RegistrationImpl : public CallbackRegistration {
   public:
    RegistrationImpl(CallbackRegistry<void(Args...)>* registry,
                     uint32_t registration_id)
        : registry_(registry), registration_id_(registration_id) {}

    RegistrationImpl(const RegistrationImpl&) = delete;
    RegistrationImpl& operator=(const RegistrationImpl&) = delete;

    ~RegistrationImpl() override { registry_->Unregister(registration_id_); }

   private:
    raw_ptr<CallbackRegistry<void(Args...)>> registry_ = nullptr;
    uint32_t registration_id_ = 0;
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
};

using ClosureRegistry = CallbackRegistry<void()>;

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_REGISTRY_H_
