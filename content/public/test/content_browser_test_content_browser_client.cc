// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_content_browser_client.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "content/public/common/content_client.h"

namespace content {

ContentBrowserTestContentBrowserClient::
    ContentBrowserTestContentBrowserClient() {
  if (GetShellContentBrowserClientInstances().size() > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(this);
  }
}

ContentBrowserTestContentBrowserClient::
    ~ContentBrowserTestContentBrowserClient() {
  // ShellContentBrowserClient is responsible for removing `this` from
  // GetShellContentBrowserClientInstances(). Only set ContentClient's
  // variable when there is at least one more
  // ContentBrowserTestContentBrowserClient. This is necessary as the
  // last instance is owned by ContentClient and this function is called
  // during ContentClient's destruction.
  const size_t client_count = GetShellContentBrowserClientInstances().size();
  if (client_count > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(
        GetShellContentBrowserClientInstances()[client_count - 2]);
  }
}

void ContentBrowserTestContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Override ShellContentBrowserClient::OnNetworkServiceCreated() not to call
  // NetworkService::ConfigureStubHostResolver(), because some tests are flaky
  // when configuring the stub host resolver.
  // TODO(crbug.com/41494161): Remove this override once the flakiness is fixed.
}

}  // namespace content
