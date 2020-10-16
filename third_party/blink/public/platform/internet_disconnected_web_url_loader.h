// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace blink {

// WebURLLoaderFactory for InternetDisconnectedWebURLLoader.
class BLINK_PLATFORM_EXPORT InternetDisconnectedWebURLLoaderFactory final
    : public WebURLLoaderFactory {
 public:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
};

// WebURLLoader which always returns an internet disconnected error. At present,
// this is used for ServiceWorker's offline-capability-check fetch event.
class InternetDisconnectedWebURLLoader final : public WebURLLoader {
 public:
  explicit InternetDisconnectedWebURLLoader(
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle);
  ~InternetDisconnectedWebURLLoader() override;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override;
  void SetDefersLoading(bool defers) override;
  void DidChangePriority(WebURLRequest::Priority, int) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

 private:
  void DidFail(WebURLLoaderClient* client, const WebURLError& error);

  std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
      task_runner_handle_;
  base::WeakPtrFactory<InternetDisconnectedWebURLLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
