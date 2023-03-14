// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import ObjectiveC
import UIKit

/// Extends UIView to notify via KVO when it moved to a new window.
extension UIView {
  /// MARK: Public

  /// Whether all instances of UIView will notify key-value observers for their `window` property.
  ///
  /// Default is `false`.
  @objc public static var cr_supportsWindowObserving: Bool {
    get {
      return swizzled
    }
    set {
      if swizzled != newValue {
        // Swap implementations.
        guard
          let willMove1 = class_getInstanceMethod(UIView.self, #selector(willMove(toWindow:))),
          let willMove2 = class_getInstanceMethod(UIView.self, #selector(cr_willMove(toWindow:))),
          let didMove1 = class_getInstanceMethod(UIView.self, #selector(didMoveToWindow)),
          let didMove2 = class_getInstanceMethod(UIView.self, #selector(cr_didMoveToWindow)),
          // UITextField's original implementations don't call the super class implementations,
          // which breaks the interposition at the UIView level. Handle UITextField explicitly.
          let textFieldWillMove1 = class_getInstanceMethod(
            UITextField.self, #selector(willMove(toWindow:))),
          let textFieldWillMove2 = class_getInstanceMethod(
            UITextField.self, #selector(cr_willMove(toWindow:))),
          let textFieldDidMove1 = class_getInstanceMethod(
            UITextField.self, #selector(didMoveToWindow)),
          let textFieldDidMove2 = class_getInstanceMethod(
            UITextField.self, #selector(cr_didMoveToWindow))
        else {
          // If it failed here, don't change the `swizzled` state.
          return
        }
        method_exchangeImplementations(willMove1, willMove2)
        method_exchangeImplementations(didMove1, didMove2)
        method_exchangeImplementations(textFieldWillMove1, textFieldWillMove2)
        method_exchangeImplementations(textFieldDidMove1, textFieldDidMove2)
        // Change the `swizzled` state.
        swizzled = newValue
      }
    }
  }

  /// Signals that the `window` key is supported via Manual Change Notification.
  /// https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/KeyValueObserving/Articles/KVOCompliance.html#//apple_ref/doc/uid/20002178-SW3
  @objc class func automaticallyNotifiesObserversOfWindow() -> Bool {
    return false
  }

  /// MARK: Private

  /// Whether the original and alternative implementations have been swapped.
  private static var swizzled = false
}

extension UIView {
  /// Adds a call to the KVO `willChangeValue(forKey:)` method.
  @objc fileprivate func cr_willMove(toWindow newWindow: UIWindow?) {
    cr_willMove(toWindow: newWindow)
    willChangeValue(forKey: "window")
  }

  /// Adds a call to the KVO `didChangeValue(forKey:)` method.
  @objc fileprivate func cr_didMoveToWindow() {
    cr_didMoveToWindow()
    didChangeValue(forKey: "window")
  }
}

extension UITextField {
  /// Adds a call to the KVO `willChangeValue(forKey:)` method.
  ///
  /// This version is necessary, as UITextField's original implementation doesn't call its
  /// superclass implementation, hence escapes the interposition at the UIView level.
  @objc override fileprivate func cr_willMove(toWindow newWindow: UIWindow?) {
    cr_willMove(toWindow: newWindow)
    willChangeValue(forKey: "window")
  }

  /// Adds a call to the KVO `didChangeValue(forKey:)` method.
  ///
  /// This version is necessary, as UITextField's original implementation doesn't call its
  /// superclass implementation, hence escapes the interposition at the UIView level.
  @objc override fileprivate func cr_didMoveToWindow() {
    cr_didMoveToWindow()
    didChangeValue(forKey: "window")
  }
}
