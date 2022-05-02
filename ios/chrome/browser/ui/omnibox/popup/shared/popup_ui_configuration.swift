// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// An extension of `@Published` that only publishes a new value when there
/// is an actual change.
/// TODO(crbug.com/1315110): See if this should be moved to a shared location.
@propertyWrapper class PublishedOnChange<Value: Equatable> {
  @Published private var value: Value

  public var projectedValue: Published<Value>.Publisher {
    get {
      return $value
    }
    set {
      $value = newValue
    }
  }
  var wrappedValue: Value {
    get {
      return value
    }
    set {
      guard newValue != value else {
        return
      }
      value = newValue
    }
  }
  init(wrappedValue value: Value) {
    self.value = value
  }

  public init(initialValue value: Value) {
    self.value = value
  }
}

/// A model object holding UI data necessary to display the popup menu
@objcMembers public class PopupUIConfiguration: NSObject, ObservableObject {
  /// Holds how much leading space there is from the edge of the popup view to
  /// where the omnibox starts.
  @PublishedOnChange var omniboxLeadingSpace: CGFloat = 0

  /// Holds how much trailing space there is from where the omnibox ends to
  /// the edge of the popup view.
  @PublishedOnChange var omniboxTrailingSpace: CGFloat = 0

  /// The current toolbar configuration, used to color items that should match
  /// the toolbar's color.
  let toolbarConfiguration: ToolbarConfiguration

  public init(toolbarConfiguration: ToolbarConfiguration) {
    self.toolbarConfiguration = toolbarConfiguration
  }
}
