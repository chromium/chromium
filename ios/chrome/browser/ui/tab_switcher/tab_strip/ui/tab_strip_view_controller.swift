// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// View Controller displaying the TabStrip.
@objcMembers
class TabStripViewController: UIViewController, TabStripCellDelegate,
  TabStripConsumer, TabStripNewTabButtonDelegate
{

  // The enum used by the data source to manage the sections.
  enum Section: Int {
    case tabs
  }

  private let layout: TabStripLayout
  // The CollectionView used to display the items.
  private let collectionView: UICollectionView
  // The DataSource for this collection view.
  private var diffableDataSource: UICollectionViewDiffableDataSource<Section, TabSwitcherItem>?
  private var tabCellRegistration: UICollectionView.CellRegistration<TabStripCell, TabSwitcherItem>?

  // The New tab button.
  private let newTabButton: TabStripNewTabButton = TabStripNewTabButton(frame: .zero)

  // Decoration views that encapsulate the collection view. They are visible
  // when the collection view can be scrolled.
  private let leadingSeparatorView: TabStripDecorationView = TabStripDecorationView(frame: .zero)
  private let trailingSeparatorView: TabStripDecorationView = TabStripDecorationView(frame: .zero)

  // Lastest dragged item. This property is set when the item
  // is long pressed which does not always result in a drag action.
  private var draggedItem: TabSwitcherItem?

  // Handles model updates.
  public weak var mutator: TabStripMutator?
  // Tab strip  delegate.
  public weak var delegate: TabStripViewControllerDelegate?
  // Handles drag and drop interactions.
  public weak var dragDropHandler: TabCollectionDragDropHandler?

  init() {
    layout = TabStripLayout()
    collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
    super.init(nibName: nil, bundle: nil)

    collectionView.delegate = self
    collectionView.dragDelegate = self
    collectionView.dropDelegate = self
    collectionView.showsHorizontalScrollIndicator = false

    createRegistrations()
    diffableDataSource = UICollectionViewDiffableDataSource<Section, TabSwitcherItem>(
      collectionView: collectionView
    ) {
      (collectionView: UICollectionView, indexPath: IndexPath, itemIdentifier: TabSwitcherItem)
        -> UICollectionViewCell? in
      return self.getCell(
        collectionView: collectionView, indexPath: indexPath, itemIdentifier: itemIdentifier)
    }

    layout.dataSource = diffableDataSource
    layout.leadingSeparatorView = leadingSeparatorView
    layout.trailingSeparatorView = trailingSeparatorView
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = UIColor(named: kGrey200Color)

    collectionView.translatesAutoresizingMaskIntoConstraints = false
    collectionView.clipsToBounds = true
    view.layer.masksToBounds = true

    collectionView.backgroundColor = .clear
    view.addSubview(collectionView)

    // Mirror the layer.
    trailingSeparatorView.transform = CGAffineTransformMakeScale(-1, 1)
    view.addSubview(leadingSeparatorView)
    view.addSubview(trailingSeparatorView)

    newTabButton.delegate = self
    view.addSubview(newTabButton)

    NSLayoutConstraint.activate([
      /// `collectionView` constraints.
      collectionView.leadingAnchor.constraint(
        equalTo: view.leadingAnchor),
      collectionView.topAnchor.constraint(
        equalTo: view.topAnchor),
      collectionView.bottomAnchor.constraint(equalTo: view.bottomAnchor),

      /// `newTabButton` constraints.
      newTabButton.leadingAnchor.constraint(
        equalTo: collectionView.trailingAnchor),
      newTabButton.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      newTabButton.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      newTabButton.topAnchor.constraint(equalTo: view.topAnchor),
      newTabButton.widthAnchor.constraint(equalToConstant: TabStripConstants.NewTabButton.width),

      /// `leadingSeparatorView` constraints.
      leadingSeparatorView.leadingAnchor.constraint(equalTo: collectionView.leadingAnchor),
      leadingSeparatorView.bottomAnchor.constraint(
        equalTo: collectionView.bottomAnchor),

      /// `trailingSeparatorView` constraints.
      trailingSeparatorView.trailingAnchor.constraint(equalTo: collectionView.trailingAnchor),
      trailingSeparatorView.bottomAnchor.constraint(
        equalTo: collectionView.bottomAnchor),
    ])
  }

  // MARK: - TabStripConsumer

  func populate(items: [TabSwitcherItem]?, selectedItem: TabSwitcherItem?) {
    guard let items = items else {
      return
    }
    var snapshot = NSDiffableDataSourceSnapshot<Section, TabSwitcherItem>()
    snapshot.appendSections([.tabs])
    snapshot.appendItems(items, toSection: .tabs)

    applySnapshot(
      diffableDataSource: diffableDataSource, snapshot: snapshot, animatingDifferences: true)
    selectItem(selectedItem)

    /// Scroll to the end of the collection view if an item has been added.
    if layout.lastUpdateAction == .insert {
      let isRTL: Bool = self.collectionView.effectiveUserInterfaceLayoutDirection == .rightToLeft
      if !isRTL {
        let scrollOffset = self.collectionView.contentSize.width - self.collectionView.frame.width
        if scrollOffset > 0 {
          self.collectionView.setContentOffset(
            CGPoint(x: scrollOffset, y: 0),
            animated: true)
        }
      }
    }
  }

  func selectItem(_ item: TabSwitcherItem?) {
    layout.selectedIndexPath = nil
    if let indexPaths = collectionView.indexPathsForSelectedItems {
      for indexPath in indexPaths {
        collectionView.deselectItem(at: indexPath, animated: false)
      }
    }
    guard
      let item = item, let diffableDataSource = diffableDataSource,
      let indexPath = diffableDataSource.indexPath(for: item)
    else { return }
    layout.selectedIndexPath = indexPath

    collectionView.selectItem(at: indexPath, animated: false, scrollPosition: [])

    /// Invalidate the layout to correctly recalculate the frame of the `selected` cell.
    collectionView.collectionViewLayout.invalidateLayout()
  }

  func reloadItem(_ item: TabSwitcherItem?) {
    guard let item = item, let diffableDataSource = diffableDataSource else {
      return
    }

    var snapshot = diffableDataSource.snapshot()
    snapshot.reconfigureItems([item])
    applySnapshot(
      diffableDataSource: diffableDataSource, snapshot: snapshot, animatingDifferences: false)
  }

  func replaceItem(_ oldItem: TabSwitcherItem?, withItem newItem: TabSwitcherItem?) {
    guard let oldItem = oldItem, let newItem = newItem, let diffableDataSource = diffableDataSource
    else {
      return
    }

    var snapshot = diffableDataSource.snapshot()
    snapshot.insertItems([newItem], beforeItem: oldItem)
    snapshot.deleteItems([oldItem])
    applySnapshot(
      diffableDataSource: diffableDataSource, snapshot: snapshot, animatingDifferences: false)
  }

  // MARK: - TabStripCellDelegate

  func closeButtonTapped(for cell: TabStripCell?) {
    guard let cell = cell, let diffableDataSource = diffableDataSource else {
      return
    }

    guard let indexPath = collectionView.indexPath(for: cell) else {
      return
    }
    let item = diffableDataSource.itemIdentifier(for: indexPath)
    mutator?.close(item)
  }

  // MARK: - Private

  /// Applies `snapshot` to `diffableDataSource` and updates the collection view layout.
  private func applySnapshot(
    diffableDataSource: UICollectionViewDiffableDataSource<Section, TabSwitcherItem>?,
    snapshot: NSDiffableDataSourceSnapshot<Section, TabSwitcherItem>,
    animatingDifferences: Bool = false
  ) {
    layout.needsUpdate = true
    diffableDataSource?.apply(snapshot, animatingDifferences: animatingDifferences)
    layout.needsUpdate = false

    updateVisibleCellIdentifiers()
  }

  /// Creates the registrations of the different cells used in the collection view.
  private func createRegistrations() {
    tabCellRegistration = UICollectionView.CellRegistration<TabStripCell, TabSwitcherItem> {
      (cell, indexPath, item) in
      cell.setTitle(item.title)
      cell.loading = item.showsActivity
      cell.delegate = self
      cell.accessibilityIdentifier = self.tabTripCellAccessibilityIdentifier(index: indexPath.item)

      weak var weakSelf = self
      item.fetchFavicon { (item: TabSwitcherItem?, image: UIImage?) -> Void in
        guard let item = item,
          let diffableDataSource = weakSelf?.diffableDataSource,
          let indexPath = weakSelf?.collectionView.indexPath(for: cell)
        else {
          // If the cell is not visible, nothing is needed.
          return
        }
        let innerItem = diffableDataSource.itemIdentifier(for: indexPath)
        if innerItem == item {
          cell.setFaviconImage(image)
        }
      }
    }

    // UICollectionViewDropPlaceholder uses a TabStripCell and needs the class to be
    // registered.
    collectionView.register(
      TabStripCell.self,
      forCellWithReuseIdentifier: TabStripConstants.CollectionView.tabStripCellReuseIdentifier)
  }

  /// Retuns the cell to be used in the collection view.
  private func getCell(
    collectionView: UICollectionView, indexPath: IndexPath, itemIdentifier: TabSwitcherItem
  ) -> UICollectionViewCell? {
    let sectionIdentifier = diffableDataSource?.sectionIdentifier(for: indexPath.section)
    guard let sectionIdentifier = sectionIdentifier, let tabCellRegistration = tabCellRegistration
    else {
      return nil
    }
    switch sectionIdentifier {
    case .tabs:
      return collectionView.dequeueConfiguredReusableCell(
        using: tabCellRegistration,
        for: indexPath,
        item: itemIdentifier)
    }
  }

  /// Returns a UIMenu for the context menu to be displayed at `indexPath`.
  private func contextMenuForIndexPath(_ indexPath: IndexPath) -> UIMenu {
    let selectedItem = diffableDataSource?.itemIdentifier(for: indexPath)

    let actionFactory = ActionFactory(scenario: kMenuScenarioHistogramTabStripEntry)
    weak var weakSelf = self

    let close = actionFactory?.actionToCloseRegularTab {
      weakSelf?.mutator?.close(selectedItem)
    }
    let closeOthers = actionFactory?.actionToCloseAllOtherTabs {
      weakSelf?.mutator?.closeAllItemsExcept(selectedItem)
    }
    let share = actionFactory?.actionToShare {
      let cell = weakSelf?.collectionView.cellForItem(at: indexPath)
      weakSelf?.delegate?.tabStrip(weakSelf, shareItem: selectedItem, originView: cell)
    }

    guard let close = close, let closeOthers = closeOthers, let share = share else {
      return UIMenu()
    }

    let closeActions = UIMenu(options: .displayInline, children: [close, closeOthers])

    return UIMenu(children: [share, closeActions])
  }

  // Update visible cells identifier, following a reorg of cells.
  func updateVisibleCellIdentifiers() {
    for indexPath in collectionView.indexPathsForVisibleItems {
      if let cell = collectionView.cellForItem(at: indexPath) {
        cell.accessibilityIdentifier = tabTripCellAccessibilityIdentifier(index: indexPath.item)
      }
    }
  }

  // Returns the accessibility identifier to set on a TabStripCell when
  // positioned at the given index.
  func tabTripCellAccessibilityIdentifier(index: Int) -> String {
    return String(
      format: "%@%ld", TabStripConstants.CollectionView.tabStripCellPrefixIdentifier, index)
  }

  // MARK: - TabStripNewTabButtonDelegate

  @objc func newTabButtonTapped() {
    mutator?.addNewItem()
  }

}

