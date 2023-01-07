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
@objc
public class LayoutGuideCenter: NSObject {
  /// MARK: Public

  /// References a view under a specific `name`.
  @objc(referenceView:underName:)
  public func reference(view referenceView: UIView?, under name: String) {
    let oldReferenceView = referencedView(under: name)
    // Early return if `referenceView` is already set.
    if referenceView == oldReferenceView {
      return
    }
    oldReferenceView?.cr_onWindowCoordinatesChanged = nil
    if let referenceView = referenceView {
      referenceViews.setObject(referenceView, forKey: name as NSString)
    } else {
      referenceViews.removeObject(forKey: name as NSString)
    }
    updateGuides(named: name)
    // Schedule updates to the matching layout guides when the reference view
    // moves in its window.
    referenceView?.cr_onWindowCoordinatesChanged = { [weak self] _ in
      self?.updateGuides(named: name)
    }
  }

  /// Returns the referenced view under `name`.
  @objc(referencedViewUnderName:)
  public func referencedView(under name: String) -> UIView? {
    return referenceViews.object(forKey: name as NSString)
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
    // Early return if there is no reference window.
    guard let referenceView = referencedView(under: name) else { return }
    guard let referenceWindow = referenceView.window else { return }
    // Early return if there are no layout guides.
    guard let layoutGuidesForName = layoutGuides[name] else { return }

    for layoutGuide in layoutGuidesForName.allObjects {
      // Skip if there is no owning window.
      guard let owningView = layoutGuide.owningView else { continue }
      guard let owningWindow = owningView.window else { continue }

      let frameInReferenceWindow = referenceView.convert(referenceView.bounds, to: nil)
      let frameInOwningWindow = referenceWindow.convert(frameInReferenceWindow, to: owningWindow)
      let frameInOwningView = owningView.convert(frameInOwningWindow, from: nil)
      layoutGuide.constrainedFrame = frameInOwningView
    }
  }
}
