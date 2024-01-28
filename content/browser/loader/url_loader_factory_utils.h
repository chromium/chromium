// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_

#include "content/common/content_export.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"

namespace content::url_loader_factory {

using Interceptor = base::RepeatingCallback<
    void(int process_id, network::URLLoaderFactoryBuilder& factory_builder)>;

// This method must be called on the UI thread.
CONTENT_EXPORT const Interceptor& GetTestingInterceptor();

// Allows intercepting the URLLoaderFactory creation.
// For every `SetInterceptorForTesting(non-null interceptor)`,
// `SetInterceptorForTesting({})` must be called to ensure restoring the default
// behavior.
// This method must be called either on the UI thread or before threads start.
// This callback is run on the UI thread.
// TODO(crbug.com/1506871): Document when the interception occurs.
CONTENT_EXPORT void SetInterceptorForTesting(const Interceptor& interceptor);

}  // namespace content::url_loader_factory

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_
