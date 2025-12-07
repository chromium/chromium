// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_INTERNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_INTERNAL_H_

#include <type_traits>
#include <utility>

#include "base/functional/bind_internal.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class RetainedRefWrapper;

namespace functional_internal {

template <size_t, typename T>
struct CheckGCedTypeRestriction {
  static_assert(
      !std::is_pointer<T>::value,
      "Raw pointers are not allowed to bind. Wrap it with either "
      "WrapPersistent, WrapWeakPersistent, WrapCrossThreadPersistent, "
      "WrapCrossThreadWeakPersistent, RetainedRef or Unretained.");
  static_assert(!IsMemberOrWeakMemberType<T>::value,
                "Member and WeakMember are not allowed to bind. Wrap it with "
                "either WrapPersistent, WrapWeakPersistent, "
                "WrapCrossThreadPersistent or WrapCrossThreadWeakPersistent.");
  static_assert(!IsGarbageCollectedTypeV<T>,
                "GCed types are forbidden as bound parameters.");
  static_assert(!IsStackAllocatedTypeV<T>,
                "Stack allocated types are forbidden as bound parameters.");
  static_assert(
      !(IsDisallowNew<T> && IsTraceableV<T>),
      "Traceable disallow new types are forbidden as bound parameters.");
};

template <typename Index, typename... Args>
struct CheckGCedTypeRestrictions;

template <size_t... Ns, typename... Args>
struct CheckGCedTypeRestrictions<std::index_sequence<Ns...>, Args...>
    : CheckGCedTypeRestriction<Ns, Args>... {
  static constexpr bool ok = true;
};

// Ideally, this would be an allowlist, but WebRTC exposes its own refcounted
// interface that happens to match //base, and it would be fragile to
// forward-declare WebRTC interfaces here.
//
// TODO(dcheng): Consider using a type tag to detect thread unsafety here in
// the future? Or perhaps C++26 reflection or the clang plugin can make it
// possible to write a trait that can catch transitive thread unsafety.
template <typename T>
struct IsNotThreadUnsafeRefCounted : std::true_type {};

template <typename T>
struct IsNotThreadUnsafeRefCounted<scoped_refptr<T>> : std::true_type {};

template <typename T>
  requires(IsSubclassOfTemplate<T, base::RefCounted>::value)
struct IsNotThreadUnsafeRefCounted<scoped_refptr<T>> : std::false_type {};

template <typename T>
struct IsNotThreadUnsafeRefCounted<::base::internal::RetainedRefWrapper<T>>
    : std::true_type {};

template <typename T>
  requires(IsSubclassOfTemplate<T, base::RefCounted>::value)
struct IsNotThreadUnsafeRefCounted<::base::internal::RetainedRefWrapper<T>>
    : std::false_type {};

template <typename T>
struct IsNotThreadUnsafeRefCounted<::blink::RetainedRefWrapper<T>>
    : std::true_type {};

template <typename T>
  requires(IsSubclassOfTemplate<T, base::RefCounted>::value)
struct IsNotThreadUnsafeRefCounted<::blink::RetainedRefWrapper<T>>
    : std::false_type {};

template <typename... Args>
inline constexpr bool kCheckNoThreadUnsafeRefCounted =
    std::conjunction_v<IsNotThreadUnsafeRefCounted<Args>...>;

}  // namespace functional_internal
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_FUNCTIONAL_INTERNAL_H_
