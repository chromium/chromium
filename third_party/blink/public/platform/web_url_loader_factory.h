// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"

namespace blink {

class WebURLLoader;
class WebURLRequest;

// An abstract interface to create a URLLoader. It is expected that each
// loading context holds its own per-context WebURLLoaderFactory.
class WebURLLoaderFactory {
 public:
  virtual ~WebURLLoaderFactory() = default;

  // Returns a new WebURLLoader instance. This should internally choose
  // the most appropriate URLLoaderFactory implementation.
  // TODO(yuzus): Only take unfreezable task runner once both
  // URLLoaderClientImpl and ResponseBodyLoader use unfreezable task runner.
  // This currently takes two task runners: freezable and unfreezable one.
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest& webreq,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          freezable_task_runner,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          unfreezable_task_runner) = 0;
};

// A test version of the above factory interface, which supports cloning the
// factory.
class WebURLLoaderFactoryForTest : public WebURLLoaderFactory {
 public:
  // Clones this factory.
  virtual std::unique_ptr<WebURLLoaderFactoryForTest> Clone() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_FACTORY_H_
