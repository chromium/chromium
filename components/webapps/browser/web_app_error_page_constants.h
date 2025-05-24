// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_WEB_APP_ERROR_PAGE_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_BROWSER_WEB_APP_ERROR_PAGE_CONSTANTS_H_

namespace web_app::error_page {

// |alternative_error_page_params| dictionary key values in the
// |AlternativeErrorPageOverrideInfo| mojom struct.
inline constexpr char kMessage[] = "web_app_error_page_message";
inline constexpr char kAppShortName[] = "app_short_name";
inline constexpr char kIconUrl[] = "icon_url";
inline constexpr char kSupplementaryIcon[] = "supplementary_icon";

// This must match the HTML element id of the svg to show as a supplementary
// icon on the default offline error page.
inline constexpr char16_t kOfflineIconId[] = u"offlineIcon";

}  // namespace web_app::error_page

#endif  // COMPONENTS_WEBAPPS_BROWSER_WEB_APP_ERROR_PAGE_CONSTANTS_H_
