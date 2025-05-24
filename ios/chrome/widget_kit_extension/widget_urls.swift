// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

struct WidgetConstants {
  struct SearchWidget {
    static let url = URL(string: "chromewidgetkit://search-widget/search")!
  }
  struct QuickActionsWidget {
    static let searchUrl =
      URL(string: "chromewidgetkit://quick-actions-widget/search")!
    static let incognitoUrl =
      URL(string: "chromewidgetkit://quick-actions-widget/incognito")!
    static let voiceSearchUrl =
      URL(string: "chromewidgetkit://quick-actions-widget/voicesearch")!
    static let qrCodeUrl =
      URL(string: "chromewidgetkit://quick-actions-widget/qrreader")!
    static let lensUrl =
      URL(string: "chromewidgetkit://quick-actions-widget/lens")!
    static let isGoogleDefaultSearchEngineKey = "isGoogleDefaultSearchEngine"
    static let enableLensInWidgetKey = "enableLensInWidget"
  }
  struct DinoGameWidget {
    static let url = URL(string: "chromewidgetkit://dino-game-widget/game")!
  }
  struct LockscreenLauncherWidget {
    static let searchUrl =
      URL(string: "chromewidgetkit://lockscreen-launcher-widget/search")!
    static let incognitoUrl =
      URL(string: "chromewidgetkit://lockscreen-launcher-widget/incognito")!
    static let voiceSearchUrl =
      URL(string: "chromewidgetkit://lockscreen-launcher-widget/voicesearch")!
    static let gameUrl = URL(string: "chromewidgetkit://lockscreen-launcher-widget/game")!
  }
  struct ShortcutsWidget {
    static let searchUrl =
      URL(string: "chromewidgetkit://shortcuts-widget/search")!
    static let open =
      URL(string: "chromewidgetkit://shortcuts-widget/open")!
  }
  struct SearchPasswordsWidget {
    static let url = URL(string: "chromewidgetkit://search-passwords-widget/search-passwords")!
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
