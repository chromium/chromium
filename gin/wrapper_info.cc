// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/wrapper_info.h"
#include "v8/include/v8-object.h"

namespace gin {

DeprecatedWrapperInfo* DeprecatedWrapperInfo::From(
    v8::Local<v8::Object> object) {
  if (object->InternalFieldCount() != kNumberOfInternalFields)
    return NULL;
  DeprecatedWrapperInfo* info = static_cast<DeprecatedWrapperInfo*>(
      object->GetAlignedPointerFromInternalField(kWrapperInfoIndex,
                                                 kDeprecatedData));
  return info->embedder == kEmbedderNativeGin ? info : NULL;
}

}  // namespace gin
