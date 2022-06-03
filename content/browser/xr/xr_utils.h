// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_XR_UTILS_H_
#define CONTENT_BROWSER_XR_XR_UTILS_H_

namespace content {
class XrIntegrationClient;

// Simplifies the querying of ContentClient->ContentBrowserClient->
// XrIntegrationClient. May return nullptr.
XrIntegrationClient* GetXrIntegrationClient();
}  // namespace content

#endif  // CONTENT_BROWSER_XR_XR_UTILS_H_
