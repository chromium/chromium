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
@class GeminiConfiguration;
@class GeminiPageContext;
@class GeminiSettingsAction;
@class GeminiSettingsMetadata;
@class GeminiStartupConfiguration;
@protocol BWGGatewayProtocol;

typedef NS_ENUM(NSInteger, GeminiSettingsContext);

using BWGEligibilityCallback = void (^)(BOOL eligible);

namespace ios::provider {

// Enum representing the location permission state of the Gemini experience.
// A full permission grant is gated by first the OS level (for Chrome) location
// permission and then the user level Gemini-specific location permission.
// This needs to stay in sync with GCRGeminiLocationPermissionState (and its SDK
// counterpart).
enum class GeminiLocationPermissionState {
  // Default state.
  kUnknown,
  // The location permission is fully granted.
  kFullyGranted,
  // The location permission is granted only at the OS level.
  kBWGDisabled,
  // The location permission is disabled at both the OS level and Gemini level.
  kBWGAndOSDisabled,
  // The location permission is disable by an Enterprise policy.
  kEnterpriseDisabled,
};

// Enum representing a page context computation state for the Gemini experience.
// This needs to stay in sync with GCRGeminiPageContextComputationState (and its
// SDK counterpart).
enum class GeminiPageContextComputationState {
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

// Enum representing the page context attachment state of the Gemini experience.
// This needs to stay in sync with GCRGeminiPageContextAttachmentState (and its
// SDK counterpart).
enum class GeminiPageContextAttachmentState {
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

// Enum representing the Gemini view state.
// This needs to stay in sync with GCRGeminiViewState (and its SDK counterpart).
// LINT.IfChange(GeminiViewState)
enum class GeminiViewState {
  // The Gemini view state is unknown.
  kUnknown,
  // The Gemini view is hidden. When the floaty is set to `kHidden`, the floaty
  // is destructed and properties are stored in the view manager in the Gemini
  // SDK. After this, setting the `GeminiViewState` to another state
  // will reinitialize the floaty with stored properties from when the floaty
  // was initially hidden.
  kHidden,
  // The Gemini view is collapsed (minimized) into a circle.
  kCollapsed,
  // The Gemini view is expanded.
  kExpanded,
  kMaxValue = kExpanded,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:GeminiViewState)

// Enum representing the UI element type for which a change is requested.
// This needs to stay in sync with GCRGeminiUIElementType (and its SDK
// counterpart).
enum class GeminiUIElementType {
  // The element type is unknown.
  kUnknown,
  // The context attachment element.
  kContextAttachment,
  // The zero state element.
  kZeroState,
};

// Configures Gemini with the given startup configuration.
void ConfigureWithStartupConfiguration(
    GeminiStartupConfiguration* startup_configuration);

// Starts the overlay experience with the given configuration.
void StartBwgOverlay(GeminiConfiguration* gemini_configuration);

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
    GeminiPageContextAttachmentState gemini_attachment_state);

// Returns true if a URL is protected.
bool IsProtectedUrl(std::string url);

// Updates the page context of the floaty.
void UpdatePageContext(GeminiPageContext* gemini_page_context);

// Returns the Gemini settings that the user is eligible for.
NSArray<GeminiSettingsMetadata*>* GetEligibleSettings(
    AuthenticationService* auth_service);

// Returns the settings action for a given settings context.
GeminiSettingsAction* ActionForSettingsContext(GeminiSettingsContext context);

// Updates Gemini overlay offset with a specific `opacity`. A positive `offset`
// will move the overlay towards the top of the viewport while a negative
// `offset` will move the overlay towards the bottom and even below the
// viewport.
void UpdateOverlayOffsetWithOpacity(CGFloat offset, CGFloat opacity);

// TODO(crbug.com/475205334): Remove this method after function below is
// implemented.
// Updates Gemini floaty view state.
void UpdateGeminiViewState(GeminiViewState view_state);

// Updates Gemini floaty view state with an animation flag.
void UpdateGeminiViewState(GeminiViewState view_state, bool animated);

// Returns the current `GeminiViewState` of the floaty.
GeminiViewState GetCurrentGeminiViewState();

// Requests a UI change for a specific element type.
void RequestUIChange(GeminiUIElementType ui_element_type);

// Attaches an image to the Gemini floaty.
void AttachImage(UIImage* image);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_API_H_
