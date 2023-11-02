// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/wrapper_info.h"
#include "v8/include/v8-object.h"

namespace gin {

WrapperInfo* WrapperInfo::From(v8::Local<v8::Object> object) {
  if (object->InternalFieldCount() != kNumberOfInternalFields)
    return NULL;
  WrapperInfo* info = static_cast<WrapperInfo*>(
      object->GetAlignedPointerFromInternalField(kWrapperInfoIndex));
  return info->embedder == kEmbedderNativeGin ? info : NULL;
}

}  // namespace gin
