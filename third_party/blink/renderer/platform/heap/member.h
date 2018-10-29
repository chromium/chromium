// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace WTF {
template <typename P, typename Traits, typename Allocator>
class ConstructTraits;
}  // namespace WTF

namespace blink {

template <typename T>
class Persistent;
template <typename T>
class TraceWrapperMember;

enum class TracenessMemberConfiguration {
  kTraced,
  kUntraced,
};

template <typename T,
          TracenessMemberConfiguration tracenessConfiguration =
              TracenessMemberConfiguration::kTraced>
class MemberBase {
  DISALLOW_NEW();

 public:
  MemberBase() : raw_(nullptr) { SaveCreationThreadState(); }

  MemberBase(std::nullptr_t) : raw_(nullptr) { SaveCreationThreadState(); }

  MemberBase(T* raw) : raw_(raw) {
    SaveCreationThreadState();
    CheckPointer();
  }

  explicit MemberBase(T& raw) : raw_(&raw) {
    SaveCreationThreadState();
    CheckPointer();
  }

  MemberBase(WTF::HashTableDeletedValueType) : raw_(reinterpret_cast<T*>(-1)) {
    SaveCreationThreadState();
  }

  bool IsHashTableDeletedValue() const {
    return raw_ == reinterpret_cast<T*>(-1);
  }

  MemberBase(const MemberBase& other) : raw_(other) {
    SaveCreationThreadState();
    CheckPointer();
  }

  template <typename U>
  MemberBase(const Persistent<U>& other) {
    SaveCreationThreadState();
    raw_ = other;
    CheckPointer();
  }

  template <typename U>
  MemberBase(const MemberBase<U>& other) : raw_(other) {
    SaveCreationThreadState();
    CheckPointer();
  }

  T* Release() {
    T* result = raw_;
    raw_ = nullptr;
    return result;
  }

  explicit operator bool() const { return raw_; }

  operator T*() const { return raw_; }

  T* operator->() const { return raw_; }
  T& operator*() const { return *raw_; }

  template <typename U>
  MemberBase& operator=(const Persistent<U>& other) {
    raw_ = other;
    CheckPointer();
    return *this;
  }

  template <typename U>
  MemberBase& operator=(const MemberBase<U>& other) {
    raw_ = other;
    CheckPointer();
    return *this;
  }

  template <typename U>
  MemberBase& operator=(U* other) {
    raw_ = other;
    CheckPointer();
    return *this;
  }

  MemberBase& operator=(WTF::HashTableDeletedValueType) {
    raw_ = reinterpret_cast<T*>(-1);
    return *this;
  }

  MemberBase& operator=(std::nullptr_t) {
    raw_ = nullptr;
    return *this;
  }

  void Swap(MemberBase<T>& other) {
    std::swap(raw_, other.raw_);
    CheckPointer();
  }

  T* Get() const { return raw_; }

  void Clear() { raw_ = nullptr; }

 protected:
  void CheckPointer() {
#if DCHECK_IS_ON()
    if (!raw_)
      return;
    // HashTable can store a special value (which is not aligned to the
    // allocation granularity) to Member<> to represent a deleted entry.
    // Thus we treat a pointer that is not aligned to the granularity
    // as a valid pointer.
    if (reinterpret_cast<intptr_t>(raw_) % kAllocationGranularity)
      return;

    if (tracenessConfiguration != TracenessMemberConfiguration::kUntraced) {
      ThreadState* current = ThreadState::Current();
      DCHECK(current);
      // m_creationThreadState may be null when this is used in a heap
      // collection which initialized the Member with memset and the
      // constructor wasn't called.
      if (creation_thread_state_) {
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK(creation_thread_state_->IsOnThreadHeap(raw_));
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK_EQ(&current->Heap(), &creation_thread_state_->Heap());
      } else {
        DCHECK(current->IsOnThreadHeap(raw_));
      }
    }

#if defined(ADDRESS_SANITIZER)
    // TODO(haraken): What we really want to check here is that the pointer
    // is a traceable object. In other words, the pointer is either of:
    //
    //   (a) a pointer to the head of an on-heap object.
    //   (b) a pointer to the head of an on-heap mixin object.
    //
    // We can check it by calling ThreadHeap::isHeapObjectAlive(raw_),
    // but we cannot call it here because it requires to include T.h.
    // So we currently only try to implement the check for (a), but do
    // not insist that T's definition is in scope.
    if (IsFullyDefined<T>::value && !IsGarbageCollectedMixin<T>::value)
      HeapObjectHeader::CheckFromPayload(raw_);
#endif
#endif
  }

  void SaveCreationThreadState() {
#if DCHECK_IS_ON()
    if (tracenessConfiguration == TracenessMemberConfiguration::kUntraced) {
      creation_thread_state_ = nullptr;
    } else {
      creation_thread_state_ = ThreadState::Current();
      // Members should be created in an attached thread. But an empty
      // value Member may be created on an unattached thread by a heap
      // collection iterator.
      DCHECK(creation_thread_state_ || !raw_);
    }
#endif
  }

