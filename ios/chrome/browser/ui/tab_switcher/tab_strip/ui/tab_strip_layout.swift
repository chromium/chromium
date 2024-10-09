// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit
import ios_chrome_browser_ui_tab_switcher_tab_strip_ui_swift_constants

/// Layout used for the TabStrip.
class TabStripLayout: UICollectionViewFlowLayout {
  /// Whether the size of the items in the flow layout needs to be updated.
  public var needsSizeUpdate: Bool = true

  /// Static decoration views that border the collection view.
  public var leadingStaticSeparator: TabStripDecorationView?
  public var trailingStaticSeparator: TabStripDecorationView?

  /// The tab strip new tab button.
  public var newTabButton: UIView?

  /// Whether the selected cell is animated, used only on iOS 16.
  /// On iOS 16, the scroll animation after opening a new tab is delayed, the
  /// selected cell should remain in an animated state until the end of the
  /// (scroll) animation.
  public var cellAnimatediOS16: Bool = false

  /// Dynamic size of a tab.
  private var tabCellSize: CGSize = .zero

  /// Index paths of animated items.
  private var indexPathsOfDeletingItems: [IndexPath] = []
  private var indexPathsOfInsertingItems: [IndexPath] = []

  /// Whether items are currently being collapsed/expanded.
  private var expandingItems = false
  private var collapsingItems = false

  /// The currently selected item.
  public var selectedItem: TabSwitcherItem? = nil

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

    if !TabStripFeaturesUtils.isModernTabStripNewTabButtonDynamic { return contentSize }
    guard
      let collectionView = collectionView,
      let newTabButton = newTabButton,
      let newTabButtonSuperView = newTabButton.superview
    else { return contentSize }

    var offset: CGFloat =
      TabStripFeaturesUtils.hasCloserNTB ? 16 : 0

    // Compare with "width - 1" to avoid floating comparison issues.
    if contentSize.width >= collectionView.bounds.width - 1 {
      // When the contentSize width is greater or equals to the collection view width, the
      // offset should be reduced to allow spacing for the separators.
      offset = 6
    }

