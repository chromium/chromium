// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_THEME_MANAGER_H_
#define FUCHSIA_ENGINE_BROWSER_THEME_MANAGER_H_

#include <fuchsia/settings/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia/engine/web_engine_export.h"

class WEB_ENGINE_EXPORT ThemeManager {
 public:
  using OnSetThemeCompleteCallback = base::OnceCallback<void(bool)>;

  explicit ThemeManager(content::WebContents* web_contents);
  ~ThemeManager();

  ThemeManager(const ThemeManager&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;

  // Sets the |theme| requested by the FIDL caller.
  // If the |theme| is AUTO, then the theme provided by |display_service_|
  // will be used.
  // |on_set_complete| is run with |true| if the theme was set correctly,
  // or |false| either if |theme| is invalid, or if |display_service_| is
  // required but unavailable.
  void SetTheme(fuchsia::settings::ThemeType theme,
                OnSetThemeCompleteCallback on_set_complete);

 private:
  // Attempts to connect to the fuchsia.settings.Display service.
  // Returns true if a connection was created, or if one already exists.
  // Return false if the service is unavailable.
  bool EnsureDisplayService();

  void ApplyTheme();
  void WatchForDisplayChanges();
  void OnWatchResultReceived(fuchsia::settings::DisplaySettings settings);
  void OnDisplayServiceMissing();

  bool observed_display_service_error_ = false;
  bool did_receive_first_watch_result_ = false;
  base::Optional<fuchsia::settings::ThemeType> requested_theme_;
  base::Optional<fuchsia::settings::ThemeType> system_theme_;
  content::WebContents* web_contents_;
  fuchsia::settings::DisplayPtr display_service_;
  OnSetThemeCompleteCallback on_set_complete_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_THEME_MANAGER_H_
