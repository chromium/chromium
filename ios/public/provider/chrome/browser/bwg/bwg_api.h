// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_

#import <UIKit/UIKit.h>

#import <string>

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "services/network/public/cpp/resource_request.h"

@class BWGConfiguration;

namespace ios::provider {

// Enum representing the PageContext state of the BWG experience. This needs to
// stay in sync with GCRGeminiPageState.
enum class BWGPageContextState {
  // Default state.
  kUnknown,
  // PageContext was successfully attached.
  kSuccessfullyAttached,
  // PageContext should be detached.
  kShouldDetach,
  // PageContext is protected.
  kProtected,
  // PageContext is present but likely to be blocked.
  kBlocked,
  // There was an error extracting the PageContext.
  kError,
};

// Creates request body data using a prompt and page context.
std::string CreateRequestBody(
    std::string prompt,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context);

// Creates resource request for loading glic.
std::unique_ptr<network::ResourceRequest> CreateResourceRequest();

// Starts the overlay experience with the given configuration.
void StartBwgOverlay(BWGConfiguration* bwg_configuration);

// Gets the portion of the PageContext script that checks whether PageContext
// should be detached from the request.
const std::u16string GetPageContextShouldDetachScript();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_
