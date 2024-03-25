// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit
import ios_chrome_browser_ui_tab_switcher_tab_strip_ui_swift_constants

/// Layout used for the TabStrip.
class TabStripLayout: UICollectionViewFlowLayout {
  /// Wether the size of the items in the flow layout needs to be updated.
  public var needsSizeUpdate: Bool = true

  /// Static decoration views that border the collection view.
  public var leftStaticSeparator: TabStripDecorationView?
  public var rightStaticSeparator: TabStripDecorationView?

  /// The tab strip new tab button.
  public var newTabButton: UIView?

  /// Wether the selected cell is animated, used only on iOS 16.
  /// On iOS 16, the scroll animation after opening a new tab is delayed, the
  /// selected cell should remain in an animated state until the end of the
  /// (scroll) animation.
  public var cellAnimatediOS16: Bool = false

  /// Dynamic size of a tab.
  private var tabCellSize: CGSize = .zero

  /// Index paths of animated items.
  private var indexPathsOfDeletingItems: [IndexPath] = []
  private var indexPathsOfInsertingItems: [IndexPath] = []

  //// Leading constraint of the `newTabButton`.
  private var newTabButtonLeadingConstraint: NSLayoutConstraint?

  /// The DataSource for this collection view.
  weak var dataSource:
    UICollectionViewDiffableDataSource<TabStripViewController.Section, TabStripItemIdentifier>?

  override init() {
    super.init()
    scrollDirection = .horizontal
    minimumLineSpacing = TabStripConstants.TabItem.horizontalSpacing
    minimumInteritemSpacing = TabStripConstants.TabItem.horizontalSpacing
    sectionInset = UIEdgeInsets(
      top: TabStripConstants.CollectionView.topInset,
      left: TabStripConstants.CollectionView.horizontalInset,
      bottom: 0,
      right: TabStripConstants.CollectionView.horizontalInset)

    NotificationCenter.default.addObserver(
      self, selector: #selector(voiceOverChanged),
      name: UIAccessibility.voiceOverStatusDidChangeNotification, object: nil)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override var collectionViewContentSize: CGSize {
    let contentSize = super.collectionViewContentSize

    if !TabStripFeaturesUtils.isModernTabStripNewTabButtonDynamic() { return contentSize }
    guard
      let collectionView = collectionView,
      let newTabButton = newTabButton,
      let newTabButtonSuperView = newTabButton.superview
    else { return contentSize }

    let updatedConstant = min(
      contentSize.width, collectionView.bounds.width)

    if newTabButtonLeadingConstraint == nil {
      newTabButtonLeadingConstraint = newTabButton.leadingAnchor.constraint(
        equalTo: newTabButtonSuperView.leadingAnchor,
        constant: updatedConstant)
      newTabButtonLeadingConstraint?.priority = .defaultLow
      newTabButtonLeadingConstraint?.isActive = true
      return contentSize
    }

    if updatedConstant != newTabButtonLeadingConstraint?.constant {
      newTabButtonLeadingConstraint?.constant = updatedConstant
      weak var weakSelf = self
      UIView.animate(
        withDuration: TabStripConstants.NewTabButton.constraintUpdateAnimationDuration, delay: 0.0,
        options: .curveEaseOut,
        animations: {
          weakSelf?.newTabButtonConstraintUpdateAnimationBlock()
        })
    }

    return contentSize
  }

  // MARK: - Properties

  // Returns the selected item index path.
  private var selectedIndexPath: IndexPath? {
    guard let collectionView = collectionView else { return nil }
    return collectionView.indexPathsForSelectedItems?.first
  }

  // MARK: - UICollectionViewLayout

  override func prepare() {
    /// Only recalculate the `tabCellSize` when needed to avoid extra
    /// computation.
    if needsSizeUpdate {
      calculateTabCellSize()
    }
    super.prepare()
  }

  override func prepare(forCollectionViewUpdates updateItems: [UICollectionViewUpdateItem]) {
    super.prepare(forCollectionViewUpdates: updateItems)

    // Keeps track of updated items to animate their transition.
    indexPathsOfDeletingItems = []
    indexPathsOfInsertingItems = []
    for item in updateItems {
      switch item.updateAction {
      case .insert:
        indexPathsOfInsertingItems.append(item.indexPathAfterUpdate!)
        break
      case .delete:
        indexPathsOfDeletingItems.append(item.indexPathBeforeUpdate!)
        break
      default:
        break
      }
    }
  }

  override func finalizeCollectionViewUpdates() {
    indexPathsOfDeletingItems = []
    indexPathsOfInsertingItems = []
    super.finalizeCollectionViewUpdates()
  }

  override func initialLayoutAttributesForAppearingItem(at itemIndexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let attributes: UICollectionViewLayoutAttributes = super
        .initialLayoutAttributesForAppearingItem(at: itemIndexPath),
      let itemIdentifier = dataSource?.itemIdentifier(for: itemIndexPath)
    else { return nil }
    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      return initialLayoutAttributesForAppearingTabSwitcherItem(
        tabSwitcherItem, at: itemIndexPath, attributes: attributes)
    case .group(let tabGroupItem):
      return initialLayoutAttributesForAppearingTabGroupItem(
        tabGroupItem, at: itemIndexPath, attributes: attributes)
    }
  }