  T* raw_;
#if DCHECK_IS_ON()
  const ThreadState* creation_thread_state_;
#endif
};

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template <typename T>
class Member : public MemberBase<T, TracenessMemberConfiguration::kTraced> {
  DISALLOW_NEW();
  typedef MemberBase<T, TracenessMemberConfiguration::kTraced> Parent;

 public:
  Member() : Parent() {}
  Member(std::nullptr_t) : Parent(nullptr) {}
  Member(T* raw) : Parent(raw) {
    // No write barrier for initializing stores.
  }
  Member(T& raw) : Parent(raw) {
    // No write barrier for initializing stores.
  }
  Member(WTF::HashTableDeletedValueType x) : Parent(x) {}

  Member(const Member& other) : Parent(other) { WriteBarrier(); }
  template <typename U>
  Member(const Member<U>& other) : Parent(other) {
    WriteBarrier();
  }
  template <typename U>
  Member(const Persistent<U>& other) : Parent(other) {
    WriteBarrier();
  }

  template <typename U>
  Member& operator=(const Persistent<U>& other) {
    Parent::operator=(other);
    WriteBarrier();
    return *this;
  }

  Member& operator=(const Member& other) {
    Parent::operator=(other);
    WriteBarrier();
    return *this;
  }

  template <typename U>
  Member& operator=(const Member<U>& other) {
    Parent::operator=(other);
    WriteBarrier();
    return *this;
  }

  template <typename U>
  Member& operator=(const WeakMember<U>& other) {
    Parent::operator=(other);
    WriteBarrier();
    return *this;
  }

  template <typename U>
  Member& operator=(U* other) {
    Parent::operator=(other);
    WriteBarrier();
    return *this;
  }

  Member& operator=(WTF::HashTableDeletedValueType x) {
    Parent::operator=(x);
    return *this;
  }

  Member& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

 protected:
  ALWAYS_INLINE void WriteBarrier() const {
#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
    MarkingVisitor::WriteBarrier(
        const_cast<typename std::remove_const<T>::type*>(this->raw_));
#endif  // BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
  }

  template <typename P, typename Traits, typename Allocator>
  friend class WTF::ConstructTraits;
};

// A checked version of Member<>, verifying that only same-thread references
// are kept in the smart pointer. Intended to be used to diagnose unclean
// thread reference usage in release builds. It simply exposes the debug-only
// MemberBase<> checking we already have in place for select usage to diagnose
// per-thread issues. Only intended used temporarily while diagnosing suspected
// problems with cross-thread references.
template <typename T>
class SameThreadCheckedMember : public Member<T> {
  DISALLOW_NEW();
  typedef Member<T> Parent;

 public:
  SameThreadCheckedMember() : Parent() { SaveCreationThreadState(); }
  SameThreadCheckedMember(std::nullptr_t) : Parent(nullptr) {
    SaveCreationThreadState();
  }

  SameThreadCheckedMember(T* raw) : Parent(raw) {
    SaveCreationThreadState();
    CheckPointer();
  }

  SameThreadCheckedMember(T& raw) : Parent(raw) {
    SaveCreationThreadState();
    CheckPointer();
  }

  SameThreadCheckedMember(WTF::HashTableDeletedValueType x) : Parent(x) {
    SaveCreationThreadState();
    CheckPointer();
  }

  SameThreadCheckedMember(const SameThreadCheckedMember& other)
      : Parent(other) {
    SaveCreationThreadState();
  }
  template <typename U>
  SameThreadCheckedMember(const SameThreadCheckedMember<U>& other)
      : Parent(other) {
    SaveCreationThreadState();
    CheckPointer();
  }

  template <typename U>
  SameThreadCheckedMember(const Persistent<U>& other) : Parent(other) {
    SaveCreationThreadState();
    CheckPointer();
  }

  template <typename U>
  SameThreadCheckedMember& operator=(const Persistent<U>& other) {
    Parent::operator=(other);
    CheckPointer();
    return *this;
  }

  template <typename U>
  SameThreadCheckedMember& operator=(const SameThreadCheckedMember<U>& other) {
    Parent::operator=(other);
    CheckPointer();
    return *this;
  }

  template <typename U>
  SameThreadCheckedMember& operator=(const WeakMember<U>& other) {
    Parent::operator=(other);
    CheckPointer();
    return *this;
  }

  template <typename U>
  SameThreadCheckedMember& operator=(U* other) {
    Parent::operator=(other);
    CheckPointer();
    return *this;
  }

  SameThreadCheckedMember& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

 private:
  void CheckPointer() {
    if (!this->raw_)
      return;
    // HashTable can store a special value (which is not aligned to the
    // allocation granularity) to Member<> to represent a deleted entry.
    // Thus we treat a pointer that is not aligned to the granularity
    // as a valid pointer.
    if (reinterpret_cast<intptr_t>(this->raw_) % kAllocationGranularity)
      return;

    ThreadState* current = ThreadState::Current();
    DCHECK(current);
    // m_creationThreadState may be null when this is used in a heap
    // collection which initialized the Member with memset and the
    // constructor wasn't called.
    if (creation_thread_state_) {
      // Member should point to objects that belong in the same ThreadHeap.
      CHECK(creation_thread_state_->IsOnThreadHeap(this->raw_));
      // Member should point to objects that belong in the same ThreadHeap.
      CHECK_EQ(&current->Heap(), &creation_thread_state_->Heap());
    } else {
      CHECK(current->IsOnThreadHeap(this->raw_));
    }
  }

