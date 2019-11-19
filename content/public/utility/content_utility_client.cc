// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/utility/content_utility_client.h"

namespace content {

bool ContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool ContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  return false;
}

mojo::ServiceFactory* ContentUtilityClient::GetIOThreadServiceFactory() {
  return nullptr;
}

mojo::ServiceFactory* ContentUtilityClient::GetMainThreadServiceFactory() {
  return nullptr;
}

}  // namespace content
