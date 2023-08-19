// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MATERIAL_MATERIAL_BRANDING_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MATERIAL_MATERIAL_BRANDING_API_H_

@class MDCSnackbarManager;
@class MDCSnackbarMessageView;

namespace ios {
namespace provider {

// Applies branding to the Snackbar `manager`.
void ApplyBrandingToSnackbarManager(MDCSnackbarManager* manager);

// Applies branding to the Snackbar `message_view`.
void ApplyBrandingToSnackbarMessageView(MDCSnackbarMessageView* message_view);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MATERIAL_MATERIAL_BRANDING_API_H_