// MARK: - UICollectionViewDelegateFlowLayout

extension TabStripViewController: UICollectionViewDelegateFlowLayout {

  func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
    if #available(iOS 16, *) {
    } else {
      self.collectionView(collectionView, performPrimaryActionForItemAt: indexPath)
    }
  }

  func collectionView(
    _ collectionView: UICollectionView, performPrimaryActionForItemAt indexPath: IndexPath
  ) {
    guard let item = diffableDataSource?.itemIdentifier(for: indexPath) else {
      return
    }
    mutator?.activate(item)
  }

  func collectionView(
    _ collectionView: UICollectionView,
    contextMenuConfigurationForItemAt indexPath: IndexPath,
    point: CGPoint
  ) -> UIContextMenuConfiguration? {
    return self.collectionView(
      collectionView, contextMenuConfigurationForItemsAt: [indexPath], point: point)
  }

  func collectionView(
    _ collectionView: UICollectionView, contextMenuConfigurationForItemsAt indexPaths: [IndexPath],
    point: CGPoint
  ) -> UIContextMenuConfiguration? {
    if indexPaths.count != 1 {
      return nil
    }

    weak var weakSelf = self
    return UIContextMenuConfiguration(actionProvider: { suggestedActions in
      return weakSelf?.contextMenuForIndexPath(indexPaths[0])
    })
  }

  func collectionView(
    _ collectionView: UICollectionView,
    layout collectionViewLayout: UICollectionViewLayout,
    sizeForItemAt indexPath: IndexPath
  ) -> CGSize {
    return layout.calculcateCellSize(indexPath: indexPath)
  }

}

