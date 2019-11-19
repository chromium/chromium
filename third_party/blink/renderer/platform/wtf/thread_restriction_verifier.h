/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREAD_RESTRICTION_VERIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREAD_RESTRICTION_VERIFIER_H_

#include "third_party/blink/renderer/platform/wtf/assertions.h"

#if DCHECK_IS_ON()

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace WTF {

// Verifies that a class is used in a way that respects its lack of
// thread-safety.  The default mode is to verify that the object will only be
// used on a single thread. The thread gets captured when setShared(true) is
// called.  The mode may be changed by calling useMutexMode (or
// turnOffVerification).
class ThreadRestrictionVerifier {
  DISALLOW_NEW();

 public:
  ThreadRestrictionVerifier() : shared_(false), owning_thread_(0) {}

  // Call onRef() before refCount is incremented in ref(). Returns whether the
  // ref() is safe.
  template <typename COUNTERTYPE>
  bool OnRef(COUNTERTYPE ref_count) {
    // Start thread verification as soon as the ref count gets to 2. This
    // heuristic reflects the fact that items are often created on one
    // thread and then given to another thread to be used.
    // FIXME: Make this restriction tigher. Especially as we move to more
    // common methods for sharing items across threads like
    // CrossThreadCopier.h
    // We should be able to add a "detachFromThread" method to make this
    // explicit.
    if (ref_count == 1)
      SetShared(true);
    return IsSafeToUse();
  }

  // Call onDeref() before refCount is decremented in deref(). Returns whether
  // the deref() is safe.
  template <typename COUNTERTYPE>
  bool OnDeref(COUNTERTYPE ref_count) {
    bool safe = IsSafeToUse();
    // Stop thread verification when the ref goes to 1 because it
    // is safe to be passed to another thread at this point.
    if (ref_count == 2)
      SetShared(false);
    return safe;
  }

  // Is it OK to use the object at this moment on the current thread?
  bool IsSafeToUse() const {
    return !shared_ || owning_thread_ == CurrentThread();
  }

 private:
  // Indicates that the object may (or may not) be owned by more than one place.
  void SetShared(bool shared) {
    bool previously_shared = shared_;
    shared_ = shared;

    if (!shared_)
      return;

    DCHECK_NE(shared, previously_shared);
    // Capture the current thread to verify that subsequent ref/deref happen on
    // this thread.
    owning_thread_ = CurrentThread();
  }

  bool shared_;

  base::PlatformThreadId owning_thread_;
};

}  // namespace WTF

#endif  // DCHECK_IS_ON()
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREAD_RESTRICTION_VERIFIER_H_
