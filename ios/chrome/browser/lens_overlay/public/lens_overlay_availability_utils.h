// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_PUBLIC_LENS_OVERLAY_AVAILABILITY_UTILS_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_PUBLIC_LENS_OVERLAY_AVAILABILITY_UTILS_H_

#import <UIKit/UIKit.h>

enum class LensOverlayEntrypoint;
class PrefService;
class TemplateURLService;
namespace web {
class WebState;
}

// Returns whether the Lens Overlay UI is currently active/attached on
// `web_state`.
bool IsLensOverlayVisible(web::WebState* web_state);

// Returns whether the Lens Overlay entrypoint is available for the given
// context.
bool IsLensOverlayEntrypointAvailable(
    LensOverlayEntrypoint entrypoint,
    const PrefService* profile_prefs,
    TemplateURLService* template_url_service,
    web::WebState* web_state,
    UITraitCollection* trait_collection = nil);

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_PUBLIC_LENS_OVERLAY_AVAILABILITY_UTILS_H_
