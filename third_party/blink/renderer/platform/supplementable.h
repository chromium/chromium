/*
 * Copyright (C) 2012 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SUPPLEMENTABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SUPPLEMENTABLE_H_

#include <cstddef>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/threading.h"
#endif

namespace blink {

// What you should know about Supplementable and Supplement
// ========================================================
// Supplementable allows a garbage-collected object to be extended with
// additional data.
//
// This is almost the same as adding a member to that class, except that
// it can also be used in cases that would otherwise be layering violation.
// For example, it is common for features implemented in modules/ to
// supplement classes in core/. In other cases, you probably should not
// use it, as it is less clear, allows for type confusion between subclasses,
// reduces inlining opportunities and requires your object to have a vtable.
// (It used to have different memory characteristics, which is why there's
// more use around than one would expect.)
//
// Supplementable and Supplement instances are meant to be thread local. They
// should only be accessed from within the thread that created them. The
// 2 classes are not designed for safe access from another thread. Violating
// this design assumption can result in memory corruption and unpredictable
// behavior.
//
// What you should know about the Supplement keys
// ==============================================
// The Supplementable is expected to provide an enum class Supplements, that
// holds keys to distinguish each of the supplements. The Supplement is expected
// to use the same enum as its key (kSupplementIndex; this needs to be static
// const, or ideally static constexpr). The Supplementable's SupplementMap
// will use the index of the enum into the array. The Supplements template also
// needs to have a kNumSupplements parameter to rightsize the array.
//
// Use extreme caution when deriving a supplementable class, as misuse can cause
// type confusion.
//
// Typical use is expected to look like this:
//
//     class Navigator : public Supplementable<Navigator, 2> {
//      public:
//       enum class Supplements {
//         kFoo = 0,
//         kBar = 1
//       };
//
//       ...
//     };
//
//     class NavigatorFoo : public Supplement<Navigator> {
//      public:
//       static constexpr auto kSupplementIndex =
//           Navigator::Supplements::kFoo;
//
//       static NavigatorFoo& From(Navigator&);
//     }
//
//     NavigatorFoo& NavigatorFoo::From(Navigator& navigator)
//     {
//       NavigatorFoo* supplement =
//           Supplement<Navigator>::From<NavigatorFoo>(navigator);
//       if (!supplement) {
//         supplement = new NavigatorFoo(navigator);
//         ProvideTo(navigator, supplement);
//       }
//       return *supplement;
//     }
//
// The map index will automatically be determined from the supplement type
// used. Out-of-bounds indexes will be detected (with a CHECK, most likely
// also complaining at compile time), but too large arrays or unused indexes
// will not.
//
// What you should know about thread checks
// ========================================
// When assertion is enabled this class performs thread-safety check so that
// supplements are provided to and from the same thread.
// If you want to provide some value for Workers, this thread check may be too
// strict, since in you'll be providing the value while worker preparation is
// being done on the main thread, even before the worker thread has started.
// If that's the case you can explicitly call reattachThread() when the
// Supplementable object is passed to the final destination thread (i.e.
// worker thread). This will allow supplements to be accessed on that thread.
// Please be extremely careful to use the method though, as randomly calling
// the method could easily cause racy condition.
//
// Note that reattachThread() does nothing if assertion is not enabled.

template <typename T, unsigned kNumSupplements>
class Supplementable;

template <typename T>
class Supplement : public GarbageCollectedMixin {
 public:
  // TODO(haraken): Remove the default constructor.
  // All Supplement objects should be instantiated with |supplementable_|.
  explicit Supplement(std::nullptr_t) {}

  explicit Supplement(T& supplementable) : supplementable_(&supplementable) {}

  // Supplements are constructed lazily on first access and are destroyed with
  // their Supplementable, so GetSupplementable() should never return null (if
  // the default constructor is completely removed).
  T* GetSupplementable() const { return supplementable_.Get(); }

  template <typename SupplementType, unsigned kNumSupplements>
  static void ProvideTo(Supplementable<T, kNumSupplements>& supplementable,
                        SupplementType* supplement) {
    supplementable.ProvideSupplement(supplement);
  }

  template <typename SupplementType, unsigned kNumSupplements>
  static SupplementType* From(
      const Supplementable<T, kNumSupplements>& supplementable) {
    return supplementable.template RequireSupplement<SupplementType>();
  }

  template <typename SupplementType, unsigned kNumSupplements>
  static SupplementType* From(
      const Supplementable<T, kNumSupplements>* supplementable) {
    return supplementable
               ? supplementable->template RequireSupplement<SupplementType>()
               : nullptr;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(supplementable_);
  }

 private:
  Member<T> supplementable_;
};

template <typename T, unsigned kNumSupplements>
class Supplementable : public GarbageCollectedMixin {
 public:
  Supplementable(const Supplementable&) = delete;
  Supplementable& operator=(const Supplementable&) = delete;

  template <typename SupplementType>
  void ProvideSupplement(SupplementType* supplement) {
#if DCHECK_IS_ON()
    DCHECK_EQ(creation_thread_id_, CurrentThread());
#endif
    this->supplements_[GetIndex<SupplementType>()] = supplement;
  }

  template <typename SupplementType>
  void RemoveSupplement() {
#if DCHECK_IS_ON()
    DCHECK_EQ(creation_thread_id_, CurrentThread());
#endif
    this->supplements_[GetIndex<SupplementType>()] = nullptr;
  }

  template <typename SupplementType>
  SupplementType* RequireSupplement() const {
#if DCHECK_IS_ON()
    DCHECK_EQ(attached_thread_id_, CurrentThread());
#endif
    return static_cast<SupplementType*>(
        this->supplements_[GetIndex<SupplementType>()].Get());
  }

  void ReattachThread() {
#if DCHECK_IS_ON()
    attached_thread_id_ = CurrentThread();
#endif
  }

  void Trace(Visitor* visitor) const override {
    visitor->TraceMultiple(supplements_.data(), supplements_.size());
  }

 protected:
  using SupplementMap = std::array<Member<Supplement<T>>, kNumSupplements>;
  SupplementMap supplements_;

  Supplementable()
#if DCHECK_IS_ON()
      : attached_thread_id_(CurrentThread()),
        creation_thread_id_(CurrentThread())
#endif
  {
  }

 private:
  template <typename SupplementType>
  unsigned GetIndex() const {
    static_assert(
        std::is_integral_v<decltype(SupplementType::kSupplementIndex)> ||
            std::is_enum_v<decltype(SupplementType::kSupplementIndex)>,
        "Declare a kSupplementIndex. See Supplementable.h for details.");
    if constexpr (__builtin_constant_p(SupplementType::kSupplementIndex)) {
      // Check compile-time if we can; if someone uses an out-of-bounds index,
      // it is better to know when compiling than when running the test.
      static_assert(static_cast<unsigned>(SupplementType::kSupplementIndex) <
                    kNumSupplements);
    }
    unsigned idx = static_cast<unsigned>(SupplementType::kSupplementIndex);
    CHECK_LT(idx, kNumSupplements);  // Should almost always be optimized away.
    return idx;
  }

#if DCHECK_IS_ON()
  base::PlatformThreadId attached_thread_id_;
  base::PlatformThreadId creation_thread_id_;
#endif
};

template <typename T>
struct ThreadingTrait<Supplement<T>> {
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T, unsigned kNumSupplements>
struct ThreadingTrait<Supplementable<T, kNumSupplements>> {
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::Affinity;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SUPPLEMENTABLE_H_
