// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import SwiftUI

extension View {
  /// Applies the given transform if the given condition evaluates to `true`.
  /// - Parameters:
  ///   - condition: The condition to evaluate.
  ///   - transform: The transform to apply to the source `View`.
  /// - Returns: Either the original `View` or the modified `View` if the condition is `true`.
  @ViewBuilder public func `if`<Content: View>(_ condition: Bool, transform: (Self) -> Content)
    -> some View
  {
    if condition {
      transform(self)
    } else {
      self
    }
  }

  /// Applies the given transform if the given condition is non nil.
  /// - Parameters:
  ///   - condition: The value to check for nil.
  ///   - transform: The transform to apply to the source `View` if `value` is
  ///                non-nil.
  /// - Returns: Either the original `View` or the modified `View` if the value
  ///            is `nil`.
  @ViewBuilder public func `ifLet`<Content: View, Value>(
    _ value: Value?, transform: (Self, Value) -> Content
  )
    -> some View
  {
    if let value = value {
      transform(self, value)
    } else {
      self
    }
  }
}
