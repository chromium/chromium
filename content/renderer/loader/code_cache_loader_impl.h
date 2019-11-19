// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CODE_CACHE_LOADER_IMPL_H_
#define CONTENT_RENDERER_LOADER_CODE_CACHE_LOADER_IMPL_H_

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "third_party/blink/public/platform/code_cache_loader.h"
#include "url/gurl.h"

namespace content {

// This class is loading V8 compilation code cache for scripts
// (either separate script resources, or inline scripts in html file).
// It is talking to the browser process and uses per-site isolated
// cache backend to avoid cross-origin contamination.
class CodeCacheLoaderImpl : public blink::CodeCacheLoader {
 public:
  CodeCacheLoaderImpl();
  explicit CodeCacheLoaderImpl(base::WaitableEvent* terminate_sync_load_event);

  ~CodeCacheLoaderImpl() override;

  // Fetches code cache corresponding to |url| and returns response in
  // |response_time_out| and |data_out|.  |response_time_out| and |data_out|
  // cannot be nullptrs. This only fetches from the Javascript cache.
  void FetchFromCodeCacheSynchronously(const GURL& url,
                                       base::Time* response_time_out,
                                       mojo_base::BigBuffer* data_out) override;

  void FetchFromCodeCache(blink::mojom::CodeCacheType cache_type,
                          const GURL& url,
                          FetchCodeCacheCallback callback) override;

 private:
  void FetchFromCodeCacheImpl(blink::mojom::CodeCacheType cache_type,
                              const GURL& url,
                              FetchCodeCacheCallback callback,
                              base::WaitableEvent* event);

  void OnReceiveCachedCode(FetchCodeCacheCallback callback,
                           base::WaitableEvent* event,
                           base::Time response_time,
                           mojo_base::BigBuffer data);
  void ReceiveDataForSynchronousFetch(base::Time response_time,
                                      mojo_base::BigBuffer data);
  void OnTerminate(base::WaitableEvent* fetch_event,
                   base::WaitableEvent* terminate_event);

  base::Time response_time_for_sync_load_;
  mojo_base::BigBuffer data_for_sync_load_;
  bool terminated_ = false;
  base::WaitableEventWatcher terminate_watcher_;
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;
  base::WeakPtrFactory<CodeCacheLoaderImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif
