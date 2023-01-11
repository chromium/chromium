// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_ERROR_CALLBACK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_ERROR_CALLBACK_H_

#include "base/functional/callback.h"

namespace mojo {

// These callback types accept user-defined disconnect reason and description.
// If the other side specifies a reason on closing the connection, it will be
// passed to the error handler.
using ConnectionErrorWithReasonCallback =
    base::OnceCallback<void(uint32_t /* custom_reason */,
                            const std::string& /* description */)>;
using RepeatingConnectionErrorWithReasonCallback =
    base::RepeatingCallback<void(uint32_t /* custom_reason */,
                                 const std::string& /* description */)>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_ERROR_CALLBACK_H_
