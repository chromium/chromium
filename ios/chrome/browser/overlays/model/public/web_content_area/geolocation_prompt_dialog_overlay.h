// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_GEOLOCATION_PROMPT_DIALOG_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_GEOLOCATION_PROMPT_DIALOG_OVERLAY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

// Enum representing the user's decision from a geolocation prompt dialog.
enum class GeolocationPromptDialogDecision {
  // The user tapped "Cancel" to dismiss the prompt without opening settings.
  kCancel = 0,
  // The user tapped "Settings" to open iOS System Settings.
  kOpenSettings = 1,
};

// Configuration object for OverlayRequests for dialogs that show geolocation
// system settings help dialog.
class GeolocationPromptDialogRequest
    : public OverlayRequestConfig<GeolocationPromptDialogRequest> {
 public:
  ~GeolocationPromptDialogRequest() override;

  NSString* title() const { return title_; }
  NSString* message() const { return message_; }

 private:
  friend class OverlayUserData<GeolocationPromptDialogRequest>;
  GeolocationPromptDialogRequest();

  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  NSString* title_;
  NSString* message_;
};

// Response type used for geolocation prompt dialogs.
class GeolocationPromptDialogResponse
    : public OverlayResponseInfo<GeolocationPromptDialogResponse> {
 public:
  ~GeolocationPromptDialogResponse() override;

  // Whether the user has tapped Settings to open iOS System Settings.
  bool tapped_settings() const {
    return decision_ == GeolocationPromptDialogDecision::kOpenSettings;
  }

  // The specific decision selected by the user in the dialog.
  GeolocationPromptDialogDecision decision() const { return decision_; }

 private:
  friend class OverlayUserData<GeolocationPromptDialogResponse>;
  explicit GeolocationPromptDialogResponse(
      GeolocationPromptDialogDecision decision);
  const GeolocationPromptDialogDecision decision_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_GEOLOCATION_PROMPT_DIALOG_OVERLAY_H_
