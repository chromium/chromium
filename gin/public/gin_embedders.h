// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_GIN_EMBEDDERS_H_
#define GIN_PUBLIC_GIN_EMBEDDERS_H_

namespace gin {

// The GinEmbedder is used to identify the owner of embedder data stored on
// v8 objects, and is used as in index into the embedder data slots of a
// v8::Isolate.

enum GinEmbedder {
  kEmbedderNativeGin,
  kEmbedderBlink,
  kEmbedderPDFium,
  kEmbedderFuchsia,
};

}  // namespace gin

#endif  // GIN_PUBLIC_GIN_EMBEDDERS_H_
