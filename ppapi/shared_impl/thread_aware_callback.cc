// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/thread_aware_callback.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"

namespace ppapi {
namespace internal {

class ThreadAwareCallbackBase::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core() : aborted_(false) {}

  void MarkAsAborted() { aborted_ = true; }

  void RunIfNotAborted(base::OnceClosure closure) {
    if (!aborted_)
      CallWhileUnlocked(std::move(closure));
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() {}

  bool aborted_;
};

ThreadAwareCallbackBase::ThreadAwareCallbackBase()
    : target_loop_(PpapiGlobals::Get()->GetCurrentMessageLoop()),
      core_(new Core()) {
  DCHECK(target_loop_.get());
}

ThreadAwareCallbackBase::~ThreadAwareCallbackBase() { core_->MarkAsAborted(); }

// static
bool ThreadAwareCallbackBase::HasTargetLoop() {
  return !!PpapiGlobals::Get()->GetCurrentMessageLoop();
}

void ThreadAwareCallbackBase::InternalRunOnTargetThread(
    base::OnceClosure closure) {
  if (target_loop_.get() != PpapiGlobals::Get()->GetCurrentMessageLoop()) {
    target_loop_->PostClosure(
        FROM_HERE,
        RunWhileLocked(
            base::BindOnce(&Core::RunIfNotAborted, core_, std::move(closure))),
        0);
  } else {
    CallWhileUnlocked(std::move(closure));
  }
}

}  // namespace internal
}  // namespace ppapi
