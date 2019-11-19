// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "v8/include/v8.h"

namespace blink {

// StringResource is a helper class for V8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class StringResourceBase {
  USING_FAST_MALLOC(StringResourceBase);

 public:
  explicit StringResourceBase(const String& string) : plain_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  explicit StringResourceBase(const AtomicString& string)
      : plain_string_(string.GetString()), atomic_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  explicit StringResourceBase(const ParkableString& string)
      : parkable_string_(string) {
#if DCHECK_IS_ON()
    thread_id_ = WTF::CurrentThread();
#endif
    // TODO(lizeb): This is only true without compression.
    DCHECK(!string.IsNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.CharactersSizeInBytes());
  }

  virtual ~StringResourceBase() {
#if DCHECK_IS_ON()
    DCHECK(thread_id_ == WTF::CurrentThread());
#endif
    int64_t reduced_external_memory = plain_string_.CharactersSizeInBytes();
    if (plain_string_.Impl() != atomic_string_.Impl() &&
        !atomic_string_.IsNull())
      reduced_external_memory += atomic_string_.CharactersSizeInBytes();
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -reduced_external_memory);
  }

  String GetWTFString() {
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return parkable_string_.ToString();
    }
    return plain_string_;
  }

  AtomicString GetAtomicString() {
#if DCHECK_IS_ON()
    DCHECK(thread_id_ == WTF::CurrentThread());
#endif
    if (!parkable_string_.IsNull()) {
      DCHECK(plain_string_.IsNull());
      DCHECK(atomic_string_.IsNull());
      return AtomicString(parkable_string_.ToString());
    }
    if (atomic_string_.IsNull()) {
      atomic_string_ = AtomicString(plain_string_);
      DCHECK(!atomic_string_.IsNull());
      if (plain_string_.Impl() != atomic_string_.Impl()) {
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
            atomic_string_.CharactersSizeInBytes());
      }
    }
    return atomic_string_;
  }

 protected:
  // A shallow copy of the string. Keeps the string buffer alive until the V8
  // engine garbage collects it.
  String plain_string_;
  // If this string is atomic or has been made atomic earlier the
  // atomic string is held here. In the case where the string starts
  // off non-atomic and becomes atomic later it is necessary to keep
  // the original string alive because v8 may keep derived pointers
  // into that string.
  AtomicString atomic_string_;
  // If this string is parkable, its value is held here, and the other
  // members above are null.
  ParkableString parkable_string_;

 private:
#if DCHECK_IS_ON()
  base::PlatformThreadId thread_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(StringResourceBase);
};

// Even though StringResource{8,16}Base are effectively empty in release mode,
// they are needed as they serve as a common ancestor to Parkable and regular
// strings.
//
// See the comment in |ToBlinkString()|'s implementation for the rationale.
class StringResource16Base : public StringResourceBase,
                             public v8::String::ExternalStringResource {
 public:
  explicit StringResource16Base(const String& string)
      : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit StringResource16Base(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(!string.Is8Bit());
  }

  explicit StringResource16Base(const ParkableString& parkable_string)
      : StringResourceBase(parkable_string) {
    DCHECK(!parkable_string.Is8Bit());
  }

  DISALLOW_COPY_AND_ASSIGN(StringResource16Base);
};

class StringResource16 final : public StringResource16Base {
 public:
  explicit StringResource16(const String& string)
      : StringResource16Base(string) {}

  explicit StringResource16(const AtomicString& string)
      : StringResource16Base(string) {}

  size_t length() const override { return plain_string_.Impl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        plain_string_.Impl()->Characters16());
  }

  DISALLOW_COPY_AND_ASSIGN(StringResource16);
};

class ParkableStringResource16 final : public StringResource16Base {
 public:
  explicit ParkableStringResource16(const ParkableString& string)
      : StringResource16Base(string) {}

  bool IsCacheable() const override {
    return !parkable_string_.may_be_parked();
  }

  void Lock() const override { parkable_string_.Lock(); }

  void Unlock() const override { parkable_string_.Unlock(); }

  size_t length() const override { return parkable_string_.length(); }

  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(parkable_string_.Characters16());
  }

  DISALLOW_COPY_AND_ASSIGN(ParkableStringResource16);
};

class StringResource8Base : public StringResourceBase,
                            public v8::String::ExternalOneByteStringResource {
 public:
  explicit StringResource8Base(const String& string)
      : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit StringResource8Base(const AtomicString& string)
      : StringResourceBase(string) {
    DCHECK(string.Is8Bit());
  }

  explicit StringResource8Base(const ParkableString& parkable_string)
      : StringResourceBase(parkable_string) {
    DCHECK(parkable_string.Is8Bit());
  }

  DISALLOW_COPY_AND_ASSIGN(StringResource8Base);
};

class StringResource8 final : public StringResource8Base {
 public:
  explicit StringResource8(const String& string)
      : StringResource8Base(string) {}

  explicit StringResource8(const AtomicString& string)
      : StringResource8Base(string) {}

  size_t length() const override { return plain_string_.Impl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(plain_string_.Impl()->Characters8());
  }

  DISALLOW_COPY_AND_ASSIGN(StringResource8);
};

class ParkableStringResource8 final : public StringResource8Base {
 public:
  explicit ParkableStringResource8(const ParkableString& string)
      : StringResource8Base(string) {}

  bool IsCacheable() const override {
    return !parkable_string_.may_be_parked();
  }

  void Lock() const override { parkable_string_.Lock(); }

  void Unlock() const override { parkable_string_.Unlock(); }

  size_t length() const override { return parkable_string_.length(); }

  const char* data() const override {
    return reinterpret_cast<const char*>(parkable_string_.Characters8());
  }

  DISALLOW_COPY_AND_ASSIGN(ParkableStringResource8);
};

enum ExternalMode { kExternalize, kDoNotExternalize };

template <typename StringType>
PLATFORM_EXPORT StringType ToBlinkString(v8::Local<v8::String>, ExternalMode);
PLATFORM_EXPORT String ToBlinkString(int value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_STRING_RESOURCE_H_
