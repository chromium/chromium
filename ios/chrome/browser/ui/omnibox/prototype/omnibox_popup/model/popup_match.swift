// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers class PopupMatch: NSObject, Identifiable {
  let title: String
  let subtitle: String
  let url: URL?

  let pedal: Pedal?

  init(title: String, subtitle: String, url: URL?, pedal: Pedal?) {
    self.title = title
    self.subtitle = subtitle
    self.url = url
    self.pedal = pedal
  }

  public var id: String {
    return title
  }
}

extension PopupMatch {
  static let short = PopupMatch(
    title: "Google",
    subtitle: "google.com",
    url: URL(string: "http://www.google.com"),
    pedal: nil)
  static let long = PopupMatch(
    title: "1292459 - Overflow menu is displayed on top of NTP ...",
    subtitle: "bugs.chromium.org/p/chromium/issues/detail?id=1292459",
    url: URL(string: "https://bugs.chromium.org/p/chromium/issues/detail?id=1292459"),
    pedal: nil)
  static let pedal = PopupMatch(
    title: "Set Default browser",
    subtitle: "Make Chrome your default browser",
    url: URL(string: ""),
    pedal: Pedal(title: "Click here"))
  static let added = PopupMatch(
    title: "New Match",
    subtitle: "a new match",
    url: URL(string: ""),
    pedal: Pedal(title: "Click here"))
  static let previews = [short, long, pedal]
}
