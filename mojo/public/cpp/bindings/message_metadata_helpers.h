// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_

#include <cstdint>

namespace mojo {

class Message;

using IPCStableHashFunction = uint32_t (*)();
// Alias for a function taking mojo::Message and returning a pointer to a
// function that computes an IPC hash (stable across Chrome versions).
// An address of the returned function is used for identifying mojo
// method after symbolization.
// The callback could have returned a pair (function address, IPC hash value)
// instead, but returning only the function address results in ~20k binary size
// savings.
using MessageToMethodInfoCallback = IPCStableHashFunction (*)(Message&);

// Alias for a function taking mojo::Message and returning method name.
using MessageToMethodNameCallback = const char* (*)(Message&);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_METADATA_HELPERS_H_
