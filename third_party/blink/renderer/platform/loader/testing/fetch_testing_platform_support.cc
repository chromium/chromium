// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/fetch_testing_platform_support.h"

#include <memory>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory_impl.h"

namespace blink {

FetchTestingPlatformSupport::FetchTestingPlatformSupport()
    : url_loader_mock_factory_(new URLLoaderMockFactoryImpl(this)) {}

FetchTestingPlatformSupport::~FetchTestingPlatformSupport() {
  // Shutdowns URLLoaderMockFactory gracefully, serving all pending requests
  // first, then flushing all registered URLs.
  url_loader_mock_factory_->ServeAsynchronousRequests();
  url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
}

URLLoaderMockFactory* FetchTestingPlatformSupport::GetURLLoaderMockFactory() {
  return url_loader_mock_factory_.get();
}

}  // namespace blink
