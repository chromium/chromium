// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_SEND_VALIDATION_H_

#include "mojo/public/cpp/bindings/lib/has_send_validation_helper.h"
#include "mojo/public/cpp/bindings/string_data_view.h"

namespace mojo::internal {

// SendValidation isn't needed on strings.
template <typename MaybeConstUserType>
struct HasSendValidationSerialize<StringDataView, MaybeConstUserType>
    : std::false_type {};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_SEND_VALIDATION_H_
