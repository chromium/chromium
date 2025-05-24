// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLIC_GLIC_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLIC_GLIC_API_H_

#import <UIKit/UIKit.h>

#import <string>

#import "base/memory/raw_ptr.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "services/network/public/cpp/resource_request.h"

class AuthenticationService;

namespace ios::provider {

// Creates request body data using a prompt and page context.
std::string CreateRequestBody(
    std::string prompt,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context);

// Creates resource request for loading glic.
std::unique_ptr<network::ResourceRequest> CreateResourceRequest();

// Starts the overlay experience on a given view controller.
void StartGlicOverlay(
    UIViewController* base_view_controller,
    raw_ptr<AuthenticationService> auth_service,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLIC_GLIC_API_H_
