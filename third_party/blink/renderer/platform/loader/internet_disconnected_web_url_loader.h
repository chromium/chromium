// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/web_url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/web_url_loader_factory.h"

namespace blink {

class WebURLRequestExtraData;

// WebURLLoaderFactory for InternetDisconnectedWebURLLoader.
class BLINK_PLATFORM_EXPORT InternetDisconnectedWebURLLoaderFactory final
    : public WebURLLoaderFactory {
 public:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper) override;
};

// WebURLLoader which always returns an internet disconnected error. At present,
// this is used for ServiceWorker's offline-capability-check fetch event.
class InternetDisconnectedWebURLLoader final : public WebURLLoader {
 public:
  explicit InternetDisconnectedWebURLLoader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_handle);
  ~InternetDisconnectedWebURLLoader() override;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      absl::optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      uint64_t& encoded_body_length,
      scoped_refptr<BlobDataHandle>& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override;
  void Freeze(WebLoaderFreezeMode mode) override;
  void DidChangePriority(WebURLRequest::Priority, int) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override;

 private:
  void DidFail(WebURLLoaderClient* client, const WebURLError& error);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<InternetDisconnectedWebURLLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
