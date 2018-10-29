// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/address_sanitizer.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

void RecordParkingAction(ParkableStringImpl::ParkingAction action) {
  UMA_HISTOGRAM_ENUMERATION("Memory.MovableStringParkingAction", action);
}

void AsanPoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  // Since |string| is not deallocated, it remains in the per-thread
  // AtomicStringTable, where its content can be accessed for equality
  // comparison for instance, triggering a poisoned memory access.
  // See crbug.com/883344 for an example.
  if (string.Impl()->IsAtomic())
    return;

  const void* start = string.Is8Bit()
                          ? static_cast<const void*>(string.Characters8())
                          : static_cast<const void*>(string.Characters16());
  ASAN_POISON_MEMORY_REGION(start, string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

void AsanUnpoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  const void* start = string.Is8Bit()
                          ? static_cast<const void*>(string.Characters8())
                          : static_cast<const void*>(string.Characters16());
  ASAN_UNPOISON_MEMORY_REGION(start, string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

}  // namespace

// Valid transitions are:
// 1. kUnparked -> kParkingInProgress: Parking started asynchronously
// 2. kParkingInProgress -> kUnparked: Parking did not complete
// 3. kParkingInProgress -> kParked: Parking completed normally
// 4. kParked -> kUnparked: String has been unparked.
//
// See |Park()| for (1), |OnParkingCompleteOnMainThread()| for 2-3, and
// |Unpark()| for (4).
enum class ParkableStringImpl::State { kUnparked, kParkingInProgress, kParked };

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       ParkableState parkable)
    : mutex_(),
      lock_depth_(0),
      state_(State::kUnparked),
      string_(std::move(impl)),
#if DCHECK_IS_ON()
      parked_string_(),
#endif
      may_be_parked_(parkable == ParkableState::kParkable),
      is_8bit_(string_.Is8Bit()),
      length_(string_.length())
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  AssertOnValidThread();
#if DCHECK_IS_ON()
  {
    MutexLocker locker(mutex_);
    DCHECK_EQ(0, lock_depth_);
  }
#endif
  AsanUnpoisonString(string_);
  DCHECK(state_ == State::kParked || state_ == State::kUnparked);

  if (may_be_parked_)
    ParkableStringManager::Instance().Remove(this, string_.Impl());
}

void ParkableStringImpl::Lock() {
  MutexLocker locker(mutex_);
  lock_depth_ += 1;
}

void ParkableStringImpl::Unlock() {
  MutexLocker locker(mutex_);
  DCHECK_GT(lock_depth_, 0);
  lock_depth_ -= 1;

#if defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
  // There are no external references to the data, nobody should touch the data.
  //
  // Note: Only poison the memory if this is on the owning thread, as this is
  // otherwise racy. Indeed |Unlock()| may be called on any thread, and
  // the owning thread may concurrently call |ToString()|. It is then allowed
  // to use the string until the end of the current owning thread task.
  // Requires DCHECK_IS_ON() for the |owning_thread_| check.
  if (CanParkNow() && owning_thread_ == CurrentThread()) {
    AsanPoisonString(string_);
  }
#endif  // defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
}

const String& ParkableStringImpl::ToString() {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
  AsanUnpoisonString(string_);

  Unpark();
  return string_;
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  AssertOnValidThread();
  return length_ * (is_8bit() ? sizeof(LChar) : sizeof(UChar));
}

bool ParkableStringImpl::Park() {
  AssertOnValidThread();
  MutexLocker locker(mutex_);
  DCHECK(may_be_parked_);
  if (state_ == State::kUnparked && CanParkNow()) {
    AsanPoisonString(string_);
    auto task_runner = Platform::Current()->CurrentThread()->GetTaskRunner();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringImpl::OnParkingCompleteOnMainThread,
                       this));
    state_ = State::kParkingInProgress;
  }

  return state_ == State::kParked || state_ == State::kParkingInProgress;
}

bool ParkableStringImpl::is_parked() const {
  return state_ == State::kParked;
}

bool ParkableStringImpl::CanParkNow() const {
  mutex_.AssertAcquired();
  // Can park iff:
  // - the string is eligible to parking
  // - There are no external reference to |string_|. Since |this| holds a
  //   reference to it, then we are the only one.
  // - |this| is not locked.
  return may_be_parked_ && string_.Impl()->HasOneRef() && lock_depth_ == 0;
}

void ParkableStringImpl::Unpark() {
  AssertOnValidThread();
  mutex_.AssertAcquired();
  if (state_ != State::kParked)
    return;

#if DCHECK_IS_ON()
  string_ = parked_string_;
  parked_string_ = String();
#endif
  state_ = State::kUnparked;

  bool backgrounded =
      ParkableStringManager::Instance().IsRendererBackgrounded();
  RecordParkingAction(backgrounded ? ParkingAction::kUnparkedInBackground
                                   : ParkingAction::kUnparkedInForeground);
  auto& manager = ParkableStringManager::Instance();
  manager.OnUnparked(this, string_.Impl());
}

void ParkableStringImpl::OnParkingCompleteOnMainThread() {
  MutexLocker locker(mutex_);
  DCHECK_EQ(State::kParkingInProgress, state_);
  // Between |Park()| and now, things may have happened:
  // 1. |ToString()| or
  // 2. |Lock()| may have been called.
  //
  // We only care about "surviving" calls, that is iff the string returned by
  // |ToString()| is still alive, or whether we are still locked. Since this
  // function is protected by the lock, no concurrent modifications can occur.
  //
  // Finally, since this is a distinct task from any one that can call
  // |ToString()|, the invariant that the pointer stays valid until the next
  // task is preserved.
  if (CanParkNow()) {
    RecordParkingAction(ParkingAction::kParkedInBackground);
    state_ = State::kParked;
    ParkableStringManager::Instance().OnParked(this, string_.Impl());
#if DCHECK_IS_ON()
    AsanUnpoisonString(string_);  // Will touch the string below.
    parked_string_ = string_.IsolatedCopy();
    // Poison the previous allocation.
    // |string_| is kept to make sure the memory is not reused.
    const void* data = is_8bit()
                           ? static_cast<const void*>(string_.Characters8())
                           : static_cast<const void*>(string_.Characters16());
    memset(const_cast<void*>(data), 0xcc, string_.CharactersSizeInBytes());
#endif  // DCHECK_IS_ON()
    AsanPoisonString(string_);
  } else {
    state_ = State::kUnparked;
  }
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl) {
  if (!impl) {
    impl_ = nullptr;
    return;
  }

  bool is_parkable = ParkableStringManager::ShouldPark(*impl);
  if (is_parkable) {
    impl_ = ParkableStringManager::Instance().Add(std::move(impl));
  } else {
    impl_ = base::MakeRefCounted<ParkableStringImpl>(
        std::move(impl), ParkableStringImpl::ParkableState::kNotParkable);
  }
}

ParkableString::~ParkableString() = default;

void ParkableString::Lock() const {
  if (impl_)
    impl_->Lock();
}

void ParkableString::Unlock() const {
  if (impl_)
    impl_->Unlock();
}

bool ParkableString::Is8Bit() const {
  return impl_->is_8bit();
}

String ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : String();
}

wtf_size_t ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
