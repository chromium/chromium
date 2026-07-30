// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

// Constants for WidgetKit extension URLs.
// The underlying scheme, host, and action strings are defined in
// ios/chrome/common/app_group/widget_constants.h and bridged via
// widget_kit_swift_bridge.h so Swift code can re-use them.
struct WidgetConstants {
  struct SearchWidget {
    static let url = URL(
      string: "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostSearchWidget)\(kWidgetKitActionSearch)"
    )!
  }
  struct QuickActionsWidget {
    static let searchUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostQuickActionsWidget)\(kWidgetKitActionSearch)"
    )!
    static let incognitoUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostQuickActionsWidget)\(kWidgetKitActionIncognito)"
    )!
    static let voiceSearchUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostQuickActionsWidget)\(kWidgetKitActionVoiceSearch)"
    )!
    static let qrCodeUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostQuickActionsWidget)\(kWidgetKitActionQRReader)"
    )!
    static let lensUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostQuickActionsWidget)\(kWidgetKitActionLens)"
    )!
    static let isGoogleDefaultSearchEngineKey = "isGoogleDefaultSearchEngine"
    static let enableLensInWidgetKey = "enableLensInWidget"
  }
  struct DinoGameWidget {
    static let url = URL(
      string: "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostDinoGameWidget)\(kWidgetKitActionGame)"
    )!
  }
  struct LockscreenLauncherWidget {
    static let searchUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostLockscreenLauncherWidget)\(kWidgetKitActionSearch)"
    )!
    static let incognitoUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostLockscreenLauncherWidget)\(kWidgetKitActionIncognito)"
    )!
    static let voiceSearchUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostLockscreenLauncherWidget)\(kWidgetKitActionVoiceSearch)"
    )!
    static let gameUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostLockscreenLauncherWidget)\(kWidgetKitActionGame)"
    )!
  }
  struct ShortcutsWidget {
    static let searchUrl = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostShortcutsWidget)\(kWidgetKitActionSearch)"
    )!
    static let open = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostShortcutsWidget)\(kWidgetKitActionOpenURL)"
    )!
  }
  struct SearchPasswordsWidget {
    static let url = URL(
      string:
        "\(kWidgetKitSchemeChrome)://\(kWidgetKitHostSearchPasswordsWidget)\(kWidgetKitActionSearchPasswords)"
    )!
  }
}

// Returns the destination URL appending the gaiaID if available.
func destinationURL(url: URL, gaia: String? = nil) -> URL {
  if gaia == nil {
    return url
  }
  guard var components = URLComponents(url: url, resolvingAgainstBaseURL: true) else {
    return url
  }
  components.queryItems = [URLQueryItem(name: "gaia_id", value: gaia)]

  return components.url ?? url
}
