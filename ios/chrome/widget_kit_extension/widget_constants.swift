// Copyright 2020 The Chromium Authors. All rights reserved.
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
  }
  struct DinoGameWidget {
    static let url = URL(string: "chromewidgetkit://dino-game-widget/game")!
  }
}
