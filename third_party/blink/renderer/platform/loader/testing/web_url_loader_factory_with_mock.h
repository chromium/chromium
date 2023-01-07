// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_WEB_URL_LOADER_FACTORY_WITH_MOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_WEB_URL_LOADER_FACTORY_WITH_MOCK_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace blink {

class WebURLLoaderMockFactory;

class WebURLLoaderFactoryWithMock : public WebURLLoaderFactory {
 public:
  explicit WebURLLoaderFactoryWithMock(WebURLLoaderMockFactory*);
  ~WebURLLoaderFactoryWithMock() override;

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      CrossVariantMojoRemote<blink::mojom::KeepAliveHandleInterfaceBase>,
      WebBackForwardCacheLoaderHelper) override;

 private:
  // Not owned. The mock factory should outlive |this|.
  WebURLLoaderMockFactory* mock_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_WEB_URL_LOADER_FACTORY_WITH_MOCK_H_
