// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_URL_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"

namespace blink {

// URLLoaderFactory for InternetDisconnectedURLLoader.
class BLINK_PLATFORM_EXPORT InternetDisconnectedURLLoaderFactory final
    : public URLLoaderFactory {
 public:
  std::unique_ptr<URLLoader> CreateURLLoader(
      const network::ResourceRequest&,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
      Vector<std::unique_ptr<URLLoaderThrottle>> throttles) override;
};

// URLLoader which always returns an internet disconnected error. At present,
// this is used for ServiceWorker's offline-capability-check fetch event.
class InternetDisconnectedURLLoader final : public URLLoader {
 public:
  explicit InternetDisconnectedURLLoader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_handle);
  ~InternetDisconnectedURLLoader() override;

  // URLLoader implementation:
  void LoadSynchronously(std::unique_ptr<network::ResourceRequest> request,
                         scoped_refptr<const SecurityOrigin> top_frame_origin,
                         bool download_to_blob,
                         bool no_mime_sniffing,
                         base::TimeDelta timeout_interval,
                         URLLoaderClient*,
                         WebURLResponse&,
                         std::optional<WebURLError>&,
                         scoped_refptr<SharedBuffer>&,
                         int64_t& encoded_data_length,
                         uint64_t& encoded_body_length,
                         scoped_refptr<BlobDataHandle>& downloaded_blob,
                         std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
                             resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<const SecurityOrigin> top_frame_origin,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      CodeCacheHost* code_cache_host,
      URLLoaderClient* client) override;
  void Freeze(LoaderFreezeMode mode) override;
  void DidChangePriority(WebURLRequest::Priority, int) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override;

 private:
  void DidFail(URLLoaderClient* client, const WebURLError& error);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<InternetDisconnectedURLLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_URL_LOADER_H_
