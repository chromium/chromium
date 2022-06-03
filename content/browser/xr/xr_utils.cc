// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/xr_utils.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
XrIntegrationClient* GetXrIntegrationClient() {
  auto* client = GetContentClient();
  if (!client)
    return nullptr;

  auto* browser = client->browser();
  if (!browser)
    return nullptr;

  return browser->GetXrIntegrationClient();
}
}  // namespace content
