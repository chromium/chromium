// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_ERROR_H_

#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

class CacheStorageError {
  STATIC_ONLY(CacheStorageError);

 public:
  // For CallbackPromiseAdapter. Ownership of a given error is not
  // transferred.
  using WebType = mojom::CacheStorageError;
  static DOMException* Take(ScriptPromiseResolver*,
                            mojom::CacheStorageError web_error) {
    return CreateException(web_error);
  }

  static DOMException* CreateException(mojom::CacheStorageError web_error,
                                       const String& message = String());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_ERROR_H_
