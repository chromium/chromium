// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_

#include <memory>
#include <utility>
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/testing/code_cache_loader_mock.h"

namespace blink {

// ResourceFetcher::LoaderFactory implementation for tests.
class TestLoaderFactory : public ResourceFetcher::LoaderFactory {
 public:
  TestLoaderFactory()
      : TestLoaderFactory(WebURLLoaderMockFactory::GetSingletonInstance()) {}

  explicit TestLoaderFactory(WebURLLoaderMockFactory* mock_factory)
      : mock_factory_(mock_factory) {}

  // LoaderFactory implementations
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper)
      override {
    return mock_factory_->CreateURLLoader();
  }

  std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override {
    return std::make_unique<CodeCacheLoaderMock>();
  }

 private:
  WebURLLoaderMockFactory* mock_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_
