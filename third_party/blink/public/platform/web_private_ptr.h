/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRIVATE_PTR_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRIVATE_PTR_H_

#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/blink/public/platform/web_common.h"
#include "v8/include/cppgc/persistent.h"

#include <cstring>

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"  // nogncheck
#include "third_party/blink/renderer/platform/heap/persistent.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/type_traits.h"  // nogncheck
#endif

namespace WTF {
template <class T, typename Traits>
class ThreadSafeRefCounted;
}

namespace blink {

// By default, the destruction of a WebPrivatePtr<> must happen on the same
// thread that created it, but can optionally be allowed to happen on
// another thread.
enum class WebPrivatePtrDestruction {
  kSameThread,
  kCrossThread,
};

// The WebPrivatePtr<> holds by default a strong reference to its Blink object,
// but Blink GC managed objects also support keeping a weak reference by
// way of WebPrivatePtr<>.
enum class WebPrivatePtrStrength {
  kNormal,
  kWeak,
};

namespace internal {
struct DummyGarbageCollected : cppgc::GarbageCollected<DummyGarbageCollected> {
};
}  // namespace internal

// WebPrivatePtr must be able to store all possible pointer types (single
// pointer for RefCounted types and two pointers for (CrossThread)Persistent).
// sizeof(Persistent) is the maximal possible size for all possible pointer
// types (we make sure that this holds with static_asserts below).
constexpr size_t kMaxWebPrivatePtrSize =
    sizeof(cppgc::Persistent<internal::DummyGarbageCollected>);

#if INSIDE_BLINK

enum class LifetimeManagementType {
  kRefCounted,
  kGarbageCollected,
};

template <typename T,
          WebPrivatePtrDestruction,
          WebPrivatePtrStrength,
          LifetimeManagementType =
              WTF::IsGarbageCollectedType<T>::value
                  ? LifetimeManagementType::kGarbageCollected
                  : LifetimeManagementType::kRefCounted>
class PtrStorageImpl;

template <typename T,
          WebPrivatePtrDestruction PtrDestruction,
          WebPrivatePtrStrength PtrStrength>
class PtrStorageImpl<T,
                     PtrDestruction,
                     PtrStrength,
                     LifetimeManagementType::kRefCounted> {
 public:
  using BlinkPtrType = scoped_refptr<T>;

  template <typename U>
  void Construct(U&& value, const PersistentLocation& loc) {
    ptr_ = nullptr;
    Assign(std::forward<U>(value));
  }

  void Assign(BlinkPtrType&& val) {
    static_assert(
        PtrDestruction == WebPrivatePtrDestruction::kSameThread ||
            WTF::IsSubclassOfTemplate<T, WTF::ThreadSafeRefCounted>::value,
        "Cross thread destructible class must derive from "
        "ThreadSafeRefCounted<>");
    static_assert(
        PtrStrength == WebPrivatePtrStrength::kNormal,
        "Ref-counted classes do not support weak WebPrivatePtr<> references");
    Release();
    if (val)
      val->AddRef();
    ptr_ = val.get();
    val = nullptr;
  }

  void Assign(const PtrStorageImpl& other) {
    T* val = other.Get();
    if (ptr_ == val)
      return;
    Release();
    if (val)
      val->AddRef();
    ptr_ = val;
  }

  T* Get() const { return ptr_; }

  void Release() {
    if (ptr_)
      ptr_->Release();
    ptr_ = nullptr;
  }

 private:
  union {
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION T* ptr_;
    [[maybe_unused]] std::aligned_storage_t<kMaxWebPrivatePtrSize, alignof(T*)>
        unused_;
  };
};

// Translates WebPrivatePtrDestruction and WebPrivatePtrStrength to the
// appropriate blink Persistent type.
template <typename T, WebPrivatePtrDestruction, WebPrivatePtrStrength>
struct WebPrivatePtrPersistentStorageType;

#define TRAIT_TO_TYPE_LIST(V)                        \
  V(kSameThread, kNormal, Persistent<T>)             \
  V(kSameThread, kWeak, WeakPersistent<T>)           \
  V(kCrossThread, kNormal, CrossThreadPersistent<T>) \
  V(kCrossThread, kWeak, CrossThreadWeakPersistent<T>)

#define DEFINE_TYPE_SPECIALIZATION(PtrDestruction, PtrStrengh, ReferenceType) \
  template <typename T>                                                       \
  struct WebPrivatePtrPersistentStorageType<                                  \
      T, WebPrivatePtrDestruction::PtrDestruction,                            \
      WebPrivatePtrStrength::PtrStrengh> {                                    \
    using Type = ReferenceType;                                               \
  };

TRAIT_TO_TYPE_LIST(DEFINE_TYPE_SPECIALIZATION)
#undef DEFINE_TYPE_TRAIT
#undef TRAIT_TO_TYPE_LIST

template <typename T,
          WebPrivatePtrDestruction PtrDestruction,
          WebPrivatePtrStrength PtrStrength>
class PtrStorageImpl<T,
                     PtrDestruction,
                     PtrStrength,
                     LifetimeManagementType::kGarbageCollected> {
 public:
  using BlinkPtrType =
      typename WebPrivatePtrPersistentStorageType<T,
                                                  PtrDestruction,
                                                  PtrStrength>::Type;

  template <typename U>
  void Construct(U&& value, const PersistentLocation& loc) {
    new (&handle_) BlinkPtrType(std::forward<U>(value), loc);
  }

  void Assign(T* val) { handle_ = val; }

  void Assign(const PtrStorageImpl& other) { handle_ = other.handle_; }

  T* Get() const { return handle_.Get(); }

  void Release() { handle_.Clear(); }

 private:
  union {
    BlinkPtrType handle_;
    [[maybe_unused]] std::aligned_storage_t<kMaxWebPrivatePtrSize, alignof(T*)>
        unused_;
  };
};

template <typename T,
          WebPrivatePtrDestruction PtrDestruction,
          WebPrivatePtrStrength PrStrength>
class PtrStorage final : public PtrStorageImpl<T, PtrDestruction, PrStrength> {
 public:
  static PtrStorage& FromSlot(void** slot) {
    return *reinterpret_cast<PtrStorage*>(slot);
  }

