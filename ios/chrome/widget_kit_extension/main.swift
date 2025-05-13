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

    if WidgetsForMultiprofile() {
      if #available(iOS 17.0, *) {
        return ChromeWidgetsForMIM.main()
      } else {
        return ChromeWidgets.main()
      }
    } else {
      return ChromeWidgets.main()
    }
  }

  // Checks if widgets for multiprofile feature is enabled.
  static func WidgetsForMultiprofile() -> Bool {
    guard let appGroup = AppGroupHelper.groupUserDefaults() else { return false }

    guard let extensionsPrefs = appGroup.object(forKey: "Extension.FieldTrial") as? NSDictionary
    else { return false }

    guard
      let shortcutsWidgetPrefs = extensionsPrefs.object(forKey: "WidgetsForMultiprofileKey")
        as? NSDictionary
    else { return false }
    guard
      let shortcutsWidgetEnabled = shortcutsWidgetPrefs.object(forKey: "FieldTrialValue")
        as? NSNumber
    else { return false }
    return shortcutsWidgetEnabled == 1
  }
}

@available(iOS 17, *)
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