extension TabStripViewController: UICollectionViewDragDelegate, UICollectionViewDropDelegate {
  // MARK: - UICollectionViewDragDelegate

  func collectionView(
    _ collectionView: UICollectionView,
    dragSessionWillBegin session: UIDragSession
  ) {
    dragDropHandler?.dragWillBegin(for: draggedItem)
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragSessionDidEnd session: UIDragSession
  ) {
    dragDropHandler?.dragSessionDidEnd()
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragPreviewParametersForItemAt indexPath: IndexPath
  ) -> UIDragPreviewParameters? {
    guard let draggedCell = (collectionView.cellForItem(at: indexPath) as? TabStripCell) else {
      return nil
    }
    return draggedCell.dragPreviewParameters
  }

  func collectionView(
    _ collectionView: UICollectionView,
    itemsForBeginning session: UIDragSession,
    at indexPath: IndexPath
  ) -> [UIDragItem] {
    guard let item = diffableDataSource?.itemIdentifier(for: indexPath),
      let dragItem = dragDropHandler?.dragItem(for: item)
    else {
      return []
    }
    draggedItem = item
    return [dragItem]
  }

  func collectionView(
    _ collectionView: UICollectionView,
    itemsForAddingTo session: UIDragSession,
    at indexPath: IndexPath,
    point: CGPoint
  ) -> [UIDragItem] {
    // Prevent more items from getting added to the drag session.
    return []
  }

