// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_SCHEMES_H_
#define CONTENT_COMMON_URL_SCHEMES_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"

namespace content {

// Called near the beginning of startup to register URL schemes that should be
// parsed as "standard" or "referrer" with the src/url/ library, then locks the
// sets of schemes down. The embedder can add additional schemes by
// overriding the ContentClient::AddAdditionalSchemes method.
CONTENT_EXPORT void RegisterContentSchemes();

// Re-initializes schemes for tests.
CONTENT_EXPORT void ReRegisterContentSchemesForTests();

// See comment in ContentClient::AddAdditionalSchemes for explanations. These
// getters can be invoked on any thread.
const std::vector<std::string>& GetSavableSchemes();
const std::vector<std::string>& GetServiceWorkerSchemes();

}  // namespace content

#endif  // CONTENT_COMMON_URL_SCHEMES_H_
