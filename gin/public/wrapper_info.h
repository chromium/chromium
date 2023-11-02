// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_WRAPPER_INFO_H_
#define GIN_PUBLIC_WRAPPER_INFO_H_

#include "gin/gin_export.h"
#include "gin/public/gin_embedders.h"
#include "v8/include/v8-forward.h"

namespace gin {

// Gin embedder that use their own WrapperInfo-like structs must ensure that
// the first field is of type GinEmbedderId and has the correct id set. They
// also should use kWrapperInfoIndex to start their WrapperInfo-like struct
// and ensure that all objects have kNumberOfInternalFields internal fields.

enum InternalFields {
  kWrapperInfoIndex,
  kEncodedValueIndex,
  kNumberOfInternalFields,
};

struct GIN_EXPORT WrapperInfo {
  static WrapperInfo* From(v8::Local<v8::Object> object);
  const GinEmbedder embedder;
};

}  // namespace gin

#endif  // GIN_PUBLIC_WRAPPER_INFO_H_
