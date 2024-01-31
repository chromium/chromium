// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_

#include <stdint.h>
#include <limits>

#include "base/check_op.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle_signals_state.h"

namespace mojo {

// OVERVIEW
//
// |Handle| and |...Handle|:
//
// |Handle| is a simple, copyable wrapper for the C type |MojoHandle| (which is
// just an integer). Its purpose is to increase type-safety, not provide
// lifetime management. For the same purpose, we have trivial *subclasses* of
// |Handle|, e.g., |MessagePipeHandle| and |DataPipeProducerHandle|. |Handle|
// and its subclasses impose *no* extra overhead over using |MojoHandle|s
// directly.
//
// Note that though we provide constructors for |Handle|/|...Handle| from a
// |MojoHandle|, we do not provide, e.g., a constructor for |MessagePipeHandle|
// from a |Handle|. This is for type safety: If we did, you'd then be able to
// construct a |MessagePipeHandle| from, e.g., a |DataPipeProducerHandle| (since
// it's a |Handle|).
//
// |ScopedHandleBase| and |Scoped...Handle|:
//
// |ScopedHandleBase<HandleType>| is a templated scoped wrapper, for the handle
// types above (in the same sense that a C++11 |unique_ptr<T>| is a scoped
// wrapper for a |T*|). It provides lifetime management, closing its owned
// handle on destruction. It also provides move semantics, again along the lines
// of C++11's |unique_ptr|. A moved-from |ScopedHandleBase<HandleType>| sets its
// handle value to MOJO_HANDLE_INVALID.
//
// |ScopedHandle| is just (a typedef of) a |ScopedHandleBase<Handle>|.
// Similarly, |ScopedMessagePipeHandle| is just a
// |ScopedHandleBase<MessagePipeHandle>|. Etc. Note that a
// |ScopedMessagePipeHandle| is *not* a (subclass of) |ScopedHandle|.
//
// Wrapper functions:
//
// We provide simple wrappers for the |Mojo...()| functions (in
// mojo/public/c/system/core.h -- see that file for details on individual
// functions).
//
// The general guideline is functions that imply ownership transfer of a handle
// should take (or produce) an appropriate |Scoped...Handle|, while those that
// don't take a |...Handle|. For example, |CreateMessagePipe()| has two
// |ScopedMessagePipe| "out" parameters, whereas |Wait()| and |WaitMany()| take
// |Handle| parameters. Some, have both: e.g., |DuplicatedBuffer()| takes a
// suitable (unscoped) handle (e.g., |SharedBufferHandle|) "in" parameter and
// produces a suitable scoped handle (e.g., |ScopedSharedBufferHandle| a.k.a.
// |ScopedHandleBase<SharedBufferHandle>|) as an "out" parameter.
//
// An exception are some of the |...Raw()| functions. E.g., |CloseRaw()| takes a
// |Handle|, leaving the user to discard the wrapper.
//
// ScopedHandleBase ------------------------------------------------------------

// Scoper for the actual handle types defined further below. It's move-only,
// like the C++11 |unique_ptr|.
template <class HandleType>
class ScopedHandleBase {
 public:
  using RawHandleType = HandleType;

  ScopedHandleBase() {}
  explicit ScopedHandleBase(HandleType handle) : handle_(handle) {}

  ScopedHandleBase(const ScopedHandleBase&) = delete;
  ScopedHandleBase& operator=(const ScopedHandleBase&) = delete;

  ~ScopedHandleBase() { CloseIfNecessary(); }

  template <class CompatibleHandleType>
  explicit ScopedHandleBase(ScopedHandleBase<CompatibleHandleType> other)
      : handle_(other.release()) {}

  // Move-only constructor and operator=.
  ScopedHandleBase(ScopedHandleBase&& other) noexcept
      : handle_(other.release()) {}
  ScopedHandleBase& operator=(ScopedHandleBase&& other) noexcept {
    if (&other != this) {
      CloseIfNecessary();
      handle_ = other.release();
    }
    return *this;
  }