  // MARK: - UICollectionViewDropDelegate

  func collectionView(
    _ collectionView: UICollectionView,
    dropSessionDidUpdate session: UIDropSession,
    withDestinationIndexPath destinationIndexPath: IndexPath?
  ) -> UICollectionViewDropProposal {
    guard let dropOperation: UIDropOperation = dragDropHandler?.dropOperation(for: session) else {
      return UICollectionViewDropProposal(operation: .cancel)
    }
    return UICollectionViewDropProposal(
      operation: dropOperation, intent: .insertAtDestinationIndexPath)
  }

  func collectionView(
    _ collectionView: UICollectionView,
    performDropWith coordinator: UICollectionViewDropCoordinator
  ) {
    for item in coordinator.items {
      // Append to the end of the collection, unless drop index is specified.
      // The sourceIndexPath is nil if the drop item is not from the same
      // collection view. Set the destinationIndex to reflect the addition of an
      // item.
      let tabsCount = collectionView.numberOfItems(inSection: 0)
      var destinationIndex = item.sourceIndexPath != nil ? (tabsCount - 1) : tabsCount
      if let destinationIndexPath = coordinator.destinationIndexPath {
        destinationIndex = destinationIndexPath.item
      }
      let dropIndexPah: IndexPath = IndexPath(item: destinationIndex, section: 0)

      // Drop synchronously if local object is available.
      if item.dragItem.localObject != nil {
        coordinator.drop(item.dragItem, toItemAt: dropIndexPah)
        // The sourceIndexPath is non-nil if the drop item is from this same
        // collection view.
        self.dragDropHandler?.drop(
          item.dragItem, to: UInt(destinationIndex),
          fromSameCollection: (item.sourceIndexPath != nil))
      } else {
        // Drop asynchronously if local object is not available.
        let placeholder: UICollectionViewDropPlaceholder = UICollectionViewDropPlaceholder(
          insertionIndexPath: dropIndexPah,
          reuseIdentifier: TabStripConstants.CollectionView.tabStripCellReuseIdentifier)
        placeholder.previewParametersProvider = {
          (placeholderCell: UICollectionViewCell) -> UIDragPreviewParameters? in
          guard let tabStripCell = placeholderCell as? TabStripCell else {
            return nil
          }
          return tabStripCell.dragPreviewParameters
        }

        let context: UICollectionViewDropPlaceholderContext = coordinator.drop(
          item.dragItem, to: placeholder)
        self.dragDropHandler?.dropItem(
          from: item.dragItem.itemProvider, to: UInt(destinationIndex), placeholderContext: context)
      }
    }
  }

}
