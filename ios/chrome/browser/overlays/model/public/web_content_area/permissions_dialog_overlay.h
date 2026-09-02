// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_PERMISSIONS_DIALOG_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_PERMISSIONS_DIALOG_OVERLAY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response_info.h"
#import "url/gurl.h"

// Enum representing the user's decision from a permissions dialog.
enum class PermissionDialogDecision {
  // The user explicitly denied access permanently for the origin.
  kDontAllow = 0,
  // The user granted access temporarily for the current session/request only.
  kAllowThisTime = 1,
  // The user explicitly granted access permanently for the origin.
  kAlwaysAllow = 2,
};

// Configuration object for OverlayRequests for dialogs that ask for camera or
// microphone permissions.
class PermissionsDialogRequest
    : public OverlayRequestConfig<PermissionsDialogRequest> {
 public:
  ~PermissionsDialogRequest() override;

  // The text shown in the popup box title.
  NSString* message() const { return message_; }

 private:
  friend class OverlayUserData<PermissionsDialogRequest>;
  PermissionsDialogRequest(const GURL& url,
                           NSArray<NSNumber*>* requested_permissions);

  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  NSString* message_;
};

// Response type used for permissions dialogs.
class PermissionsDialogResponse
    : public OverlayResponseInfo<PermissionsDialogResponse> {
 public:
  ~PermissionsDialogResponse() override;
  // Whether the user has allowed the website to access camera or microphone.
  bool capture_allow() const {
    return decision_ == PermissionDialogDecision::kAlwaysAllow ||
           decision_ == PermissionDialogDecision::kAllowThisTime;
  }

  // The specific decision selected by the user in the dialog.
  PermissionDialogDecision decision() const { return decision_; }

 private:
  friend class OverlayUserData<PermissionsDialogResponse>;
  explicit PermissionsDialogResponse(bool capture_allow);
  explicit PermissionsDialogResponse(PermissionDialogDecision decision);
  const PermissionDialogDecision decision_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_PERMISSIONS_DIALOG_OVERLAY_H_
