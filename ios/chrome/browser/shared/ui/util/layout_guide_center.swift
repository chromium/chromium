// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// Globally maps reference views to layout guides.
///
/// Usage:
/// 1.  Reference a view from a part of the UI under a specific name.
/// 2.  In a different part of the UI, request a layout guide under that name. Add the synced layout
///     guide to the UI. It will track the reference view automatically.
///
/// Details:
/// -   Referenced views and created layout guides are only weakly stored by the layout guide
///     center.
/// -   Referenced views don't have to be laid out by AutoLayout.
/// -   Referenced views and layout guides don't have to be in the same window.
@MainActor
@objc
public class LayoutGuideCenter: NSObject {
  /// MARK: Public

  /// Potential parent layout guide center. Referenced views lookups fallback
  /// to looking up in the parent if the view is not reference by this layout
  /// guide center.
  @objc public weak var parent: LayoutGuideCenter?

  /// References a view under a specific `name`.
  /// If forcesSynchronousLayoutUpdates is true, when the window coordinates change, the layout guides will be
  /// updated synchronously. Otherwise, the layout guides will be updated in the next runloop.
  @objc(referenceView:underName:forcesSynchronousLayoutUpdates:)
  public func reference(
    view referenceView: UIView?, under name: String, forcesSynchronousLayoutUpdates: Bool
  ) {
    let oldReferenceView = referencedView(under: name)
    // Early return if `referenceView` is already set.
    if referenceView == oldReferenceView {
      return
    }
    oldReferenceView?.cr_forcesSynchronousLayoutUpdates = false
    oldReferenceView?.cr_onWindowCoordinatesChanged = nil
    if let referenceView = referenceView {
      referenceViews.setObject(referenceView, forKey: name as NSString)
    } else {
      referenceViews.removeObject(forKey: name as NSString)
    }
    updateGuides(named: name)
    referenceView?.cr_forcesSynchronousLayoutUpdates = forcesSynchronousLayoutUpdates
    // Schedule updates to the matching layout guides when the reference view
    // moves in its window.
    referenceView?.cr_onWindowCoordinatesChanged = { [weak self] _ in
      self?.updateGuides(named: name)
    }
  }

  /// References a view under a specific `name`.
  @objc(referenceView:underName:)
  public func reference(view referenceView: UIView?, under name: String) {
    self.reference(view: referenceView, under: name, forcesSynchronousLayoutUpdates: false)
  }

  /// Returns the referenced view under `name`.
  @objc(referencedViewUnderName:)
  public func referencedView(under name: String) -> UIView? {
    if let view = referenceViews.object(forKey: name as NSString) {
      return view
    }
    return parent?.referencedView(under: name)
  }

  /// Returns the frame of the view referenced under `name` in `view`'s
  /// coordinates, or `.zero` if no view is referenced or either view isn't in a
  /// window.
  ///
  /// Use this instead of a layout guide when the frame is only needed once,
  /// to avoid installing a guide that is updated whenever the referenced view
  /// moves.
  @objc(frameOfReferencedViewUnderName:convertedToView:)
  public func frameOfReferencedView(under name: String, convertedTo view: UIView) -> CGRect {
    guard let referenceView = referencedView(under: name) else { return .zero }
    return frame(of: referenceView, in: view) ?? .zero
  }

  /// Creates a new layout guide tracking the view referenced under a specific `name`.
  ///
  /// If the referenced view doesn't exist or isn't yet part of the view hierarchy, it is still
  /// valid to call this method. The layout guide will be updated as soon as the referenced view
  /// is available.
  @objc(makeLayoutGuideNamed:)
  public func makeLayoutGuide(named name: String) -> UILayoutGuide {
    let layoutGuide = FrameLayoutGuide()
    layoutGuide.onDidMoveToWindow = { [weak self] _ in
      self?.updateGuides(named: name)
    }
    // Keep a weak reference to the layout guide.
    if layoutGuides[name] == nil {
      layoutGuides[name] = NSHashTable<FrameLayoutGuide>.weakObjects()
    }
    layoutGuides[name]?.add(layoutGuide)
    return layoutGuide
  }

  /// MARK: Private

  /// Weakly stores the reference views.
  private var referenceViews = NSMapTable<NSString, UIView>.strongToWeakObjects()
  /// Weakly stores the layout guides.
  private var layoutGuides = [String: NSHashTable<FrameLayoutGuide>]()

  /// Updates all available guides for the given `name`.
  private func updateGuides(named name: String) {
    guard let referenceView = referencedView(under: name) else { return }
    // Early return if there are no layout guides.
    guard let layoutGuidesForName = layoutGuides[name] else { return }

    for layoutGuide in layoutGuidesForName.allObjects {
      guard let owningView = layoutGuide.owningView,
        let frameInOwningView = frame(of: referenceView, in: owningView)
      else { continue }
      layoutGuide.constrainedFrame = frameInOwningView
    }
  }

  /// Returns the frame of `referenceView` in `view`'s coordinates, or `nil` if
  /// either view isn't in a window. The views may be in different windows.
  private func frame(of referenceView: UIView, in view: UIView) -> CGRect? {
    guard let referenceWindow = referenceView.window, let window = view.window else {
      return nil
    }
    let frameInReferenceWindow = referenceView.convert(referenceView.bounds, to: nil)
    let frameInWindow = referenceWindow.convert(frameInReferenceWindow, to: window)
    return view.convert(frameInWindow, from: nil)
  }
}
