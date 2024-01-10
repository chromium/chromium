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

  // Separator views for the collection view.
  public var leadingSeparatorView: TabStripSeparatorView?
  public var trailingSeparatorView: TabStripSeparatorView?

  /// Dynamic size of a tab.
  private var tabCellSize: CGSize = .zero

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
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - UICollectionViewLayout

  override func prepare() {
    /// Display collection view separators if the `contentSize` of the
    /// `collectionView` is bigger than its frame.
    guard let leadingSeparatorView = leadingSeparatorView,
      let trailingSeparatorView = trailingSeparatorView,
      let collectionView = collectionView
    else { return }
    let hideSeparator: Bool = collectionView.contentSize.width <= collectionView.frame.width
    leadingSeparatorView.isHidden = hideSeparator
    trailingSeparatorView.isHidden = hideSeparator

    /// Only recalculate the `tabCellSize` when needed to avoid extra
    /// computation.
    if needsUpdate {
      calculateTabCellSize()
    }
    super.prepare()
  }

  override func prepare(forCollectionViewUpdates updateItems: [UICollectionViewUpdateItem]) {
    super.prepare(forCollectionViewUpdates: updateItems)

    /// Keeps track of updated items to animate their transition.
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
        .initialLayoutAttributesForAppearingItem(at: itemIndexPath)
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
      let attributes: UICollectionViewLayoutAttributes = super
        .finalLayoutAttributesForDisappearingItem(at: itemIndexPath)
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
    guard
      let cell = collectionView.cellForItem(at: indexPath) as? TabStripCell
    else { return layoutAttributes }

    let contentOffset = collectionView.contentOffset
    var frame = layoutAttributes.frame
    let collectionViewSizeWidth = collectionView.bounds.size.width

    // The selected cell should remain on top of other cells within collection
    // view's bounds.
    if indexPath == selectedIndexPath {
      // Update cell separators.
      cell.leadingSeparatorHidden = true
      cell.trailingSeparatorHidden = true

      var origin = layoutAttributes.frame.origin

      // Update the cell's origin horizontally to prevent it from being
      // partially hidden off-screen.
      let maxOriginX = collectionViewSizeWidth - frame.size.width
      // Check the left side.
      origin.x = max(origin.x, contentOffset.x)
      // Check the right side.
      origin.x = min(origin.x, contentOffset.x + maxOriginX)

      layoutAttributes.frame = CGRect(origin: origin, size: frame.size)
      return layoutAttributes
    }

    // Update cell separators.
    cell.leadingSeparatorHidden = true
    // Hide the trailing separator if the next cell is selected.
    let isNextCellSelected = (indexPath.item + 1) == selectedIndexPath?.item
    cell.trailingSeparatorHidden = isNextCellSelected

    let isRTL: Bool = collectionView.effectiveUserInterfaceLayoutDirection == .rightToLeft

    // Recalculate the cell width and origin when it intersects with the left
    // collection view's bounds. The cell should collapse within the collection
    // view's bounds until its width reaches 0.
    if frame.minX < contentOffset.x && contentOffset.x < frame.maxX {
      if !isRTL {
        cell.leadingSeparatorHidden = false
      }
      frame.origin.x = max(contentOffset.x, frame.origin.x)
      let offsetLeft: CGFloat = abs(frame.origin.x - layoutAttributes.frame.origin.x)
      frame.size.width = min(frame.width - offsetLeft, frame.width)
    }

    let offsetRight: CGFloat = collectionViewSizeWidth + contentOffset.x

    // Recalculate the cell width when it intersects with the left collection
    // view's bounds. The cell should collapse within the collection view's
    // bounds until its width reaches 0.
    if frame.minX < offsetRight && offsetRight < frame.maxX {
      if isRTL {
        cell.leadingSeparatorHidden = false
      }
      frame.size.width = min(offsetRight - frame.origin.x, frame.size.width)
    }

    layoutAttributes.frame = frame
    return layoutAttributes
  }

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]?
  {
    guard
      var computedAttributes = super.layoutAttributesForElements(in: rect)
    else { return nil }

    computedAttributes = computedAttributes.compactMap { layoutAttribute in
      layoutAttributesForItem(at: layoutAttribute.indexPath)
    }

    // Explicitly update the layout of the selected item.
    guard let selectedIndexPath = selectedIndexPath else { return computedAttributes }
    if let attr = layoutAttributesForItem(at: selectedIndexPath) {
      computedAttributes.append(attr)
    }

    return computedAttributes
  }

  override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
    return true
  }

  // MARK: - Private

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
    let itemSpacingSum: CGFloat = minimumLineSpacing * (cellCount - 1)

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
