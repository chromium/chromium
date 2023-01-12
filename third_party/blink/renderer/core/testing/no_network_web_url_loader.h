// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NO_NETWORK_WEB_URL_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NO_NETWORK_WEB_URL_LOADER_H_

#include "base/task/single_thread_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {

// A WebURLLoader simulating that requests time out forever due to no network.
// Useful for perftests that don't really want to benchmark URL loading.
class NoNetworkWebURLLoader : public WebURLLoader {
 public:
  NoNetworkWebURLLoader() = default;
  NoNetworkWebURLLoader(const NoNetworkWebURLLoader&) = delete;
  NoNetworkWebURLLoader& operator=(const NoNetworkWebURLLoader&) = delete;

  // WebURLLoader member functions:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient* client,
      WebURLResponse& response,
      absl::optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      uint64_t& encoded_body_length,
      blink::WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    // Nothing should call this in our test.
    NOTREACHED();
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override {
    // We simply never call back, simulating load times that are larger
    // than the test runtime.
  }
  void Freeze(WebLoaderFreezeMode mode) override {
    // Ignore.
  }
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) override {
    // Ignore.
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

class NoNetworkWebURLLoaderFactory : public WebURLLoaderFactory {
 public:
  NoNetworkWebURLLoaderFactory() = default;

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      CrossVariantMojoRemote<blink::mojom::KeepAliveHandleInterfaceBase>,
      WebBackForwardCacheLoaderHelper) override {
    return std::make_unique<NoNetworkWebURLLoader>();
  }
};

// A LocalFrameClient that uses NoNetworkWebURLLoader, so that nothing external
// is ever loaded.
class NoNetworkLocalFrameClient : public EmptyLocalFrameClient {
 public:
  NoNetworkLocalFrameClient() = default;

 private:
  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    return std::make_unique<NoNetworkWebURLLoaderFactory>();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NO_NETWORK_WEB_URL_LOADER_H_
