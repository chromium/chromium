// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// Lets any UIView communicate when its coordinates in its window change.
///
/// This is useful to know when a view moved on the screen, even when it didn't change frame in its
/// own parent.
///
/// To get notified when the view moved in its window:
///
///     let myView = UIView()
///     myView.cr_onWindowCoordinatesChanged = { view in
///       // Print the window coordinates.
///       print("\(view) moved to \(view convertRect:view.bounds toView:nil)")
///     }
///     let parentView = UIView()
///     parentView.addSubview(myView)
///     let window = UIWindow()
///     window.addSubview(parentView)  // → Calls the closure a first time.
///     parentView.frame = CGRect(x: 10, y: 20, width: 30, height: 40)  // → Calls the closure.
///
///  Even though `myView`'s frame itself was not modified in `parentView`, the closure is called, as
///  actually, `myView` moved transitively in its window.
///
@objc
extension UIView {
  /// MARK: Public

  /// Called when the window coordinates of the view changed.
  ///
  /// The view is passed as argument to the closure. Use it to avoid retaining the view in the
  /// closure, otherwise the view will leak and never get deinitialized.
  @objc public var cr_onWindowCoordinatesChanged: ((UIView) -> Void)? {
    get {
      objc_getAssociatedObject(self, UIView.OnWindowCoordinatesChangedKey) as? (UIView) -> Void
    }
    set {
      objc_setAssociatedObject(
        self, UIView.OnWindowCoordinatesChangedKey, newValue, .OBJC_ASSOCIATION_COPY)
      if newValue != nil {
        // Make sure UIView supports window observing.
        Self.cr_supportsWindowObserving = true
        observation = observe(\.window, options: [.initial]) { [weak self] _, _ in
          guard let self = self else { return }
          if self.window != nil {
            self.addMirrorViewInWindow()
            // Additionally, call the closure here as the view moved to a window.
            self.cr_onWindowCoordinatesChanged?(self)
          } else {
            self.removeMirrorViewInWindow()
          }
        }
      } else {
        observation = nil
      }
    }
  }

  /// MARK: Private

  /// The currently set observation of the window property.
  private var observation: NSKeyValueObservation? {
    get {
      objc_getAssociatedObject(self, UIView.ObservationKey) as? NSKeyValueObservation
    }
    set {
      objc_setAssociatedObject(
        self, UIView.ObservationKey, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }
  }

  /// Inserts a direct subview to the receiver's window, with constraints such that the mirror view
  /// always has the same window coordinates as the receiver. The mirror view calls the
  /// `onWindowCoordinatesChanged` closure when its bounds change.
  private func addMirrorViewInWindow() {
    let mirrorViewInWindow = NotifyingView()
    mirrorViewInWindow.backgroundColor = .clear
    mirrorViewInWindow.isUserInteractionEnabled = false
    mirrorViewInWindow.translatesAutoresizingMaskIntoConstraints = false
    mirrorViewInWindow.onLayoutChanged = { [weak self] _ in
      // Callback on the next turn of the run loop to wait for AutoLayout to have updated the entire
      // hierarchy. (It can happen that AutoLayout updates the mirror view before the mirrored
      // view.)
      DispatchQueue.main.async {
        guard let self = self else { return }
        self.cr_onWindowCoordinatesChanged?(self)
      }
    }

    guard let window = window else { fatalError() }
    window.insertSubview(mirrorViewInWindow, at: 0)
    NSLayoutConstraint.activate([
      mirrorViewInWindow.topAnchor.constraint(equalTo: topAnchor),
      mirrorViewInWindow.bottomAnchor.constraint(equalTo: bottomAnchor),
      mirrorViewInWindow.leadingAnchor.constraint(equalTo: leadingAnchor),
      mirrorViewInWindow.trailingAnchor.constraint(equalTo: trailingAnchor),
    ])

    self.mirrorViewInWindow = mirrorViewInWindow
  }

  /// Removes the mirror view added by a call to `addMirrorViewInWindow`.
  private func removeMirrorViewInWindow() {
    mirrorViewInWindow?.onLayoutChanged = nil
    mirrorViewInWindow?.removeFromSuperview()
    mirrorViewInWindow = nil
  }

  /// The currently set mirror view.
  private var mirrorViewInWindow: NotifyingView? {
    get {
      objc_getAssociatedObject(self, UIView.MirrorViewInWindowKey) as? NotifyingView
    }
    set {
      objc_setAssociatedObject(
        self, UIView.MirrorViewInWindowKey, newValue, .OBJC_ASSOCIATION_ASSIGN)
    }
  }

  /// A simple view that calls a closure when its bounds and center changed.
  private class NotifyingView: UIView {
    var onLayoutChanged: ((UIView) -> Void)?

    override var bounds: CGRect {
      didSet {
        onLayoutChanged?(self)
      }
    }

    override var center: CGPoint {
      didSet {
        onLayoutChanged?(self)
      }
    }
  }

  /// Keys for storing associated objects.
  @UniqueAddress private static var OnWindowCoordinatesChangedKey
  @UniqueAddress private static var ObservationKey
  @UniqueAddress private static var MirrorViewInWindowKey
}

/// A property wrapper to more safely support associated object keys.
/// https://github.com/atrick/swift-evolution/blob/diagnose-implicit-raw-bitwise/proposals/nnnn-implicit-raw-bitwise-conversion.md#associated-object-string-keys
@propertyWrapper
struct UniqueAddress {
  private var _placeholder: Int8 = 0

  var wrappedValue: UnsafeRawPointer {
    mutating get {
      // This is "ok" only as long as the wrapped property appears inside of something with a stable
      // address (a global/static variable or class property) and the pointer is never read or
      // written through, only used for its unique value.
      return withUnsafeBytes(of: &self) {
        return $0.baseAddress.unsafelyUnwrapped
      }
    }
  }
}
