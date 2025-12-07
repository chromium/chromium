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

    if #available(iOS 17.0, *) {
      return ChromeWidgetsForMIM.main()
    } else {
      return ChromeWidgets.main()
    }
  }
}

struct ChromeWidgetsForMIM: WidgetBundle {
  @WidgetBundleBuilder
  var body: some Widget {
    #if IOS_ENABLE_WIDGETS_FOR_MIM
      QuickActionsWidgetConfigurable()
      SearchWidgetConfigurable()
      ShortcutsWidgetConfigurable()
      SearchPasswordsWidgetConfigurable()
      DinoGameWidgetConfigurable()
    #else
      QuickActionsWidget()
      SearchWidget()
      ShortcutsWidget()
      SearchPasswordsWidget()
      DinoGameWidget()
    #endif
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

struct ChromeWidgets: WidgetBundle {
  @WidgetBundleBuilder
  var body: some Widget {
    QuickActionsWidget()
    SearchWidget()
    ShortcutsWidget()
    SearchPasswordsWidget()
    DinoGameWidget()
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
