// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// Layout used for the TabStrip.
class TabStripLayout: UICollectionViewFlowLayout {
  /// Wether the flow layout needs to be updated.
  public var needsUpdate: Bool = true

  /// IndexPath of the selected item.
  public var selectedIndexPath: IndexPath?

  /// Dynamic size of a tab.
  private var tabCellSize: CGSize = .zero

  /// Last update item action.
  public var lastUpdateAction: UICollectionViewUpdateItem.Action = .none

  /// Index paths of animated items.
  private var indexPathsOfDeletingItems: [IndexPath] = []
  private var indexPathsOfInsertingItems: [IndexPath] = []

  /// The DataSource for this collection view.
  weak var dataSource:
    UICollectionViewDiffableDataSource<TabStripViewController.Section, TabSwitcherItem>?

  override init() {
    super.init()
    scrollDirection = .horizontal
    minimumLineSpacing = TabStripConstants.TabItem.horizontalSpacing
    sectionInset = UIEdgeInsets(
      top: TabStripConstants.CollectionView.topInset,
      left: TabStripConstants.CollectionView.horizontalInset,
      bottom: 0,
      right: TabStripConstants.CollectionView.horizontalInset)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - UICollectionViewLayout

  override func prepare() {
    /// Only recalculate the `tabCellSize` when needed to avoid extra
    /// computation.
    if needsUpdate {
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
        lastUpdateAction = .insert
        break
      case .delete:
        indexPathsOfDeletingItems.append(item.indexPathBeforeUpdate!)
        lastUpdateAction = .delete
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
      let attributes: UICollectionViewLayoutAttributes =
        self
        .layoutAttributesForItem(at: itemIndexPath)
    else { return nil }

    if indexPathsOfInsertingItems.contains(itemIndexPath) {
      // Animate the appearing item by starting it with zero opacity and
      // translated down by its height.
      attributes.alpha = 0
      attributes.transform = CGAffineTransform(
        translationX: 0,
        y: attributes.frame.size.height
      )
    }

    return attributes
  }

  override func finalLayoutAttributesForDisappearingItem(at itemIndexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let attributes: UICollectionViewLayoutAttributes =
        self
        .layoutAttributesForItem(at: itemIndexPath)
    else { return nil }
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

  override func layoutAttributesForItem(at indexPath: IndexPath)
    -> UICollectionViewLayoutAttributes?
  {
    guard
      let layoutAttributes = super.layoutAttributesForItem(at: indexPath),
      let collectionView = collectionView
    else { return nil }

    /// Early return the updated `selectedAttributes` if the cell is selected.
    if let selectedAttributes = self.layoutAttributesForSelectedCell(
      layoutAttributes: layoutAttributes)
    {
      return selectedAttributes
    }

    guard
      let cell = collectionView.cellForItem(at: indexPath) as? TabStripCell
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

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]?
  {
    guard
      let collectionView = collectionView
    else { return nil }

    /// To ensure the proper positioning of the selected cell and the
    /// disappearing cells, compute the `attribute` of each cells.
    var computedAttributes: [UICollectionViewLayoutAttributes] = []
    for section in 0..<collectionView.numberOfSections {
      for item in 0..<collectionView.numberOfItems(inSection: section) {
        if let attribute = layoutAttributesForItem(at: IndexPath(item: item, section: section)) {
          computedAttributes.append(attribute)
        }
      }
    }

    return computedAttributes
  }

  override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
    return true
  }

  // MARK: - Private

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

    if let cell = collectionView.cellForItem(at: indexPath) as? TabStripCell {
      // Update cell separators.
      cell.leadingSeparatorHidden = true
      cell.trailingSeparatorHidden = true
      cell.leadingSeparatorGradientViewHidden = true
      cell.trailingSeparatorGradientViewHidden = true
    }

    let collectionViewWidth = collectionView.bounds.size.width
    let horizontalOffset = collectionView.contentOffset.x
    let frame = layoutAttributes.frame
    var origin = layoutAttributes.frame.origin
    var horizontalInset: CGFloat = 0

    // If the collection view is scrollable, add an horizontal inset to its origin.
    if collectionView.contentSize.width > collectionView.frame.width {
      horizontalInset = TabStripConstants.TabItem.horizontalSelectedInset
    }

    // Update the cell's origin horizontally to prevent it from being
    // partially hidden off-screen.

    // Check the left side.
    let minOringin = horizontalOffset + sectionInset.left + horizontalInset
    origin.x = max(origin.x, minOringin)
    // Check the right side.
    let maxOrigin =
      horizontalOffset + collectionViewWidth - frame.size.width - sectionInset.right
      - horizontalInset
    origin.x = min(origin.x, maxOrigin)

    layoutAttributes.frame = CGRect(origin: origin, size: frame.size)
    return layoutAttributes
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

  // Calculates the dynamic size of a tab according to the number of tabs and
  // groups.
  private func calculateTabCellSize() {
    guard let collectionView = self.collectionView, let snapshot = dataSource?.snapshot() else {
      return
    }

    let groupCellWidthSum: CGFloat = 0
    var tabCellCount: CGFloat = 0
    let cellCount: CGFloat = CGFloat(snapshot.itemIdentifiers.count)

    if cellCount == 0 {
      return
    }

    for _ in snapshot.itemIdentifiers {
      // TODO(crbug.com/1509342): Handle tab group item.
      tabCellCount += 1
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

  // MARK: - Public

  public func calculcateCellSize(indexPath: IndexPath) -> CGSize {
    // TODO(crbug.com/1509342): Handle tab group item.
    return tabCellSize
  }

}
