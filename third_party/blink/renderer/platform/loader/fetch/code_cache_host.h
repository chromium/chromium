// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Wrapper around mojo::Remote which can be shared among multiple
// CodeCacheLoaders that may outlive the frame lifetime, e.g. due to
// teardown ordering.
class BLINK_PLATFORM_EXPORT CodeCacheHost {
 public:
  // Get a weak pointer to this CodeCacheHost. Only valid when the remote
  // has been bound.
  base::WeakPtr<CodeCacheHost> GetWeakPtr() {
    DCHECK(remote_.is_bound());
    return weak_factory_.GetWeakPtr();
  }

  mojom::CodeCacheHost* get() { return remote_.get(); }
  mojom::CodeCacheHost& operator*() { return *remote_.get(); }
  mojom::CodeCacheHost* operator->() { return remote_.get(); }

  void SetRemote(mojo::Remote<mojom::CodeCacheHost> remote) {
    DCHECK(!remote_);
    if (remote.is_connected()) {
      remote_ = std::move(remote);
      // Call the CodeCacheHost reset handler rather than the remote's one, to
      // make sure to invalidate the weakptrs. Using Unretained(this) is safe
      // here, because the remote is owned by this (so its lifetime can't exceed
      // this's), and the disconnect handler is owned by the remote.
      remote_.set_disconnect_handler(
          base::BindOnce(&CodeCacheHost::Reset, base::Unretained(this)));
    }
  }

  bool HasBoundRemote() const { return remote_.is_bound(); }

  void Reset() {
    remote_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

 private:
  mojo::Remote<mojom::CodeCacheHost> remote_;
  base::WeakPtrFactory<CodeCacheHost> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
