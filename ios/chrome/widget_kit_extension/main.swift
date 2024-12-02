// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

@main
struct ChromeWidgets: WidgetBundle {
  init() {
    CrashHelper.configure()
  }

  var body: some Widget {
    if #available(iOS 17.0, *) {
      return body17
    } else {
      return body16
    }
  }

  @available(iOS 17, *)
  @WidgetBundleBuilder
  var body17: some Widget {
    QuickActionsWidget()
    #if IOS_ENABLE_WIDGETS_FOR_MIM
      SearchWidgetConfigurable()
    #else
      SearchWidget()
    #endif
    #if IOS_ENABLE_SHORTCUTS_WIDGET
      ShortcutsWidget()
    #endif
    SearchPasswordsWidget()
    #if IOS_ENABLE_WIDGETS_FOR_MIM
      DinoGameWidgetConfigurable()
    #else
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

  @WidgetBundleBuilder
  var body16: some Widget {
    QuickActionsWidget()
    SearchWidget()
    #if IOS_ENABLE_SHORTCUTS_WIDGET
      ShortcutsWidget()
    #endif
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
