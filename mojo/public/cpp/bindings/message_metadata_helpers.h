// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_

#include <cstdint>
#include <utility>

namespace mojo {

class Message;

// Alias for a function taking mojo::Message and returning an IPC hash (stable
// across Chrome versions) + an address of the symbol representing the
// mojo method name.
using MessageToMethodInfoCallback =
    std::pair<uint32_t, const void*> (*)(Message&);

// Alias for a function taking mojo::Message and returning method name.
using MessageToMethodNameCallback = const char* (*)(Message&);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_