  override func finalLayoutAttributesForDisappearingItem(at itemIndexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let itemIdentifier = dataSource?.itemIdentifier(for: itemIndexPath),
      let attributes: UICollectionViewLayoutAttributes =
        super.finalLayoutAttributesForDisappearingItem(at: itemIndexPath)
    else { return nil }

    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      return finalLayoutAttributesForDisappearingTabSwitcherItem(
        tabSwitcherItem, at: itemIndexPath, attributes: attributes)
    case .group(let tabGroupItem):
      return finalLayoutAttributesForDisappearingTabGroupItem(
        tabGroupItem, at: itemIndexPath, attributes: attributes)
    }
  }

  override func layoutAttributesForItem(at indexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let itemIdentifier = dataSource?.itemIdentifier(for: indexPath),
      let layoutAttributes = super.layoutAttributesForItem(at: indexPath),
      let collectionView = collectionView
    else { return nil }

    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      return layoutAttributesForTabSwitcherItem(
        tabSwitcherItem, at: indexPath, layoutAttributes: layoutAttributes,
        collectionView: collectionView)
    case .group(let tabGroupItem):
      return layoutAttributesForTabGroupItem(
        tabGroupItem, at: indexPath, layoutAttributes: layoutAttributes,
        collectionView: collectionView)
    }
  }

  private func layoutAttributesForTabSwitcherItem(
    _ tabSwitcherItem: TabSwitcherItem, at indexPath: IndexPath,
    layoutAttributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    /// Early return the updated `selectedAttributes` if the cell is selected.
    if let selectedAttributes = self.layoutAttributesForSelectedCell(
      layoutAttributes: layoutAttributes)
    {
      return selectedAttributes
    }

    guard
      let cell = collectionView.cellForItem(at: indexPath) as? TabStripTabCell
    else { return layoutAttributes }

    let contentOffset = collectionView.contentOffset
    var frame = layoutAttributes.frame
    let collectionViewWidth = collectionView.bounds.size.width

    let leftBounds: CGFloat = contentOffset.x + sectionInset.left
    let rightBounds: CGFloat = collectionViewWidth + contentOffset.x - sectionInset.right
    let isScrollable: Bool = collectionView.contentSize.width > collectionView.frame.width
    let isRTL: Bool = collectionView.effectiveUserInterfaceLayoutDirection == .rightToLeft

    /// Hide the `trailingSeparator`if the next cell is selected.
    let isNextCellSelected = (indexPath.item + 1) == selectedIndexPath?.item
    cell.trailingSeparatorHidden = isNextCellSelected

    /// Hide the `leadingSeparator` if the previous cell is selected or the
    /// collection view is not scrollable.
    let isPreviousCellSelected = (indexPath.item - 1) == selectedIndexPath?.item
    cell.leadingSeparatorHidden = isPreviousCellSelected || !isScrollable

    if UIAccessibility.isVoiceOverRunning {
      // Prevent frame resizing while VoiceOver is active.
      // This ensures swiping right/left goes to the next cell.
      return layoutAttributes
    }

    /// Recalculate the cell width and origin when it intersects with the left
    /// collection view's bounds. The cell should collapse within the collection
    /// view's bounds until its width reaches 0. Its `separatorHeight` is also
    /// reduced when the cell reached an edege.
    var separatorHeight = TabStripConstants.AnimatedSeparator.regularSeparatorHeight
    if isScrollable && (frame.minX < leftBounds || frame.maxX > rightBounds) {
      // Show leading and trailing gradient.
      cell.leadingSeparatorGradientViewHidden = false
      cell.trailingSeparatorGradientViewHidden = false

      let collapseThreshold = TabStripConstants.AnimatedSeparator.collapseHorizontalInsetThreshold
      let collapseHorizontalInset = TabStripConstants.AnimatedSeparator.collapseHorizontalInset

      // If intersects with the left bounds.
      if frame.minX < leftBounds {
        cell.leadingSeparatorHidden = false

        // Update the frame origin and width.
        frame.origin.x = max(leftBounds, frame.origin.x)
        let offsetLeft: CGFloat = abs(frame.origin.x - layoutAttributes.frame.origin.x)
        frame.size.width = min(frame.size.width - offsetLeft, frame.size.width)

        /// Start animating the cell out of the collection view  if  the new
        /// width `frame.size.width` is less than or equal to
        /// `collapseThreshold`.
        if frame.size.width <= collapseThreshold {

          // Update the size of the separator.
          separatorHeight = calculateSeparatorsHeight(for: frame.size.width)

          // Move the cell to the left until it reaches its final position.
          frame.origin.x = max(
            frame.origin.x - collapseThreshold + frame.size.width,
            leftBounds - collapseHorizontalInset)

          // Update its alpha value.
          layoutAttributes.alpha = calculateAlphaValue(for: abs(frame.size.width))

          // Set its width to 0 and update its separators.
          frame.size.width = 0
          if !isRTL {
            cell.trailingSeparatorHidden = true
          } else {
            cell.leadingSeparatorHidden = true
          }
          cell.leadingSeparatorGradientViewHidden = true
          cell.trailingSeparatorGradientViewHidden = true
        }
      }

      // If intersects with the right bounds.
      else if frame.maxX > rightBounds {
        cell.trailingSeparatorHidden = false

        // Update the frame origin and width.
        frame.origin.x = min(rightBounds, frame.origin.x)
        frame.size.width = min(rightBounds - frame.origin.x, frame.size.width)

        /// Start animating the cell out of the collection view  if the new
        ///  width `frame.size.width` is less than or equal to
        ///  `collapseThreshold`.
        if frame.size.width <= collapseThreshold {

          // Update the size of the separator.
          separatorHeight = calculateSeparatorsHeight(for: frame.size.width)

          // Move the cell to the right until it reaches its final position.
          frame.origin.x = min(
            frame.origin.x + collapseThreshold - frame.size.width,
            rightBounds + collapseHorizontalInset)

          // Update its alpha value.
          let offset = layoutAttributes.frame.minX - frame.minX + collapseHorizontalInset
          layoutAttributes.alpha = calculateAlphaValue(for: offset)

          // Update its width.
          frame.size.width = max(
            min(rightBounds + collapseHorizontalInset - frame.origin.x, frame.size.width), 0)

          // Update its separators.
          if !isRTL {
            cell.leadingSeparatorHidden = true
          } else {
            cell.trailingSeparatorHidden = true
          }
          cell.leadingSeparatorGradientViewHidden = true
          cell.trailingSeparatorGradientViewHidden = true
        }
      }
    }

    // Update separators height once the computation is done.
    cell.setSeparatorsHeight(separatorHeight)

    layoutAttributes.frame = frame
    return layoutAttributes
  }

  private func layoutAttributesForTabGroupItem(
    _ tabGroupItem: TabGroupItem, at indexPath: IndexPath,
    layoutAttributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    return layoutAttributes
  }

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]?
  {
    let rectToConsider = CGRectInset(rect, -2 * TabStripConstants.TabItem.maxWidth, 0)
    guard
      let superAttributes = super.layoutAttributesForElements(in: rectToConsider)
    else { return nil }

    var indexPathToConsider = superAttributes.map(\.indexPath)
    if let selectedIndexPath = selectedIndexPath {
      if !indexPathToConsider.contains(selectedIndexPath) {
        indexPathToConsider.append(selectedIndexPath)
      }
    }

    return indexPathToConsider.compactMap { indexPath in
      layoutAttributesForItem(at: indexPath)
    }

  }

  override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
    return true
  }

  // MARK: - Private

  /// Animation block executed when `newTabButtonLeadingConstraint` is updated.
  private func newTabButtonConstraintUpdateAnimationBlock() {
    newTabButton?.superview?.layoutIfNeeded()
  }

  /// Returns the initial layout attributes for an appearing `TabSwitcherItem`.
  private func initialLayoutAttributesForAppearingTabSwitcherItem(
    _ tabSwitcherItem: TabSwitcherItem, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes
  )
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let selectedAttributes: UICollectionViewLayoutAttributes = layoutAttributesForSelectedCell(
        layoutAttributes: attributes)
    else { return nil }

    // Animate the appearing item by starting it with zero opacity and
    // translated down by its height.
    selectedAttributes.alpha = 0
    selectedAttributes.transform = CGAffineTransform(
      translationX: 0,
      y: attributes.frame.size.height)
    return selectedAttributes
  }

  /// Returns the initial layout attributes for an appearing `TabGroupItem`.
  private func initialLayoutAttributesForAppearingTabGroupItem(
    _ tabGroupItem: TabGroupItem, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes
  )
    -> UICollectionViewLayoutAttributes?
  {
    return attributes
  }

  /// Updates and returns the given `layoutAttributes` if the cell is selected.
  /// Inserted items are considered as selected.
  private func layoutAttributesForSelectedCell(layoutAttributes: UICollectionViewLayoutAttributes)
    -> UICollectionViewLayoutAttributes?
  {
    guard let collectionView = collectionView
    else { return nil }
    // The selected cell should remain on top of other cells within collection
    // view's bounds.
    let indexPath = layoutAttributes.indexPath
    guard
      indexPath == selectedIndexPath || indexPathsOfInsertingItems.contains(indexPath)
    else {
      return nil
    }

    /// `cellAnimatediOS16` is always `false` above iOS 16.
    var cellAnimated = cellAnimatediOS16
    let cell = collectionView.cellForItem(at: indexPath) as? TabStripTabCell
    if let animationKeys = cell?.layer.animationKeys() {
      cellAnimated = !animationKeys.isEmpty || cellAnimatediOS16
    }

    // Update cell separators.
    cell?.leadingSeparatorHidden = true
    cell?.trailingSeparatorHidden = true
    cell?.leadingSeparatorGradientViewHidden = true
    cell?.trailingSeparatorGradientViewHidden = true

    let isScrollable: Bool = collectionView.contentSize.width > collectionView.frame.width
    let collectionViewWidth = collectionView.bounds.size.width
    let horizontalOffset = collectionView.contentOffset.x
    let frame = layoutAttributes.frame
    var origin = layoutAttributes.frame.origin
    var horizontalInset: CGFloat = 0

    // Add a static separator horizontal inset only if the selected cell is the
    // first or the last one. Otherwise, when the selected cell is anchored and
    // a cell is scrolled behind, only one separator is displayed until the
    // horizontal inset threshold is reached.
    var staticSeparatorHorizontalInset: CGFloat = 0
    if let dataSource = dataSource, let sectionIndex = dataSource.index(for: .tabs) {
      let itemCount = dataSource.collectionView(
        collectionView, numberOfItemsInSection: sectionIndex)
      if indexPath.item == 0 || indexPath.item == itemCount - 1 {
        staticSeparatorHorizontalInset =
          tabCellSize.width - TabStripConstants.AnimatedSeparator.collapseHorizontalInsetThreshold
      }
    }

    var hideLeftStaticSeparator = true
    var hideRightStaticSeparator = true

    // If the collection view is scrollable, add an horizontal inset to its
    // origin.
    if isScrollable {
      horizontalInset = TabStripConstants.TabItem.horizontalSelectedInset
    }

    // Update the cell's origin horizontally to prevent it from being
    // partially hidden off-screen.

    // Check the left side.
    let minOringin = horizontalOffset + sectionInset.left + horizontalInset
    // Show left static separators when all of the following conditions are
    // satisfied:
    // - The selected cell is on the left edge.
    // - A cell behind the selected cell is also reaching the left edge.
    // - The cell is not animated (inserted / deleted).
    if (minOringin - staticSeparatorHorizontalInset) >= origin.x {
      hideLeftStaticSeparator = !isScrollable || cellAnimated
    }
    origin.x = max(origin.x, minOringin)

    // Check the right side.
    let maxOrigin =
      horizontalOffset + collectionViewWidth - frame.size.width - sectionInset.right
      - horizontalInset
    // Show right static separators when all of the following conditions are
    // satisfied:
    // - The selected cell is on the right edge.
    // - A cell behind the selected cell is also reaching the right edge.
    // - The cell is not animated (inserted / deleted).
    if (maxOrigin + staticSeparatorHorizontalInset) <= origin.x {
      hideRightStaticSeparator = !isScrollable || cellAnimated
    }
    origin.x = min(origin.x, maxOrigin)

    leftStaticSeparator?.isHidden = hideLeftStaticSeparator
    rightStaticSeparator?.isHidden = hideRightStaticSeparator
    cell?.leftSelectedBorderBackgroundViewHidden = hideLeftStaticSeparator
    cell?.rightSelectedBorderBackgroundViewHidden = hideRightStaticSeparator

    layoutAttributes.frame = CGRect(origin: origin, size: frame.size)
    layoutAttributes.zIndex = TabStripConstants.TabItem.selectedZIndex
    return layoutAttributes
  }

  /// Returns the final layout attributes for an disappearing `TabSwitcherItem`.
  private func finalLayoutAttributesForDisappearingTabSwitcherItem(
    _ tabSwitcherItem: TabSwitcherItem, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes
  )
    -> UICollectionViewLayoutAttributes?
  {
    var attributes = attributes
    /// Update `attributes` if the disappearing cell is selected.
    if let selectedAttributes = self.layoutAttributesForSelectedCell(
      layoutAttributes: attributes)
    {
      attributes = selectedAttributes
    }

    if indexPathsOfDeletingItems.contains(itemIndexPath) {
      // Animate the disappearing item by fading it out and translating it down
      // by its height.
      attributes.alpha = 0
      attributes.transform = CGAffineTransform(
        translationX: 0,
        y: attributes.frame.size.height
      )
    }
    return attributes
  }

  /// Returns the final layout attributes for an disappearing `TabGroupItem`.
  private func finalLayoutAttributesForDisappearingTabGroupItem(
    _ tabGroupItem: TabGroupItem, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes
  )
    -> UICollectionViewLayoutAttributes?
  {
    return attributes
  }

  /// This function calculates the separator height value for a given
  /// `frameWidth`. The returned value will always be within the range of
  /// `regularSeparatorHeight` and `minSeparatorHeight`.
  private func calculateSeparatorsHeight(for frameWidth: CGFloat) -> CGFloat {
    let regularHeight = TabStripConstants.AnimatedSeparator.regularSeparatorHeight
    let separatorHeight = min(
      regularHeight,
      regularHeight + frameWidth
        - TabStripConstants.AnimatedSeparator.collapseHorizontalInsetThreshold)
    return max(TabStripConstants.AnimatedSeparator.minSeparatorHeight, separatorHeight)
  }

  /// Calculates the alpha value for the given `offset`.
  private func calculateAlphaValue(for offset: CGFloat) -> CGFloat {
    var alpha: CGFloat = 1

    /// If the sum of `offset` and `collapseHorizontalInset` exceeds
    /// the `tabCellSize.width`, that means the cell will almost disappear from
    /// the collection view. Its alpha value should be reduced.
    let distance =
      offset + TabStripConstants.AnimatedSeparator.collapseHorizontalInset - tabCellSize.width
    if distance > 0 {
      alpha = 1 / distance
    }

    return alpha
  }

  // Called when voice over is activated.
  @objc func voiceOverChanged() {
    self.invalidateLayout()
  }

  // MARK: - Public

  // Calculates the dynamic size of a tab according to the number of tabs and
  // groups.
  public func calculateTabCellSize() {
    guard let collectionView = self.collectionView, let snapshot = dataSource?.snapshot() else {
      return
    }

    var groupCellWidthSum: CGFloat = 0
    var tabCellCount: CGFloat = 0
    let cellCount: CGFloat = CGFloat(snapshot.itemIdentifiers.count)

    if cellCount == 0 {
      return
    }

    for itemIdentifier in snapshot.itemIdentifiers {
      switch itemIdentifier.item {
      case .tab(_):
        tabCellCount += 1
      case .group(let tabGroupItem):
        groupCellWidthSum += calculateCellSizeForTabGroupItem(tabGroupItem).width
      }
    }

    let collectionViewWidth: CGFloat = CGRectGetWidth(collectionView.bounds)
    let itemSpacingSum: CGFloat =
      minimumLineSpacing * (cellCount - 1) + sectionInset.left + sectionInset.right

    var itemWidth: CGFloat =
      (collectionViewWidth - itemSpacingSum - groupCellWidthSum) / tabCellCount
    itemWidth = max(itemWidth, TabStripConstants.TabItem.minWidth)
    itemWidth = min(itemWidth, TabStripConstants.TabItem.maxWidth)

    tabCellSize = CGSize(width: itemWidth, height: TabStripConstants.TabItem.height)
  }

  public func calculateCellSizeForTabSwitcherItem(_ tabSwitcherItem: TabSwitcherItem) -> CGSize {
    return tabCellSize
  }

  public func calculateCellSizeForTabGroupItem(_ tabGroupItem: TabGroupItem) -> CGSize {
    var width = tabGroupItem.title.size(withAttributes: [
      .font: UIFont.systemFont(ofSize: TabStripConstants.GroupItem.fontSize, weight: .medium)
    ]).width
    width += 2 * TabStripConstants.GroupItem.titleContainerHorizontalMargin
    width += 2 * TabStripConstants.GroupItem.titleContainerHorizontalPadding
    return CGSize(width: width, height: TabStripConstants.GroupItem.height)
  }

}
