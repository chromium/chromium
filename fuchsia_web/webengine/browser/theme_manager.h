// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_THEME_MANAGER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_THEME_MANAGER_H_

#include <fuchsia/settings/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <optional>

#include "base/fuchsia/process_context.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class WEB_ENGINE_EXPORT ThemeManager {
 public:
  explicit ThemeManager(content::WebContents* web_contents,
                        base::OnceClosure on_display_error);
  ~ThemeManager();

  ThemeManager(const ThemeManager&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;

  // Sets the |theme| requested by the FIDL caller, but does not apply the
  // theme. Call |WebContents::OnWebPreferencesChanged| to apply the theme to
  // web contents.
  //
  // If the |theme| is DEFAULT, then the theme provided by |display_service_|
  // will be used. |on_display_error_| is run if |display_service_| is required
  // but unavailable.
  void SetTheme(fuchsia::settings::ThemeType theme);

  // Override |blink_prefs| with theme set by |SetTheme|.
  void ApplyThemeToWebPreferences(blink::web_pref::WebPreferences* web_prefs);

 private:
  // Attempts to connect to the fuchsia.settings.Display service.
  // Returns true if a connection was created, or if one already exists.
  // Return false if the service is unavailable.
  bool EnsureDisplayService();

  void WatchForDisplayChanges();
  void OnWatchResultReceived(fuchsia::settings::DisplaySettings settings);
  void OnDisplayServiceMissing();

  bool observed_display_service_error_ = false;
  bool did_receive_first_watch_result_ = false;
  std::optional<fuchsia::settings::ThemeType> requested_theme_;
  std::optional<fuchsia::settings::ThemeType> system_theme_;
  content::WebContents* web_contents_;
  fuchsia::settings::DisplayPtr display_service_;
  base::OnceClosure on_display_error_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_THEME_MANAGER_H_
