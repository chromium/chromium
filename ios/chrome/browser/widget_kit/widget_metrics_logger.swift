// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import WidgetKit

/// Logs metrics associated with iOS 14 home screen widgets.
public final class WidgetsMetricLogger: NSObject {

  /// The queue onto which time-consuming work is dispatched.
  private static let queue = DispatchQueue(label: "com.google.chrome.ios.WidgetMetricLogger")

  // MARK: Public

  /// A callback to be called when a widget install is detected. This callback is passed the
  /// information about which widget was installed.
  ///
  /// This property must be set before the `logInstalledWidgets` method is called.
  @objc public static var widgetInstalledCallback: ((String) -> Void)? = nil

  /// A callback to be called when a widget uninstall is detected. This callback is passed the
  /// kind about which widget was uninstalled.
  ///
  /// This property must be set before the `logInstalledWidgets` method is called.
  @objc public static var widgetUninstalledCallback: ((String) -> Void)? = nil

  /// A callback to be called when a widget in use is detected. This callback is passed the
  /// kind about which widget is in use.
  ///
  /// This property must be set before the `logInstalledWidgets` method is called.
  @objc public static var widgetCurrentCallback: ((String) -> Void)? = nil

  /// Logs metrics if the user has installed or uninstalled a widget since the last check.
  ///
  /// This method should be called once per application foreground, for example in the
  /// `applicationWillEnterForeground` method.
  ///
  /// This method is safe to call from any thread.
  @objc(logInstalledWidgets)
  public static func logInstalledWidgets() {
    if #available(iOS 14, *) {
      // To avoid blocking startup, perform work on a background queue.
      queue.async {
        logInstalledWidgets(fetcher: WidgetCenter.shared, store: UserDefaultsWidgetStore())
      }
    }
  }

  // MARK: Private

  /// Logs metrics if the user has installed or uninstalled a widget since the last app launch.
  static func logInstalledWidgets(fetcher: WidgetCenter, store: UserDefaultsWidgetStore) {
    fetcher.getCurrentConfigurations { result in
      // If fetching either current or previous info fails, avoid logging anything. The next time
      // this is called, metrics will be logged.
      guard let currentWidgets = try? result.get().map({ $0.kind }) else {
        return
      }

      // Log current widgets.
      for widget in currentWidgets {
        widgetCurrentCallback?(widget)
      }

      guard let storedWidgets = try? store.retrieveStoredWidgetInfo().get() else {
        return
      }

      // Attempt to store the new configurations and verify that it is successful to avoid double
      // logging. If metrics were logged when storage failed, they would be double-logged the next
      // time this method is called.
      do {
        try store.storeWidgetInfo(currentWidgets)
      } catch {
        return
      }

      // Current widgets minus stored widgets are installations.
      var installedWidgets = currentWidgets
      for storedWidget in storedWidgets {
        if let index = installedWidgets.firstIndex(of: storedWidget) {
          installedWidgets.remove(at: index)
        }
      }
      for installedWidget in installedWidgets {
        widgetInstalledCallback?(installedWidget)
      }

      // Stored widgets minus current widgets are uninstallations.
      var uninstalledWidgets = storedWidgets
      for currentWidget in currentWidgets {
        if let index = uninstalledWidgets.firstIndex(of: currentWidget) {
          uninstalledWidgets.remove(at: index)
        }
      }
      for uninstalledWidget in uninstalledWidgets {
        widgetUninstalledCallback?(uninstalledWidget)
      }
    }
  }
}
