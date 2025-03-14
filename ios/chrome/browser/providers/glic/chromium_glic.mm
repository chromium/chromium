// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/glic/glic_api.h"

namespace ios {
namespace provider {

std::string CreateRequestBody(
    std::string prompt,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  return std::string();
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
  return nullptr;
}

}  // namespace provider
}  // namespace ios
