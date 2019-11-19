// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CACHE_TYPE_H_
#define NET_BASE_CACHE_TYPE_H_

namespace net {

// The types of caches that can be created.
// Note: enum values used in some metrics, so add new values at end only,
// and don't renumber old ones.
enum CacheType {
  DISK_CACHE,                 // Disk is used as the backing storage.
  MEMORY_CACHE,               // Data is stored only in memory.
  REMOVED_MEDIA_CACHE,        // No longer in use.
  APP_CACHE,                  // Backing store for an AppCache.
  SHADER_CACHE,               // Backing store for the GL shader cache.
  PNACL_CACHE,                // Backing store the PNaCl translation cache
  GENERATED_BYTE_CODE_CACHE,  // Backing store for renderer generated data like
                              // bytecode for JavaScript.
  GENERATED_NATIVE_CODE_CACHE,  // Backing store for renderer generated data
                                // like native code for WebAssembly.
};

// The types of disk cache backend, only used at backend instantiation.
enum BackendType {
  CACHE_BACKEND_DEFAULT,
  CACHE_BACKEND_BLOCKFILE,  // The |BackendImpl|.
  CACHE_BACKEND_SIMPLE  // The |SimpleBackendImpl|.
};

}  // namespace net

#endif  // NET_BASE_CACHE_TYPE_H_
