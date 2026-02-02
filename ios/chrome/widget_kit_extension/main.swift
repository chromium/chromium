// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

@main
struct ChromeWidgetsMain {

  static func main() {
    CrashHelper.configure()

    return ChromeWidgetsForMIM.main()
  }
}

struct ChromeWidgetsForMIM: WidgetBundle {
  @WidgetBundleBuilder
  var body: some Widget {
    QuickActionsWidgetConfigurable()
    SearchWidgetConfigurable()
    ShortcutsWidgetConfigurable()
    SearchPasswordsWidgetConfigurable()
    DinoGameWidgetConfigurable()
    #if IOS_ENABLE_LOCKSCREEN_WIDGET
      #if IOS_AVAILABLE_LOCKSCREEN_WIDGET
        LockscreenLauncherSearchWidget()
        LockscreenLauncherIncognitoWidget()
        LockscreenLauncherVoiceSearchWidget()
        LockscreenLauncherGameWidget()
      #endif
    #endif
  }
}
