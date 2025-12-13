// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Wrapper around mojo::Remote which can be shared among multiple
// CodeCacheLoaders that may outlive the frame lifetime, e.g. due to
// teardown ordering.
//
// Important: This class is not allowed to be on the Oilpan heap, since accesses
// to the mojo::Remote and the data it holds reliy on the object being valid
// (and not poisoned) until the destructor is called.
class BLINK_PLATFORM_EXPORT CodeCacheHost {
 public:
  static std::unique_ptr<CodeCacheHost> Create(
      mojo::Remote<mojom::blink::CodeCacheHost> remote);
  CodeCacheHost(const CodeCacheHost&) = delete;
  CodeCacheHost& operator=(const CodeCacheHost&) = delete;
  virtual ~CodeCacheHost() = default;

  // Get a weak pointer to this CodeCacheHost. Only valid when the remote
  // has been bound.
  virtual base::WeakPtr<CodeCacheHost> GetWeakPtr() = 0;

  virtual mojom::blink::CodeCacheHost* get() = 0;
  virtual mojom::blink::CodeCacheHost& operator*() = 0;
  virtual mojom::blink::CodeCacheHost* operator->() = 0;

 protected:
  CodeCacheHost() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
