// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_

#import <UIKit/UIKit.h>

#import <string>

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "services/network/public/cpp/resource_request.h"

class AuthenticationService;
@class BWGConfiguration;
@class GeminiPageContext;
@protocol BWGGatewayProtocol;

using BWGEligibilityCallback = void (^)(BOOL eligible);

namespace ios::provider {

// Enum representing the location permission state of the BWG experience. A full
// permission grant is gated by first the OS level (for Chrome) location
// permission and then the user level BWG-specific location permission.
// This needs to stay in sync with GCRGeminiLocationPermissionState (and its SDK
// counterpart).
enum class BWGLocationPermissionState {
  // Default state.
  kUnknown,
  // The location permission is fully granted.
  kFullyGranted,
  // The location permission is granted only at the OS level.
  kBWGDisabled,
  // The location permission is disabled at both the OS level and BWG level.
  kBWGAndOSDisabled,
  // The location permission is disable by an Enterprise policy.
  kEnterpriseDisabled,
};

// Enum representing the page context computation state of the BWG experience.
// This needs to stay in sync with GCRGeminiPageContextComputationState (and its
// SDK counterpart).
enum class BWGPageContextComputationState {
  // The state of the page context is unknown; this likely means that it was not
  // set.
  kUnknown,
  // The page context was successfully created.
  kSuccess,
  // The page context should have been included, but was not gathered
  // successfully.
  kError,
  // The page contains protected content which should not be used for Gemini,
  // and should not be sent to any server or stored.
  kProtected,
  // The page contains blocked content that could be used for Gemini, but will
  // likely be rejected due to its content.
  kBlocked,
  // The page context is still being created.
  kPending,
};

// Enum representing the page context attachment state of the BWG experience.
// This needs to stay in sync with GCRGeminiPageContextAttachmentState (and its
// SDK counterpart).
enum class BWGPageContextAttachmentState {
  // The attach state is unknown.
  kUnknown,
  // Page context should be attached.
  kAttached,
  // Page context should be detached.
  kDetached,
  // Page context attachment is disabled by the user.
  kUserDisabled,
  // Page context attachment is disabled by an enterprise policy.
  kEnterpriseDisabled,
};

// Starts the overlay experience with the given configuration.
void StartBwgOverlay(BWGConfiguration* bwg_configuration);

// Gets the portion of the PageContext script that checks whether PageContext
// should be detached from the request.
const std::u16string GetPageContextShouldDetachScript();

// Creates a BWG gateway object for relaying internal protocols.
id<BWGGatewayProtocol> CreateBWGGateway();

// Checks if the feature is disabled through a Gemini Enterprise policy, and
// returns the result through a `completion` block.
void CheckGeminiEligibility(AuthenticationService* auth_service,
                            BWGEligibilityCallback completion);

// Resets the Gemini instance by clearing its state.
void ResetGemini();

// Updates the page attachment state of the floaty if it's invoked.
void UpdatePageAttachmentState(
    BWGPageContextAttachmentState bwg_attachment_state);

// Returns true if a URL is protected.
bool IsProtectedUrl(std::string url);

// Updates the page context of the floaty.
void UpdatePageContext(GeminiPageContext* gemini_page_context);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_
