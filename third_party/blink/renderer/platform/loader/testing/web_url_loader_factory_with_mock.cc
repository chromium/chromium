// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/web_url_loader_factory_with_mock.h"

#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"

namespace blink {

WebURLLoaderFactoryWithMock::WebURLLoaderFactoryWithMock(
    WebURLLoaderMockFactory* mock_factory)
    : mock_factory_(mock_factory) {}

WebURLLoaderFactoryWithMock::~WebURLLoaderFactoryWithMock() = default;

std::unique_ptr<WebURLLoader> WebURLLoaderFactoryWithMock::CreateURLLoader(
    const WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>) {
  return mock_factory_->CreateURLLoader();
}

}  // namespace blink
