// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HAS_SEND_VALIDATION_HELPER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HAS_SEND_VALIDATION_HELPER_H_

namespace mojo::internal {

// Primary template for checking if a Serialize with SendValidation function
// is defined. Default to true, because most objects have validation on send
// capabilities.
template <typename MojomType, typename InputUserType, typename = void>
struct HasSendValidationSerialize : std::true_type {};

// Helper constant for easier usage.
template <typename MojomType, typename InputUserType>
inline constexpr bool HasSendValidationSerialize_v =
    HasSendValidationSerialize<MojomType, InputUserType>::value;
}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HAS_SEND_VALIDATION_HELPER_H_
