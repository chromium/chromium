// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_TYPE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_TYPE_H_

namespace mojo::internal {

// An enum used to determine what kind of sending validation mojo
// will do after serialization.
enum class SendValidation { kWarning, kFatal, kDefault = kWarning };

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_TYPE_H_
