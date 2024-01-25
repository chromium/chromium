// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_TAB_HELPER_H_

#include <optional>
#include <string>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web {
class WebState;
}  // namespace web

enum Zoom {
  ZOOM_OUT = -1,
  ZOOM_RESET = 0,
  ZOOM_IN = 1,
};

// Adjusts font size of web page by mapping
// `UIApplication.sharedApplication.preferredContentSizeCategory` to a scaling
// percentage and setting it to "-webkit-font-size-adjust" style on <body> when
// the page is successfully loaded or system font size changes.
class FontSizeTabHelper : public web::WebFramesManager::Observer,
                          public web::WebStateObserver,
                          public web::WebStateUserData<FontSizeTabHelper> {
 public:
  FontSizeTabHelper(const FontSizeTabHelper&) = delete;
  FontSizeTabHelper& operator=(const FontSizeTabHelper&) = delete;

  ~FontSizeTabHelper() override;

  // Performs a zoom in the given direction on the WebState this is attached to.
  void UserZoom(Zoom zoom);

  // Returns whether the user can still zoom in. (I.e., They have not reached
  // the max zoom level.).
  bool CanUserZoomIn() const;
  // Returns whether the user can still zoom out. (I.e., They have not reached
  // the min zoom level.).
  bool CanUserZoomOut() const;
  // Returns whether the user can reset the zoom level. (I.e., They are not at
  // the default zoom level.)
  bool CanUserResetZoom() const;

  // Returns true if the Text Zoom process is currently active.
  bool IsTextZoomUIActive() const;

  // Marks the Text Zoom process as active or not.  This method does not
  // directly show or hide the UI.  It simply acts as a marker for whether or
  // not the UI should be displayed when possible.
  void SetTextZoomUIActive(bool active);

  // Text zoom is currently only supported on HTML pages.
  bool CurrentPageSupportsTextZoom() const;

  // Remove any stored zoom levels from `pref_service`.
  static void ClearUserZoomPrefs(PrefService* pref_service);

  static void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class web::WebStateUserData<FontSizeTabHelper>;

  explicit FontSizeTabHelper(web::WebState* web_state);

  // Sets font size in web page by scaling percentage.
  void SetPageFontSize(int size);

  // Returns the true font size in scaling percentage (e.g. 150 for
  // 150%) taking all sources into account (system level and user zoom).
  int GetFontSize() const;

  // Used in callbacks related to `notification_observer_`.
  void OnContentSizeCategoryChanged();

  // Set the zoom level correctly for a new navigation.
  void NewPageZoom();

  PrefService* GetPrefService() const;

  // Logs any events after zooming.
  void LogZoomEvent(Zoom zoom) const;

  // Returns the new multiplier after zooming in the given direction. Returns
  // nullopt if it is impossible to zoom in the given direction;
  std::optional<double> NewMultiplierAfterZoom(Zoom zoom) const;
  // Returns the current user zoom multiplier (i.e. not counting any additional
  // zoom due to the system accessibility settings).
  double GetCurrentUserZoomMultiplier() const;
  void StoreCurrentUserZoomMultiplier(double multiplier);
  std::string GetCurrentUserZoomMultiplierKey() const;
  std::string GetUserZoomMultiplierKeyUrlPart() const;
  bool IsGoogleCachedAMPPage() const;
  bool tab_helper_has_zoomed_ = false;

  // web::WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* context) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helper used to create notification observer.
  void CreateNotificationObserver();

  // web::WebFramesManager::Observer
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;
  // Whether the Text Zoom UI is active
  bool text_zoom_ui_active_ = false;
  // Holds references to NSNotification callback observer.
  id notification_observer_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<FontSizeTabHelper> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_TAB_HELPER_H_
