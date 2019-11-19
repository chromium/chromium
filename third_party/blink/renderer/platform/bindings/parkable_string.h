// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

// ParkableString represents a string that may be parked in memory, that it its
// underlying memory address may change. Its content can be retrieved with the
// |ToString()| method.
// As a consequence, the inner pointer should never be cached, and only touched
// through a string returned by the |ToString()| method.
//
// As with WTF::AtomicString, this class is *not* thread-safe, and strings
// created on a thread must always be used on the same thread.

namespace blink {

class WebProcessMemoryDump;
struct CompressionTaskParams;

// A parked string is parked by calling |Park()|, and unparked by calling
// |ToString()| on a parked string.
// |Lock()| does *not* unpark a string, and |ToString()| must be called on
// a single thread, the one on which the string was created. Only |Lock()|
// and |Unlock()| can be called from any thread.
class PLATFORM_EXPORT ParkableStringImpl final
    : public RefCounted<ParkableStringImpl> {
 public:
  enum class ParkableState { kParkable, kNotParkable };
  enum class ParkingMode { kIfCompressedDataExists, kAlways };
  enum class AgeOrParkResult {
    kSuccessOrTransientFailure,
    kNonTransientFailure
  };

  // Not all parkable strings can actually be parked. If |parkable| is
  // kNotParkable, then one cannot call |Park()|, and the underlying StringImpl
  // will not move.
  ParkableStringImpl(scoped_refptr<StringImpl>&& impl, ParkableState parkable);
  ~ParkableStringImpl();

  void Lock();
  void Unlock();

  void PurgeMemory();

  // The returned string may be used as a normal one, as long as the
  // returned value (or a copy of it) is alive.
  const String& ToString();

  // See the matching String methods.
  bool is_8bit() const { return is_8bit_; }
  unsigned length() const { return length_; }
  unsigned CharactersSizeInBytes() const;

  // Tries to either age or park a string:
  //
  // - If the string is already old, tries to park it.
  // - Otherwise, tries to age it.
  //
  // The action doesn't necessarily succeed. either due to a temporary
  // or potentially lasting condition.
  //
  // As parking may be synchronous, this can call back into
  // ParkableStringManager.
  AgeOrParkResult MaybeAgeOrParkString();

  // A parked string cannot be accessed until it has been |Unpark()|-ed.
  //
  // Parking may be synchronous, and will be if compressed data is already
  // available. If |mode| is |kIfCompressedDataExists|, then parking will always
  // be synchronous.
  //
  // Must not be called if |may_be_parked()| returns false.
  //
  // Returns true if the string is being parked or has been parked.
  bool Park(ParkingMode mode);
  // Returns true iff the string can be parked. This does not mean that the
  // string can be parked now, merely that it is eligible to be parked at some
  // point.
  bool may_be_parked() const { return may_be_parked_; }
  // Returns true if the string is parked.
  bool is_parked() const;
  // Returns whether synchronous parking is possible, that is the string was
  // parked in the past.
  bool has_compressed_data() const { return !!compressed_; }
  // Returns the compressed size, must not be called unless the string has a
  // compressed representation.
  size_t compressed_size() const {
    DCHECK(has_compressed_data());
    return compressed_->size();
  }

  bool is_young_for_testing() {
    MutexLocker locker(mutex_);
    return is_young_;
  }

 private:
  enum class State : uint8_t;
  enum class Status : uint8_t;
  friend class ParkableStringManager;

  unsigned GetHash() const { return hash_; }

  // Both functions below can be expensive (i.e. trigger unparking).
  bool Equal(const ParkableStringImpl& rhs) const;
  bool Equal(const scoped_refptr<StringImpl> rhs) const;

#if defined(ADDRESS_SANITIZER)
  // See |CompressInBackground()|. Doesn't make the string young.
  // May be called from any thread.
  void LockWithoutMakingYoung();
#endif  // defined(ADDRESS_SANITIZER)
  // May be called from any thread.
  void MakeYoung() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Whether the string is referenced or locked. The return value is valid as
  // long as |mutex_| is held.
  Status CurrentStatus() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool CanParkNow() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ParkInternal(ParkingMode mode);
  void Unpark();
  String UnparkInternal() const;
  String ToStringTransient() const;
  // Called on the main thread after compression is done.
  // |params| is the same as the one passed to |CompressInBackground()|,
  // |compressed| is the compressed data, nullptr if compression failed.
  // |parking_thread_time| is the CPU time used by the background compression
  // task.
  void OnParkingCompleteOnMainThread(
      std::unique_ptr<CompressionTaskParams> params,
      std::unique_ptr<Vector<uint8_t>> compressed,
      base::TimeDelta parking_thread_time);

  // Background thread.
  static void CompressInBackground(std::unique_ptr<CompressionTaskParams>);

  int lock_depth_for_testing() {
    MutexLocker locker_(mutex_);
    return lock_depth_;
  }

  Mutex mutex_;
  int lock_depth_ GUARDED_BY(mutex_);

  // Main thread only.
  State state_;
  String string_;
  std::unique_ptr<Vector<uint8_t>> compressed_;

  // A string can either be "young" or "old". It starts young, and transitions
  // are:
  // Young -> Old: By calling |MaybeAgeOrParkString()|.
  // Old -> Young: When the string is accessed, either by |Lock()|-ing it or
  //               calling |ToString()|.
  //
  // Thread safety: it is typically not safe to guard only one part of a
  // bitfield with a mutex, but this is correct here, as the other members are
  // const (and never change).
  bool is_young_ : 1 GUARDED_BY(mutex_);

  const bool may_be_parked_ : 1;
  const bool is_8bit_ : 1;
  const unsigned length_;
  const wtf_size_t hash_;

#if DCHECK_IS_ON()
  const base::PlatformThreadId owning_thread_;
#endif

  void AssertOnValidThread() const {
#if DCHECK_IS_ON()
    DCHECK_EQ(owning_thread_, CurrentThread());
#endif
  }

  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockUnlock);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, LockParkedString);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, Equality);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, EqualityNoUnparking);
  DISALLOW_COPY_AND_ASSIGN(ParkableStringImpl);
};

class PLATFORM_EXPORT ParkableString final {
  DISALLOW_NEW();

 public:
  ParkableString() : impl_(nullptr) {}
  explicit ParkableString(scoped_refptr<StringImpl>&& impl);
  ParkableString(const ParkableString& rhs) : impl_(rhs.impl_) {}
  ~ParkableString();

  // Locks a string. A string is unlocked when the number of Lock()/Unlock()
  // calls match. A locked string cannot be parked.
  // Can be called from any thread.
  void Lock() const;

  // Unlocks a string.
  // Can be called from any thread.
  void Unlock() const;

  void OnMemoryDump(WebProcessMemoryDump* pmd, const String& name) const;

  // See the matching String methods.
  bool Is8Bit() const;
  bool IsNull() const { return !impl_; }
  unsigned length() const { return impl_ ? impl_->length() : 0; }
  bool may_be_parked() const { return impl_ && impl_->may_be_parked(); }

  ParkableStringImpl* Impl() const { return impl_ ? impl_.get() : nullptr; }
  // Returns an unparked version of the string.
  // The string is guaranteed to be valid for
  // max(lifetime of a copy of the returned reference, current thread task).
  const String& ToString() const;
  wtf_size_t CharactersSizeInBytes() const;

  // Causes the string to be unparked. Note that the pointer must not be
  // cached.
  const LChar* Characters8() const { return ToString().Characters8(); }
  const UChar* Characters16() const { return ToString().Characters16(); }

 private:
  scoped_refptr<ParkableStringImpl> impl_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_H_
