// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CACHE_TYPE_H_
#define NET_BASE_CACHE_TYPE_H_

namespace net {

// The types of caches that can be created.
enum CacheType {
  DISK_CACHE,                 // Disk is used as the backing storage.
  MEMORY_CACHE,               // Data is stored only in memory.
  REMOVED_MEDIA_CACHE,        // No longer in use.
  APP_CACHE,                  // Special case of DISK_CACHE.  Optimizes for
                              // cases where auto-eviction is not desired:
                              // e.g. cache_storage, service worker script cache
  SHADER_CACHE,               // Backing store for the GL shader cache.
  PNACL_CACHE,                // Backing store the PNaCl translation cache
  GENERATED_BYTE_CODE_CACHE,  // Backing store for renderer generated data like
                              // bytecode for JavaScript.
  GENERATED_NATIVE_CODE_CACHE,      // Backing store for renderer generated data
                                    // like native code for WebAssembly.
  GENERATED_WEBUI_BYTE_CODE_CACHE,  // Backing store for renderer generated data
                                    // like bytecode for JavaScript from WebUI
                                    // pages.
};

// The types of disk cache backend, only used at backend instantiation.
enum BackendType {
  CACHE_BACKEND_DEFAULT,
  CACHE_BACKEND_BLOCKFILE,  // The |BackendImpl|.
  CACHE_BACKEND_SIMPLE  // The |SimpleBackendImpl|.
};

}  // namespace net

#endif  // NET_BASE_CACHE_TYPE_H_
