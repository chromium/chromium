// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// An object that stores and retrieves the widget configurations from user defaults.
public struct UserDefaultsWidgetStore {

  /// The key that the current widget configurations are stored under in the user defaults.
  private let widgetInfoKey = "com.google.chrome.ios.widgets.widget-info"

  // Use UserDefaults.standard because this information only needs to be stored and retrieved from
  // the main app.
  private let userDefaults = UserDefaults.standard

  public func storeWidgetInfo(_ info: [String]) throws {
    let encoder = JSONEncoder()
    let data = try encoder.encode(info)
    userDefaults.set(data, forKey: widgetInfoKey)
  }

  public func retrieveStoredWidgetInfo() -> Result<[String], Error> {
    guard let data = userDefaults.data(forKey: widgetInfoKey) else {
      // If there is no stored data, return an empty array.
      return .success([])
    }

    let decoder = JSONDecoder()
    return Result {
      return try decoder.decode(Array<String>.self, from: data)
    }
  }
}
