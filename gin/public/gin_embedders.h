// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_GIN_EMBEDDERS_H_
#define GIN_PUBLIC_GIN_EMBEDDERS_H_

#include <cstdint>

namespace gin {

// The GinEmbedder is used to identify the owner of embedder data stored on
// v8 objects, and is used as in index into the embedder data slots of a
// v8::Isolate.
//
// GinEmbedder is using uint16_t as underlying storage as V8 requires that
// external pointers in embedder fields are at least 2-byte-aligned.
enum GinEmbedder : uint16_t {
  kEmbedderNativeGin,
  kEmbedderBlink,
  kEmbedderPDFium,
  kEmbedderFuchsia,
};

}  // namespace gin

#endif  // GIN_PUBLIC_GIN_EMBEDDERS_H_
