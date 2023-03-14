// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// A layout guide with two distinct additions:
/// 1.  encapsulates the KVO on its owning view's window and triggers a callback closure;
/// 2.  can be constrained to a fixed frame in its owning view coordinates.
///
/// 1. To get notified when the layout guide is added to a new window:
///
///     let layoutGuide = FrameLayoutGuide()
///     layoutGuide.onDidMoveToWindow = { guide in
///       if let window = guide.owningView?.window {
///         print("\(guide) moved to \(guide.owningView?.window)")
///       } else {
///         print("\(guide) was removed from its window")
///       }
///     }
///
///  2. To constrain a frame to the layout guide:
///
///      let layoutGuide = FrameLayoutGuide()
///      layoutGuide.constrainedFrame = CGRect(x: 10, y: 20, width: 30, height: 40)
///
///  The layout guide can then be used as an anchor to place elements related to its position.
@objc
public class FrameLayoutGuide: UILayoutGuide {
  /// MARK: Public

  /// Called when the layout guide's owning view moved to a new window (or was removed from its
  /// window).
  ///
  /// The layout guide is passed as argument to the closure. Use it to avoid retaining the layout
  /// guide in the closure, otherwise the layout guide will leak and never get deinitialized.
  @objc public var onDidMoveToWindow: ((UILayoutGuide) -> Void)?

  /// The frame to force on this layout guide.
  @objc public var constrainedFrame: CGRect {
    get {
      constrainedFrameView.frame
    }
    set {
      constrainedFrameView.frame = newValue
    }
  }

  /// MARK: Private

  /// When `owningView` changes, remove `constrainedFrameView` from the old to the new view, then
  /// reset the constraints anchoring the layout guide on `constrainedFrameView`.
  override open var owningView: UIView? {
    willSet {
      constrainedFrameView.removeFromSuperview()
    }
    didSet {
      if let owningView = owningView {
        owningView.addSubview(constrainedFrameView)
        NSLayoutConstraint.activate([
          leadingAnchor.constraint(equalTo: constrainedFrameView.leadingAnchor),
          trailingAnchor.constraint(equalTo: constrainedFrameView.trailingAnchor),
          topAnchor.constraint(equalTo: constrainedFrameView.topAnchor),
          bottomAnchor.constraint(equalTo: constrainedFrameView.bottomAnchor),
        ])
      }
    }
  }

  /// The observation of the owning view's window property. It's an optional variable because it
  /// can't be initialized before `self` is. See `init`.
  private var observation: NSKeyValueObservation?

  /// The view to inject in the owning view. Its frame will be set to `constrainedFrame`. The layout
  /// guide will anchor itself on that view.
  private let constrainedFrameView: UIView

  @objc
  public override init() {
    constrainedFrameView = UIView()
    constrainedFrameView.backgroundColor = .clear
    constrainedFrameView.isUserInteractionEnabled = false
    super.init()

    // Make sure UIView supports window observing.
    UIView.cr_supportsWindowObserving = true
    // Start observing. It is not possible to initialize `observation` before `super.init()` because
    // the call to `observe(_:changeHandler:)` requires `self` to be initialized.
    // https://developer.apple.com/documentation/swift/cocoa_design_patterns/using_key-value_observing_in_swift
    observation = observe(\.owningView?.window, options: [.old, .new]) { layoutGuide, change in
      // Filter out changes where owning view changed but not the window. This can happen if a
      // layout guide is moved to a different owning view whose window is the same.
      if change.oldValue != change.newValue {
        layoutGuide.onDidMoveToWindow?(layoutGuide)
      }
    }
  }

  required init(coder aDecoder: NSCoder) {
    fatalError("Not using storyboards")
  }
}
