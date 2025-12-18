// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Collection of IDs for matching views that animate when the menu enters
/// customization state.
enum MenuCustomizationAnimationID {
  static let destinations = "destinations"
  static let actions = "actions"
  static func from(_ destination: OverflowMenuDestination) -> String {
    return "destination-\(destination.destination)"
  }
}
