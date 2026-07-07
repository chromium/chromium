// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

private enum Constants {
  /// The width of the share sheet in compact height environments.
  static let compactHeightSheetWidth: CGFloat = 568
}

// UIHostingController subclass for the overflow menu. Mostly used to set
// preferredContentSize in compact height environments.
class OverflowMenuHostingController<Content>: UIHostingController<Content> where Content: View {
  let uiConfiguration: OverflowMenuUIConfiguration
  private var contentSizeObservation: NSKeyValueObservation?
  private var scrollView: UIScrollView?
  private var lastLaidOutBoundsSize: CGSize = .zero

  init(rootView: Content, uiConfiguration: OverflowMenuUIConfiguration) {
    self.uiConfiguration = uiConfiguration
    super.init(rootView: rootView)
    let sizeTraits: [UITrait] = [UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self]
    self.registerForTraitChanges(sizeTraits, action: #selector(updateUIOnTraitChange))
  }

  required init(coder aDecoder: NSCoder) {
    fatalError("Not using storyboards")
  }

  var compactHeightPreferredContentSize: CGSize {
    return CGSize(
      width: Constants.compactHeightSheetWidth,
      height: presentingViewController?.view.bounds.size.height ?? 0
    )
  }

  // MARK: - UIViewController

  override func viewDidLoad() {
    super.viewDidLoad()

    // Only set the preferredContentSize in height == compact because otherwise
    // it overrides the default size of the menu on iPad.
    preferredContentSize =
      traitCollection.verticalSizeClass == .compact ? compactHeightPreferredContentSize : .zero

    uiConfiguration.presentingViewControllerHorizontalSizeClass =
      presentingViewController?.traitCollection.horizontalSizeClass == .regular
      ? .regular : .compact
    uiConfiguration.presentingViewControllerVerticalSizeClass =
      presentingViewController?.traitCollection.verticalSizeClass == .regular ? .regular : .compact
  }

  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    setupScrollViewObservation()
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    setupScrollViewObservation()
    if self.view.bounds.size != lastLaidOutBoundsSize {
      lastLaidOutBoundsSize = self.view.bounds.size
      if let scrollView = self.scrollView {
        self.updatePreferredContentSize(contentSize: scrollView.contentSize)
      }
    }
  }

  // Updates the presented view controller's horizontal and vertical layout on UITrait changes.
  @objc func updateUIOnTraitChange() {
    // Only set the preferredContentSize in height == compact because otherwise
    // it overrides the default size of the menu on iPad.
    preferredContentSize =
      traitCollection.verticalSizeClass == .compact ? compactHeightPreferredContentSize : .zero

    uiConfiguration.presentingViewControllerHorizontalSizeClass =
      presentingViewController?.traitCollection.horizontalSizeClass == .regular
      ? .regular : .compact
    uiConfiguration.presentingViewControllerVerticalSizeClass =
      presentingViewController?.traitCollection.verticalSizeClass == .regular ? .regular : .compact
  }

  // MARK: - Private

  /// Observes the underlying scroll view's content size.
  ///
  /// Since the menu is built in SwiftUI, there is no public API to get the
  /// exact height of the list. Observing the UIKit scroll view's `contentSize`
  /// is the only way to dynamically size the sheet to fit its contents
  /// without using hardcoded height estimates that break with dynamic type
  /// or localized text wrapping.
  private func setupScrollViewObservation() {
    guard contentSizeObservation == nil else { return }
    guard let scrollView = findActionsScrollView(in: self.view) else { return }

    self.scrollView = scrollView

    contentSizeObservation = scrollView.observe(\.contentSize, options: [.initial, .new]) {
      [weak self] _, _ in
      guard let self = self else { return }
      // Swift KVO closures run in a non-isolated context. Since both `UIScrollView`
      // and this class are `@MainActor` (main thread)-isolated, wrap the update in an
      // asynchronous Task on `@MainActor` to safely access UI properties.
      Task { @MainActor in
        if let scrollView = self.scrollView {
          self.updatePreferredContentSize(contentSize: scrollView.contentSize)
        }
      }
    }
  }

  /// Updates the view controller's `preferredContentSize` based on the scroll
  /// view's content size, top insets, and bottom safe area. This is restricted
  /// to iPhone portrait to preserve default popover sizing on iPad.
  private func updatePreferredContentSize(contentSize: CGSize) {
    // Only update preferredContentSize dynamically in regular height (portrait iPhone).
    // In compact height (landscape iPhone), use compactHeightPreferredContentSize.
    // On iPad, keep default popover sizing (.zero).
    guard
      traitCollection.userInterfaceIdiom == .phone
        && traitCollection.verticalSizeClass == .regular
    else { return }

    guard let scrollView = self.scrollView else { return }

    // Convert the scroll view's origin to the hosting controller's view coordinate space
    // to get the absolute height of everything above it (destinations row + divider).
    let scrollViewOrigin = self.view.convert(scrollView.bounds.origin, from: scrollView)
    let totalHeight =
      scrollViewOrigin.y
      + contentSize.height
      + scrollView.adjustedContentInset.top

    let newSize = CGSize(width: self.preferredContentSize.width, height: totalHeight)
    if self.preferredContentSize != newSize {
      self.preferredContentSize = newSize
    }
  }

  /// Recursively searches the view hierarchy for the vertical `UICollectionView`
  /// or `UITableView` backing the actions list, ignoring the horizontal
  /// destinations scroll view.
  private func findActionsScrollView(in view: UIView) -> UIScrollView? {
    // The vertical actions list is backed by a UICollectionView.
    // The horizontal destinations row is backed by a plain UIScrollView.
    if view is UICollectionView || view is UITableView {
      return view as? UIScrollView
    }
    for subview in view.subviews {
      if let scrollView = findActionsScrollView(in: subview) {
        return scrollView
      }
    }
    return nil
  }
}