  void SaveCreationThreadState() {
    creation_thread_state_ = ThreadState::Current();
    // All Members should be created in an attached thread, but an empty
    // value Member may be created on an unattached thread by a heap
    // collection iterator.
    CHECK(this->creation_thread_state_ || !this->raw_);
  }

  const ThreadState* creation_thread_state_;
};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember
// creates a weak pointer, which does not keep the pointee alive. Hence if all
// pointers to to a heap allocated object are weak the object will be garbage
// collected. At the time of GC the weak pointers will automatically be set to
// null.
template <typename T>
class WeakMember : public MemberBase<T, TracenessMemberConfiguration::kTraced> {
  typedef MemberBase<T, TracenessMemberConfiguration::kTraced> Parent;

 public:
  WeakMember() : Parent() {}

  WeakMember(std::nullptr_t) : Parent(nullptr) {}

  WeakMember(T* raw) : Parent(raw) {}

  WeakMember(WTF::HashTableDeletedValueType x) : Parent(x) {}

  template <typename U>
  WeakMember(const Persistent<U>& other) : Parent(other) {}

  template <typename U>
  WeakMember(const Member<U>& other) : Parent(other) {}

  template <typename U>
  WeakMember& operator=(const Persistent<U>& other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  template <typename U>
  WeakMember& operator=(const Member<U>& other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  template <typename U>
  WeakMember& operator=(U* other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  WeakMember& operator=(std::nullptr_t) {
    this->raw_ = nullptr;
    return *this;
  }

 private:
  T** Cell() const { return const_cast<T**>(&this->raw_); }

  friend class Visitor;
};

// UntracedMember is a pointer to an on-heap object that is not traced for some
// reason. Please don't use this unless you understand what you're doing.
// Basically, all pointers to on-heap objects must be stored in either of
// Persistent, Member or WeakMember. It is not allowed to leave raw pointers to
// on-heap objects. However, there can be scenarios where you have to use raw
// pointers for some reason, and in that case you can use UntracedMember. Of
// course, it must be guaranteed that the pointing on-heap object is kept alive
// while the raw pointer is pointing to the object.
template <typename T>
class UntracedMember final
    : public MemberBase<T, TracenessMemberConfiguration::kUntraced> {
  typedef MemberBase<T, TracenessMemberConfiguration::kUntraced> Parent;

 public:
  UntracedMember() : Parent() {}

  UntracedMember(std::nullptr_t) : Parent(nullptr) {}

  UntracedMember(T* raw) : Parent(raw) {}

  template <typename U>
  UntracedMember(const Persistent<U>& other) : Parent(other) {}

  template <typename U>
  UntracedMember(const Member<U>& other) : Parent(other) {}

  UntracedMember(WTF::HashTableDeletedValueType x) : Parent(x) {}

  template <typename U>
  UntracedMember& operator=(const Persistent<U>& other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  template <typename U>
  UntracedMember& operator=(const Member<U>& other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  template <typename U>
  UntracedMember& operator=(U* other) {
    this->raw_ = other;
    this->CheckPointer();
    return *this;
  }

  UntracedMember& operator=(std::nullptr_t) {
    this->raw_ = nullptr;
    return *this;
  }
};

}  // namespace blink

namespace WTF {

// PtrHash is the default hash for hash tables with Member<>-derived elements.
template <typename T>
struct MemberHash : PtrHash<T> {
  STATIC_ONLY(MemberHash);
  template <typename U>
  static unsigned GetHash(const U& key) {
    return PtrHash<T>::GetHash(key);
  }
  template <typename U, typename V>
  static bool Equal(const U& a, const V& b) {
    return a == b;
  }
};

template <typename T>
struct DefaultHash<blink::Member<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::UntracedMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::SameThreadCheckedMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct IsTraceable<blink::Member<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsWeak<blink::WeakMember<T>> {
  STATIC_ONLY(IsWeak);
  static const bool value = true;
};

template <typename T>
struct IsTraceable<blink::WeakMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsTraceable<blink::SameThreadCheckedMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsTraceable<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T, typename Traits, typename Allocator>
class ConstructTraits<blink::Member<T>, Traits, Allocator> {
  STATIC_ONLY(ConstructTraits);

 public:
  template <typename... Args>
  static blink::Member<T>* ConstructAndNotifyElement(void* location,
                                                     Args&&... args) {
    blink::Member<T>* object =
        new (NotNull, location) blink::Member<T>(std::forward<Args>(args)...);
    object->WriteBarrier();
    return object;
  }

  static void NotifyNewElements(blink::Member<T>* array, size_t len) {
    while (len-- > 0) {
      array->WriteBarrier();
      array++;
    }
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