  static const PtrStorage& FromSlot(void* const* slot) {
    return *reinterpret_cast<const PtrStorage*>(slot);
  }

  // Prevent construction via normal means.
  PtrStorage() = delete;
  PtrStorage(const PtrStorage&) = delete;
};
#endif

//
// WebPrivatePtr is an opaque reference to a Blink-internal class that can keep
// an object alive over the Blink API boundary. The object referred to can be
// reference counted or garbage collected. The pointer must be set and cleared
// from within Blink. Outside of Blink, WebPrivatePtr only allows constructing
// an empty reference and checking for an empty reference.
//
// A typical implementation of a class which uses WebPrivatePtr might look like
// this:
//
//   // WebFoo.h
//   class WebFoo {
//    public:
//     BLINK_EXPORT ~WebFoo();
//     // Implemented in the .cc file as it requires full definition of Foo.
//     WebFoo();
//     // Implemented in the .cc file as it requires full definition of Foo.
//     WebFoo(const WebFoo& other);
//     WebFoo& operator=(const WebFoo& other) {
//       Assign(other);
//       return *this;
//     }
//     // Implemented in the .cc file as it requires full definition of Foo.
//     BLINK_EXPORT void Assign(const WebFoo&);
//
//     // Methods that are exposed to Chromium and which are specific to
//     // WebFoo go here.
//     BLINK_EXPORT DoWebFooThing();
//
//     // Methods that are used only by other Blink classes should only be
//     // declared when INSIDE_BLINK is set.
//     #if INSIDE_BLINK
//     WebFoo(scoped_refptr<Foo>);
//     #endif
//
//     private:
//      WebPrivatePtr<Foo> private_;
//    };
//
//    // WebFoo.cpp
//    WebFoo::~WebFoo() { private_.Reset(); }
//    WebFoo::WebFoo() = default;
//    WebFoo::WebFoo(const WebFoo& other) { Assign(other); }
//    void WebFoo::Assign(const WebFoo& other) { ... }
//
template <typename T,
          WebPrivatePtrDestruction PtrDestruction =
              WebPrivatePtrDestruction::kSameThread,
          WebPrivatePtrStrength PtrStrength = WebPrivatePtrStrength::kNormal>
class WebPrivatePtr final {
 public:
  ~WebPrivatePtr() {
    // We don't destruct the object pointed by storage_ here because we don't
    // want to expose destructors of core classes to embedders which is the case
    // for reference counted objects. The embedder should call Reset() manually
    // in destructors of classes with WebPrivatePtr members.
    DCHECK(IsNull());
  }

  // Disable the copy constructor; classes that contain a WebPrivatePtr
  // should implement their copy constructor using assign().
  WebPrivatePtr(const WebPrivatePtr&) = delete;

  bool IsNull() const { return !*reinterpret_cast<void* const*>(&storage_); }

  explicit operator bool() const { return !IsNull(); }

#if INSIDE_BLINK
  explicit WebPrivatePtr(
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
    Storage().Construct(nullptr, loc);
  }

  void Reset() { Storage().Release(); }

  T* Get() const { return Storage().Get(); }

  T& operator*() const {
    DCHECK(!IsNull());
    return *Get();
  }

  T* operator->() const {
    DCHECK(!IsNull());
    return Get();
  }

  template <typename U>
  // NOLINTNEXTLINE(google-explicit-constructor)
  WebPrivatePtr(U&& ptr,
                const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
    Storage().Construct(std::forward<U>(ptr), loc);
  }

  template <typename U>
  WebPrivatePtr& operator=(U&& ptr) {
    Storage().Assign(std::forward<U>(ptr));
    return *this;
  }

  WebPrivatePtr& operator=(const WebPrivatePtr& other) {
    Storage().Assign(other.Storage());
    return *this;
  }

#else   // !INSIDE_BLINK
  WebPrivatePtr() { memset(&storage_, 0, sizeof(storage_)); }

  // Disable the assignment operator; we define it above for when
  // INSIDE_BLINK is set, but we need to make sure that it is not
  // used outside there; the compiler-provided version won't handle reference
  // counting properly.
  WebPrivatePtr& operator=(const WebPrivatePtr& other) = delete;
#endif  // !INSIDE_BLINK

 private:
#if INSIDE_BLINK
  using PtrStorageType = PtrStorage<T, PtrDestruction, PtrStrength>;

  PtrStorageType& Storage() {
    static_assert(sizeof(WebPrivatePtr) == sizeof(PtrStorageType),
                  "Sizes must match");
    return PtrStorageType::FromSlot(reinterpret_cast<void**>(&storage_));
  }
  const PtrStorageType& Storage() const {
    return const_cast<WebPrivatePtr*>(this)->Storage();
  }
#endif  // INSIDE_BLINK

  std::aligned_storage_t<kMaxWebPrivatePtrSize, alignof(uintptr_t)> storage_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRIVATE_PTR_H_
