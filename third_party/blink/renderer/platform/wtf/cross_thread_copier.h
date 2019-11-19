/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_H_

#include <memory>
#include <string>
#include <vector>
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/deprecated_interface_types_forward.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"  // FunctionThreadAffinity
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace base {
template <typename, typename>
class RefCountedThreadSafe;
class TimeDelta;
class TimeTicks;
class Time;
class UnguessableToken;
}  // namespace base

class SkRefCnt;
template <typename T>
class sk_sp;

namespace gfx {
class Size;
}

namespace gpu {
struct SyncToken;
}

namespace mojo {
template <typename Interface>
class PendingReceiver;
}

namespace WTF {

template <typename T>
struct CrossThreadCopierPassThrough {
  STATIC_ONLY(CrossThreadCopierPassThrough);
  typedef T Type;
  static Type Copy(const T& parameter) { return parameter; }
};

template <typename T, bool isArithmeticOrEnum>
struct CrossThreadCopierBase;

// Arithmetic values (integers or floats) and enums can be safely copied.
template <typename T>
struct CrossThreadCopierBase<T, true> : public CrossThreadCopierPassThrough<T> {
  STATIC_ONLY(CrossThreadCopierBase);
};

template <typename T>
struct CrossThreadCopier
    : public CrossThreadCopierBase<T,
                                   std::is_arithmetic<T>::value ||
                                       std::is_enum<T>::value> {
  STATIC_ONLY(CrossThreadCopier);
};

// CrossThreadCopier specializations follow.
template <typename T>
struct CrossThreadCopier<RetainedRefWrapper<T>> {
  STATIC_ONLY(CrossThreadCopier);
  static_assert(IsSubclassOfTemplate<T, base::RefCountedThreadSafe>::value,
                "scoped_refptr<T> can be passed across threads only if T is "
                "ThreadSafeRefCounted or base::RefCountedThreadSafe.");
  using Type = RetainedRefWrapper<T>;
  static Type Copy(Type pointer) { return pointer; }
};
template <typename T>
struct CrossThreadCopier<scoped_refptr<T>> {
  STATIC_ONLY(CrossThreadCopier);
  static_assert(IsSubclassOfTemplate<T, base::RefCountedThreadSafe>::value,
                "scoped_refptr<T> can be passed across threads only if T is "
                "ThreadSafeRefCounted or base::RefCountedThreadSafe.");
  using Type = scoped_refptr<T>;
  static scoped_refptr<T> Copy(scoped_refptr<T> pointer) { return pointer; }
};
template <typename T>
struct CrossThreadCopier<sk_sp<T>>
    : public CrossThreadCopierPassThrough<sk_sp<T>> {
  STATIC_ONLY(CrossThreadCopier);
  static_assert(std::is_base_of<SkRefCnt, T>::value,
                "sk_sp<T> can be passed across threads only if T is SkRefCnt.");
};

template <>
struct CrossThreadCopier<base::TimeDelta>
    : public CrossThreadCopierPassThrough<base::TimeDelta> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::TimeTicks>
    : public CrossThreadCopierPassThrough<base::TimeTicks> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::Time>
    : public CrossThreadCopierPassThrough<base::Time> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::File> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = base::File;
  static Type Copy(Type pointer) { return pointer; }
};

template <>
struct CrossThreadCopier<base::UnguessableToken>
    : public CrossThreadCopierPassThrough<base::UnguessableToken> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<gpu::SyncToken>
    : public CrossThreadCopierPassThrough<gpu::SyncToken> {
  STATIC_ONLY(CrossThreadCopier);
};

// nullptr_t can be passed through without any changes.
template <>
struct CrossThreadCopier<std::nullptr_t>
    : public CrossThreadCopierPassThrough<std::nullptr_t> {
  STATIC_ONLY(CrossThreadCopier);
};

// To allow a type to be passed across threads using its copy constructor, add a
// forward declaration of the type and provide a specialization of
// CrossThreadCopier<T> in this file.

template <typename T, typename Deleter>
struct CrossThreadCopier<std::unique_ptr<T, Deleter>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::unique_ptr<T, Deleter>;
  static std::unique_ptr<T, Deleter> Copy(std::unique_ptr<T, Deleter> pointer) {
    return pointer;  // This is in fact a move.
  }
};

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<
    Vector<std::unique_ptr<T>, inlineCapacity, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = Vector<std::unique_ptr<T>, inlineCapacity, Allocator>;
  static Type Copy(Type pointer) {
    return pointer;  // This is in fact a move.
  }
};

template <>
struct CrossThreadCopier<std::vector<uint8_t>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::vector<uint8_t>;
  static Type Copy(Type value) { return value; }
};

template <class CharT, class Traits, class Allocator>
struct CrossThreadCopier<std::basic_string<CharT, Traits, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::basic_string<CharT, Traits, Allocator>;
  static Type Copy(Type string) {
    return string;  // This is in fact a move.
  }
};

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<Vector<T, inlineCapacity, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  static_assert(std::is_enum<T>() || std::is_arithmetic<T>(),
                "Vectors of this type are not known to be safe to pass to "
                "another thread.");
  using Type = Vector<T, inlineCapacity, Allocator>;
  static Type Copy(Type value) { return value; }
};

template <wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<Vector<String, inlineCapacity, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = Vector<String, inlineCapacity, Allocator>;
  static Type Copy(const Type& value) {
    Type result;
    result.ReserveInitialCapacity(value.size());
    for (const auto& element : value)
      result.push_back(element.IsolatedCopy());
    return result;
  }
};

template <typename T>
struct CrossThreadCopier<CrossThreadUnretainedWrapper<T>>
    : public CrossThreadCopierPassThrough<CrossThreadUnretainedWrapper<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<base::WeakPtr<T>>
    : public CrossThreadCopierPassThrough<base::WeakPtr<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<PassedWrapper<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = PassedWrapper<typename CrossThreadCopier<T>::Type>;
  static Type Copy(PassedWrapper<T>&& value) {
    return WTF::Passed(CrossThreadCopier<T>::Copy(value.MoveOut()));
  }
};

template <typename Signature>
struct CrossThreadCopier<CrossThreadFunction<Signature>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = CrossThreadFunction<Signature>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <typename Signature>
struct CrossThreadCopier<CrossThreadOnceFunction<Signature>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = CrossThreadOnceFunction<Signature>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<String> {
  STATIC_ONLY(CrossThreadCopier);
  typedef String Type;
  WTF_EXPORT static Type Copy(const String&);
};

template <typename Interface>
struct CrossThreadCopier<mojo::PendingReceiver<Interface>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = mojo::PendingReceiver<Interface>;
  static Type Copy(Type receiver) {
    return receiver;  // This is in fact a move.
  }
};

template <>
struct CrossThreadCopier<blink::MessagePortChannel> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::MessagePortChannel;
  static Type Copy(Type pointer) {
    return pointer;  // This is in fact a move.
  }
};

template <wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<
    Vector<blink::MessagePortChannel, inlineCapacity, Allocator>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = Vector<blink::MessagePortChannel, inlineCapacity, Allocator>;
  static Type Copy(Type pointer) {
    return pointer;  // This is in fact a move.
  }
};

template <>
struct CrossThreadCopier<gfx::Size>
    : public CrossThreadCopierPassThrough<gfx::Size> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_H_
