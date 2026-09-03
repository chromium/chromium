// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/public/lens_overlay_availability_utils.h"

#import "base/notimplemented.h"
#import "components/google/core/common/google_util.h"
#import "components/lens/lens_url_utils.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"

bool IsLensOverlayVisible(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }
  LensOverlayTabHelper* lens_overlay_tab_helper =
      LensOverlayTabHelper::FromWebState(web_state);
  return lens_overlay_tab_helper &&
         lens_overlay_tab_helper->IsLensOverlayUIAttachedAndAlive();
}

bool IsLensOverlayEntrypointAvailable(LensOverlayEntrypoint entrypoint,
                                      const PrefService* profile_prefs,
                                      TemplateURLService* template_url_service,
                                      web::WebState* web_state,
                                      UITraitCollection* trait_collection) {
  if (!web_state) {
    return false;
  }

  if (!profile_prefs || !IsLensOverlayAllowedByPolicy(profile_prefs)) {
    return false;
  }

  if (!search_engines::SupportsSearchImageWithLens(template_url_service)) {
    return false;
  }

  bool is_incognito = false;
  if (web_state->GetBrowserState()) {
    is_incognito = web_state->GetBrowserState()->IsOffTheRecord();
  }
  GURL visible_url = web_state->GetVisibleURL();

  switch (entrypoint) {
    case LensOverlayEntrypoint::kLocationBar: {
      if (is_incognito) {
        return false;
      }
      if (google_util::IsGoogleSearchUrl(visible_url) ||
          google_util::IsGoogleHomePageUrl(visible_url)) {
        return false;
      }
      return !IsVisibleURLNewTabPage(web_state) &&
             !lens::IsLensMWebResult(visible_url);
    }
    case LensOverlayEntrypoint::kOverflowMenu: {
      if (IsOverflowMenuNTPRefactorEnabled() &&
          IsVisibleURLNewTabPage(web_state)) {
        return false;
      }
      bool is_portrait =
          trait_collection ? !IsCompactHeight(trait_collection) : true;
      bool portrait_override =
          IsLensOverlayLandscapeOrientationEnabled(profile_prefs);
      return is_portrait || portrait_override;
    }
    case LensOverlayEntrypoint::kAppBar:
    case LensOverlayEntrypoint::kLevelUp:
      return true;
    case LensOverlayEntrypoint::kSearchImageContextMenu:
    case LensOverlayEntrypoint::kLVFCameraCapture:
    case LensOverlayEntrypoint::kLVFImagePicker:
    case LensOverlayEntrypoint::kAIHub:
    case LensOverlayEntrypoint::kFREPromo:
      NOTIMPLEMENTED();
      return false;
  }
}