  const HandleType& get() const { return handle_; }
  const HandleType* operator->() const {
    DCHECK(handle_.is_valid());
    return &handle_;
  }

  template <typename PassedHandleType>
  static ScopedHandleBase<HandleType> From(
      ScopedHandleBase<PassedHandleType> other) {
    static_assert(
        sizeof(static_cast<PassedHandleType*>(static_cast<HandleType*>(0))),
        "HandleType is not a subtype of PassedHandleType");
    return ScopedHandleBase<HandleType>(
        static_cast<HandleType>(other.release().value()));
  }

  void swap(ScopedHandleBase& other) { handle_.swap(other.handle_); }

  [[nodiscard]] HandleType release() {
    HandleType rv;
    rv.swap(handle_);
    return rv;
  }

  void reset(HandleType handle = HandleType()) {
    CloseIfNecessary();
    handle_ = handle;
  }

  bool is_valid() const { return handle_.is_valid(); }

  explicit operator bool() const { return handle_.is_valid(); }

  bool operator==(const ScopedHandleBase& other) const {
    return handle_.value() == other.get().value();
  }

 private:
  void CloseIfNecessary() {
    if (handle_.is_valid())
      handle_.Close();
  }

  HandleType handle_;
};

template <typename HandleType>
inline ScopedHandleBase<HandleType> MakeScopedHandle(HandleType handle) {
  return ScopedHandleBase<HandleType>(handle);
}

// Handle ----------------------------------------------------------------------

const MojoHandle kInvalidHandleValue = MOJO_HANDLE_INVALID;

// Wrapper base class for |MojoHandle|.
class Handle {
 public:
  Handle() : value_(kInvalidHandleValue) {}
  explicit Handle(MojoHandle value) : value_(value) {}
  ~Handle() {}

  void swap(Handle& other) {
    MojoHandle temp = value_;
    value_ = other.value_;
    other.value_ = temp;
  }

  bool is_valid() const { return value_ != kInvalidHandleValue; }

  explicit operator bool() const { return value_ != kInvalidHandleValue; }

  const MojoHandle& value() const { return value_; }
  MojoHandle* mutable_value() { return &value_; }
  void set_value(MojoHandle value) { value_ = value; }

  void Close() {
    DCHECK(is_valid());
    [[maybe_unused]] MojoResult result = MojoClose(value_);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }

  HandleSignalsState QuerySignalsState() const {
    HandleSignalsState signals_state;
    [[maybe_unused]] MojoResult result = MojoQueryHandleSignalsState(
        value_, static_cast<MojoHandleSignalsState*>(&signals_state));
    DCHECK_EQ(MOJO_RESULT_OK, result);
    return signals_state;
  }

 private:
  MojoHandle value_;

  // Copying and assignment allowed.
};

// Should have zero overhead.
static_assert(sizeof(Handle) == sizeof(MojoHandle), "Bad size for C++ Handle");

// The scoper should also impose no more overhead.
typedef ScopedHandleBase<Handle> ScopedHandle;
static_assert(sizeof(ScopedHandle) == sizeof(Handle),
              "Bad size for C++ ScopedHandle");

// |Close()| takes ownership of the handle, since it'll invalidate it.
// Note: There's nothing to do, since the argument will be destroyed when it
// goes out of scope.
template <class HandleType>
inline void Close(ScopedHandleBase<HandleType> /*handle*/) {
}

// Most users should typically use |Close()| (above) instead.
inline MojoResult CloseRaw(Handle handle) {
  return MojoClose(handle.value());
}

// Strict weak ordering, so that |Handle|s can be used as keys in |std::map|s,
inline bool operator<(const Handle a, const Handle b) {
  return a.value() < b.value();
}

// Comparison, so that |Handle|s can be used as keys in hash maps.
inline bool operator==(const Handle a, const Handle b) {
  return a.value() == b.value();
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_
