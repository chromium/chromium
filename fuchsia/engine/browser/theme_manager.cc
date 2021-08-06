// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/theme_manager.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

namespace {

using blink::mojom::PreferredColorScheme;
using fuchsia::settings::ThemeType;

constexpr PreferredColorScheme kFallbackColorScheme =
    PreferredColorScheme::kLight;

PreferredColorScheme ThemeTypeToBlinkScheme(ThemeType type) {
  switch (type) {
    case ThemeType::LIGHT:
      return PreferredColorScheme::kLight;
    case ThemeType::DARK:
      return PreferredColorScheme::kDark;
    default:
      NOTREACHED();
      return kFallbackColorScheme;
  }
}

}  // namespace

ThemeManager::ThemeManager(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);

  // Per the FIDL API, the default theme is LIGHT.
  SetTheme(ThemeType::LIGHT, base::DoNothing());
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::SetTheme(
    ThemeType theme,
    ThemeManager::OnSetThemeCompleteCallback on_set_complete) {
  requested_theme_ = theme;

  on_set_complete_ = std::move(on_set_complete);

  if (theme == ThemeType::DEFAULT) {
    if (!EnsureDisplayService()) {
      OnDisplayServiceMissing();
      return;
    }
  }

  ApplyTheme();
}

bool ThemeManager::EnsureDisplayService() {
  if (observed_display_service_error_)
    return false;

  if (display_service_)
    return true;

  display_service_ = base::ComponentContextForProcess()
                         ->svc()
                         ->Connect<fuchsia::settings::Display>();

  display_service_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << "fuchsia.settings.Display disconnected.";

    observed_display_service_error_ = true;

    // If the channel to the Display service was dropped before we received a
    // response from WatchForDisplayChanges, then it's likely that the service
    // isn't available in the namespace at all, which should be reported as
    // an error on the Frame.
    // Otherwise, if a failure was detected for a Display that was previously
    // functioning, it should be treated as a transient issue and the last known
    // system theme should be used.
    if (requested_theme_ && (*requested_theme_ == ThemeType::DEFAULT) &&
        !did_receive_first_watch_result_) {
      OnDisplayServiceMissing();
    }
  });

  WatchForDisplayChanges();
  return true;
}

void ThemeManager::OnDisplayServiceMissing() {
  LOG(ERROR) << "DEFAULT theme requires access to the "
                "`fuchsia.settings.Display` service to work.";

  if (on_set_complete_)
    std::move(on_set_complete_).Run(false);
}

void ThemeManager::ApplyTheme() {
  DCHECK(requested_theme_);

  blink::web_pref::WebPreferences web_preferences =
      web_contents_->GetOrCreateWebPreferences();

  if (requested_theme_ == ThemeType::DEFAULT) {
    if (!system_theme_) {
      // Defer theme application until we receive a system theme.
      return;
    }

    web_preferences.preferred_color_scheme =
        ThemeTypeToBlinkScheme(*system_theme_);
  } else {
    DCHECK(requested_theme_ == ThemeType::LIGHT ||
           requested_theme_ == ThemeType::DARK);

    web_preferences.preferred_color_scheme =
        ThemeTypeToBlinkScheme(*requested_theme_);
  }

  web_contents_->SetWebPreferences(web_preferences);

  if (on_set_complete_)
    std::move(on_set_complete_).Run(true);
}

void ThemeManager::WatchForDisplayChanges() {
  DCHECK(display_service_);

  // Will reply immediately for the first call of Watch(). Subsequent calls to
  // Watch() will be replied to as changes occur.
  display_service_->Watch(
      fit::bind_member(this, &ThemeManager::OnWatchResultReceived));
}

void ThemeManager::OnWatchResultReceived(
    fuchsia::settings::DisplaySettings settings) {
  did_receive_first_watch_result_ = true;

  if (settings.has_theme() && settings.theme().has_theme_type() &&
      (settings.theme().theme_type() == ThemeType::DARK ||
       settings.theme().theme_type() == ThemeType::LIGHT)) {
    system_theme_ = settings.theme().theme_type();
  } else {
    system_theme_ = absl::nullopt;
  }

  ApplyTheme();
  WatchForDisplayChanges();
}
