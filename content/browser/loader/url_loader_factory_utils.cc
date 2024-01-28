// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_utils.h"

#include "content/public/browser/browser_thread.h"

namespace content {
namespace url_loader_factory {

namespace {

Interceptor& GetMutableInterceptor() {
  static base::NoDestructor<Interceptor> s_callback;
  return *s_callback;
}

}  // namespace

const Interceptor& GetTestingInterceptor() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetMutableInterceptor();
}

void SetInterceptorForTesting(const Interceptor& interceptor) {
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
        BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(interceptor.is_null() || GetMutableInterceptor().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetMutableInterceptor() = interceptor;
}

}  // namespace url_loader_factory
}  // namespace content
