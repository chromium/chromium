// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_SEND_VALIDATION_H_

#include "mojo/public/cpp/bindings/lib/has_send_validation_helper.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"

// This header defines sepcializations for HasSendValidationSerialize for
// handles

namespace mojo::internal {

// Handle Serialization has no send validation:
template <typename T>
struct HasSendValidationSerialize<ScopedHandleBase<T>, ScopedHandleBase<T>>
    : std::false_type {};

template <>
struct HasSendValidationSerialize<PlatformHandle, PlatformHandle>
    : std::false_type {};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_SEND_VALIDATION_H_
