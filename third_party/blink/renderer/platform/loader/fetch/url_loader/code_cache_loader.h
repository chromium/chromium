// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_LOADER_H_

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "url/gurl.h"

namespace blink {

// This class is loading V8 compilation code cache for scripts
// (either separate script resources, or inline scripts in html file).
// It is talking to the browser process and uses per-site isolated
// cache backend to avoid cross-origin contamination.
class BLINK_PLATFORM_EXPORT CodeCacheLoader : public WebCodeCacheLoader {
 public:
  CodeCacheLoader();
  // |code_cache_host| is the per-frame mojo interface that should be used when
  // fetching code cache. If this value is nullptr it uses per-process
  // interface.
  // TODO(mythria): Remove the per-process interface and only expect non nullptr
  // for |code_cache_host|.
  // |terminate_sync_load_event| is required on worker threads to monitor for
  // any terminate requests from main thread.
  CodeCacheLoader(mojom::CodeCacheHost* code_cache_host,
                  base::WaitableEvent* terminate_sync_load_event);

  ~CodeCacheLoader() override;

  // Fetches code cache corresponding to |url| and returns response in
  // |response_time_out| and |data_out|.  |response_time_out| and |data_out|
  // cannot be nullptrs. This only fetches from the Javascript cache.
  void FetchFromCodeCacheSynchronously(const WebURL& url,
                                       base::Time* response_time_out,
                                       mojo_base::BigBuffer* data_out) override;

  void FetchFromCodeCache(mojom::CodeCacheType cache_type,
                          const WebURL& url,
                          FetchCodeCacheCallback callback) override;

 private:
  void FetchFromCodeCacheImpl(mojom::CodeCacheType cache_type,
                              const WebURL& url,
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
  mojom::CodeCacheHost* code_cache_host_;
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;
  base::WeakPtrFactory<CodeCacheLoader> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER__FETCH_URL_LOADER_CODE_CACHE_LOADER_H_