    let updatedConstant =
      min(
        contentSize.width, collectionView.bounds.width) - offset

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
    return TabStripItemIdentifier(selectedItem).flatMap {
      dataSource?.indexPath(for: $0)
    }
  }

  // MARK: - UICollectionViewLayout

  override var flipsHorizontallyInOppositeLayoutDirection: Bool {
    return true
  }

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
      case .insert where !expandingItems:
        indexPathsOfInsertingItems.append(item.indexPathAfterUpdate!)
        break
      case .delete where !collapsingItems:
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
      let collectionView = collectionView,
      let attributes: UICollectionViewLayoutAttributes = super
        .initialLayoutAttributesForAppearingItem(at: itemIndexPath),
      let itemIdentifier = dataSource?.itemIdentifier(for: itemIndexPath)
    else { return nil }
    switch itemIdentifier.item {
    case .tab(_):
      return initialLayoutAttributesForAppearingTabCell(
        at: itemIndexPath, attributes: attributes, collectionView: collectionView)
    case .group(_):
      return initialLayoutAttributesForAppearingGroupCell(
        at: itemIndexPath, attributes: attributes, collectionView: collectionView)
    }
  }

  override func finalLayoutAttributesForDisappearingItem(at itemIndexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let collectionView = collectionView,
      let cell = collectionView.cellForItem(at: itemIndexPath),
      let attributes: UICollectionViewLayoutAttributes =
        super.finalLayoutAttributesForDisappearingItem(at: itemIndexPath)
    else { return nil }

    switch cell {
    case let tabCell as TabStripTabCell:
      return finalLayoutAttributesForDisappearingTabCell(
        tabCell, at: itemIndexPath, attributes: attributes, collectionView: collectionView)
    case let groupCell as TabStripGroupCell:
      return finalLayoutAttributesForDisappearingGroupCell(
        groupCell, at: itemIndexPath, attributes: attributes, collectionView: collectionView)
    default:
      return nil
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
    let cell = collectionView.cellForItem(at: indexPath)
    switch itemIdentifier.item {
    case .tab(_):
      let tabCell = cell as? TabStripTabCell
      return layoutAttributesForTabCell(
        tabCell, at: indexPath, layoutAttributes: layoutAttributes,
        collectionView: collectionView)
    case .group(_):
      let groupCell = cell as? TabStripGroupCell
      return layoutAttributesForGroupCell(
        groupCell, at: indexPath, layoutAttributes: layoutAttributes,
        collectionView: collectionView)
    }
  }

  private func layoutAttributesForTabCell(
    _ cell: TabStripTabCell?, at indexPath: IndexPath,
    layoutAttributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    /// Early return the updated `selectedAttributes` if the cell is selected.
    if let selectedAttributes = self.layoutAttributesForSelectedTabCell(
      cell, at: indexPath, layoutAttributes: layoutAttributes, collectionView: collectionView)
    {
      return selectedAttributes
    }

    guard let cell = cell else { return layoutAttributes }

    layoutAttributes.zIndex = 0

    let contentOffset = collectionView.contentOffset
    var frame = layoutAttributes.frame
    let collectionViewWidth = collectionView.bounds.size.width

    let leftBounds: CGFloat = contentOffset.x + sectionInset.left
    let rightBounds: CGFloat = collectionViewWidth + contentOffset.x - sectionInset.right
    let isScrollable: Bool = collectionView.contentSize.width > collectionView.frame.width

    /// Hide the `trailingSeparator`if the next cell is selected.
    let isNextCellSelected = (indexPath.item + 1) == selectedIndexPath?.item
    cell.trailingSeparatorHidden = isNextCellSelected

    /// Hide the `leadingSeparator` if the previous cell is selected or this is the first cell and collection
    /// view is not scrollable, or the previous cell is a group.
    let indexPathOfPreviousItem = IndexPath(item: indexPath.item - 1, section: indexPath.section)
    let isFirstCellAndNotScrollable = !isScrollable && (indexPath.item == 0)
    let isPreviousCellSelected = indexPathOfPreviousItem == selectedIndexPath
    cell.leadingSeparatorHidden =
      isPreviousCellSelected || isFirstCellAndNotScrollable || cell.isFirstTabInGroup

    if UIAccessibility.isVoiceOverRunning {
      // Prevent frame resizing while VoiceOver is active.
      // This ensures swiping right/left goes to the next cell.
      return layoutAttributes
    }

    var intersectsLeftEdge = false
    var intersectsRightEdge = false

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
        intersectsLeftEdge = true

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
          cell.trailingSeparatorHidden = true
          cell.leadingSeparatorGradientViewHidden = true
          cell.trailingSeparatorGradientViewHidden = true
        }
      }

      // If intersects with the right bounds.
      else if frame.maxX > rightBounds {
        cell.trailingSeparatorHidden = false
        intersectsRightEdge = true

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
          cell.leadingSeparatorHidden = true
          cell.leadingSeparatorGradientViewHidden = true
          cell.trailingSeparatorGradientViewHidden = true
        }
      }
    }

    // Update separators height once the computation is done.
    cell.setSeparatorsHeight(separatorHeight)
    cell.intersectsLeftEdge = intersectsLeftEdge
    cell.intersectsRightEdge = intersectsRightEdge

    if TabStripFeaturesUtils.hasCloseButtonsVisible {
      let visibilityChangeWidth = TabStripConstants.TabItem.closeButtonVisibilityWidth
      let closeHiddenWidth = TabStripConstants.TabItem.minWidthV3 - visibilityChangeWidth
      let visibility = (frame.width - closeHiddenWidth) / visibilityChangeWidth
      cell.setCloseButtonVisibility(min(max(visibility, 0), 1))
    }

    layoutAttributes.frame = frame
    return layoutAttributes
  }

  private func layoutAttributesForGroupCell(
    _ groupCell: TabStripGroupCell?, at indexPath: IndexPath,
    layoutAttributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    if UIAccessibility.isVoiceOverRunning {
      // Prevent frame resizing while VoiceOver is active.
      // This ensures swiping right/left goes to the next cell.
      return layoutAttributes
    }

    guard let groupCell = groupCell else { return layoutAttributes }

    let contentOffset = collectionView.contentOffset
    var frame = layoutAttributes.frame
    let collectionViewWidth = collectionView.bounds.size.width

    let leftBounds: CGFloat = contentOffset.x + sectionInset.left
    let rightBounds: CGFloat = collectionViewWidth + contentOffset.x - sectionInset.right
    let isScrollable: Bool = collectionView.contentSize.width > collectionView.frame.width

    var intersectsLeftEdge = false
    var intersectsRightEdge = false

    /// Recalculate the cell width and origin when it intersects with the left
    /// collection view's bounds. The cell should collapse within the collection
    /// view's bounds until its width reaches 0.
    if isScrollable && (frame.minX < leftBounds || frame.maxX > rightBounds) {
      let minCellWidth = TabStripConstants.GroupItem.minCellWidth

      // If intersects with the left bounds.
      if frame.minX < leftBounds {
        intersectsLeftEdge = true
        // Update the frame origin and width.
        frame.origin.x = max(leftBounds, frame.origin.x)
        let offsetLeft: CGFloat = abs(frame.origin.x - layoutAttributes.frame.origin.x)
        frame.size.width = min(frame.size.width - offsetLeft, frame.size.width)

        /// Start animating the cell out of the collection view  if  the new
        /// width `frame.size.width` is less than or equal to
        /// `collapseThreshold`.
        if frame.size.width <= minCellWidth {
          // Move the cell to the left until it reaches its final position.
          frame.origin.x = frame.origin.x - minCellWidth + frame.size.width
          frame.size.width = minCellWidth
        }
      }

      // If intersects with the right bounds.
      else if frame.maxX > rightBounds {
        intersectsRightEdge = true
        // Update the frame origin and width.
        frame.size.width = min(rightBounds - frame.origin.x, frame.size.width)

        /// Start animating the cell out of the collection view  if the new
        ///  width `frame.size.width` is less than or equal to
        ///  `collapseThreshold`.
        if frame.size.width <= minCellWidth {
          frame.size.width = minCellWidth
        }
      }
    }

    groupCell.intersectsLeftEdge = intersectsLeftEdge
    groupCell.intersectsRightEdge = intersectsRightEdge

    layoutAttributes.frame = frame
    return layoutAttributes
  }

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]?
  {
    let rectToConsider = CGRectInset(rect, -2 * TabStripConstants.TabItem.maxWidth, 0)
    guard
      let superAttributes = super.layoutAttributesForElements(in: rectToConsider)
    else { return nil }

    var indexPathToConsider = superAttributes.map(\.indexPath)
    // If there is a selected tab, and its index path is one of the visible
    // index paths of the collection view, add it to the list of index paths to
    // consider.
    if let selectedIndexPath = selectedIndexPath,
      collectionView?.indexPathsForVisibleItems.contains(selectedIndexPath) == true
    {
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

  /// Returns the initial layout attributes for an appearing `tabCell`.
  private func initialLayoutAttributesForAppearingTabCell(
    at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    let tabCell = collectionView.cellForItem(at: itemIndexPath) as? TabStripTabCell
    guard
      let selectedAttributes: UICollectionViewLayoutAttributes =
        self.layoutAttributesForSelectedTabCell(
          tabCell, at: itemIndexPath, layoutAttributes: attributes, collectionView: collectionView)
    else { return nil }

    if indexPathsOfInsertingItems.contains(itemIndexPath) {
      // Animate the appearing item by starting it with zero opacity and
      // translated down by its height.
      selectedAttributes.alpha = 0
      selectedAttributes.transform = CGAffineTransform(
        translationX: 0,
        y: attributes.frame.size.height)
    }
    return selectedAttributes
  }

  /// Returns the initial layout attributes for an appearing `groupCell`.
  private func initialLayoutAttributesForAppearingGroupCell(
    at itemIndexPath: IndexPath, attributes: UICollectionViewLayoutAttributes,
    collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    return attributes
  }

  /// Updates and returns the given `attributes` if the cell is selected.
  /// Inserted items are considered as selected.
  private func layoutAttributesForSelectedTabCell(
    _ cell: TabStripTabCell?, at indexPath: IndexPath,
    layoutAttributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    // The selected cell should remain on top of other cells within collection
    // view's bounds.
    guard
      indexPath == selectedIndexPath || indexPathsOfInsertingItems.contains(indexPath)
    else {
      return nil
    }

    /// `cellAnimatediOS16` is always `false` above iOS 16.
    var cellAnimated = cellAnimatediOS16
    if let animationKeys = cell?.layer.animationKeys() {
      cellAnimated = !animationKeys.isEmpty || cellAnimatediOS16
    }

    var intersectsLeftEdge = false
    var intersectsRightEdge = false

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

    var hideLeadingStaticSeparator = true
    var hideTrailingStaticSeparator = true

    // If the collection view is scrollable, add an horizontal inset to its
    // origin.
    if isScrollable {
      horizontalInset = TabStripConstants.TabItem.horizontalSelectedInset
    }

    // Update the cell's origin horizontally to prevent it from being
    // partially hidden off-screen.

    // Check the left side.
    let minOringin = horizontalOffset + sectionInset.left + horizontalInset
    // Show leading static separators when all of the following conditions are
    // satisfied:
    // - The selected cell is on the leading edge.
    // - A cell behind the selected cell is also reaching the leading edge.
    // - The cell is not animated (inserted / deleted).
    if (minOringin - staticSeparatorHorizontalInset) >= origin.x {
      hideLeadingStaticSeparator = !isScrollable || cellAnimated
    }
    if origin.x < minOringin {
      origin.x = minOringin
      intersectsLeftEdge = true
    }

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
      hideTrailingStaticSeparator = !isScrollable || cellAnimated
    }
    if origin.x > maxOrigin {
      origin.x = maxOrigin
      intersectsRightEdge = true
    }

    cell?.intersectsLeftEdge = intersectsLeftEdge
    cell?.intersectsRightEdge = intersectsRightEdge

    leadingStaticSeparator?.isHidden = hideLeadingStaticSeparator
    trailingStaticSeparator?.isHidden = hideTrailingStaticSeparator
    cell?.leadingSelectedBorderBackgroundViewHidden = hideLeadingStaticSeparator
    cell?.trailingSelectedBorderBackgroundViewHidden = hideTrailingStaticSeparator

    layoutAttributes.frame = CGRect(origin: origin, size: frame.size)
    layoutAttributes.zIndex = TabStripConstants.TabItem.selectedZIndex
    return layoutAttributes
  }

  /// Returns the final layout attributes for andisappearing `tabCell`.
  private func finalLayoutAttributesForDisappearingTabCell(
    _ tabCell: TabStripTabCell?, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
  )
    -> UICollectionViewLayoutAttributes?
  {
    var attributes = attributes
    /// Update `attributes` if the disappearing cell is selected.
    if let selectedAttributes = self.layoutAttributesForSelectedTabCell(
      tabCell, at: itemIndexPath, layoutAttributes: attributes, collectionView: collectionView)
    {
      attributes = selectedAttributes
    }

    if indexPathsOfDeletingItems.contains(itemIndexPath) {
      tabCell?.leadingSeparatorHidden = true
      tabCell?.trailingSeparatorHidden = true

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

  /// Returns the final layout attributes for a disappearing `groupCell`.
  private func finalLayoutAttributesForDisappearingGroupCell(
    _ groupCell: TabStripGroupCell?, at itemIndexPath: IndexPath,
    attributes: UICollectionViewLayoutAttributes, collectionView: UICollectionView
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
      alpha = max(0, 1 - distance / TabStripConstants.TabItem.maximumVisibleDistance)
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
    guard let collectionView = self.collectionView, let snapshot = dataSource?.snapshot(for: .tabs)
    else {
      return
    }

    var groupCellWidthSum: CGFloat = 0
    var tabCellCount: CGFloat = 0
    let cellCount: CGFloat = CGFloat(snapshot.visibleItems.count)

    if cellCount == 0 {
      return
    }

    for itemIdentifier in snapshot.visibleItems {
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
    if TabStripFeaturesUtils.hasCloseButtonsVisible {
      itemWidth = max(itemWidth, TabStripConstants.TabItem.minWidthV3)
    } else {
      itemWidth = max(itemWidth, TabStripConstants.TabItem.minWidth)
    }
    itemWidth = min(itemWidth, TabStripConstants.TabItem.maxWidth)

    tabCellSize = CGSize(width: itemWidth, height: TabStripConstants.TabItem.height)
  }

  public func calculateCellSizeForTabSwitcherItem(_ tabSwitcherItem: TabSwitcherItem) -> CGSize {
    return tabCellSize
  }

  public func calculateCellSizeForTabGroupItem(_ tabGroupItem: TabGroupItem) -> CGSize {
    var width =
      tabGroupItem.title?.size(withAttributes: [
        .font: UIFont.systemFont(ofSize: TabStripConstants.GroupItem.fontSize, weight: .medium)
      ]).width ?? 0
    width += 2 * TabStripConstants.GroupItem.titleContainerHorizontalMargin
    width += 2 * TabStripConstants.GroupItem.titleContainerHorizontalPadding
    width = min(width, TabStripConstants.GroupItem.maxCellWidth)
    return CGSize(width: width, height: TabStripConstants.GroupItem.height)
  }

  /// Prepares the layout for collection view updates resulting from expanding items.
  public func prepareForItemsExpanding() {
    self.expandingItems = true
  }

  /// Finalizes collection view updates resulting from expanding items.
  public func finalizeItemsExpanding() {
    self.expandingItems = false
  }

  /// Prepares the layout for collection view updates resulting from collapsing items.
  public func prepareForItemsCollapsing() {
    self.collapsingItems = true
  }

  /// Finalizes collection view updates resulting from collapsing items.
  public func finalizeItemsCollapsing() {
    self.collapsingItems = false
  }
}
