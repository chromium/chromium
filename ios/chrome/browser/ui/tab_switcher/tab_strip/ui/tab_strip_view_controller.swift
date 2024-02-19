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
  private let newTabButton: TabStripNewTabButton = TabStripNewTabButton()

  // Static decoration views that border the collection view. They are
  // visible when the selected cell reaches an edge of the collection view and
  // if the collection view can be scrolled.
  private let leftStaticSeparator: TabStripDecorationView = TabStripDecorationView()
  private let rightStaticSeparator: TabStripDecorationView = TabStripDecorationView()

  // Lastest dragged item. This property is set when the item
  // is long pressed which does not always result in a drag action.
  private var draggedItem: TabSwitcherItem?

  // The item currently selected in the tab strip.
  // The collection view appears to sometimes forget what item is selected,
  // so it is better to store this information here rather than directly using
  // `collectionView.selectItem:` and `collectionView.deselectItem:`.
  // `self.ensureSelectedItemIsSelected()` is used to ensure the value of
  // `collectionView.indexPathsForSelectedItems` remains consistent with `selectedItem`.
  private var selectedItem: TabSwitcherItem? {
    didSet { self.ensureSelectedItemIsSelected() }
  }

  /// `true` if the new tab button has been tapped. Used to scroll to the newly
  /// added item. It's automatically set to `false` after the scroll.
  private var newTabOpened: Bool = false

  /// `true` if the dragged tab moved to a new index.
  private var dragEndAtNewIndex: Bool = false

  /// `true` if a drop animation is in progress.
  private var dropAnimationInProgress: Bool = false

  // Handles model updates.
  public weak var mutator: TabStripMutator?
  // Tab strip  delegate.
  public weak var delegate: TabStripViewControllerDelegate?
  // Handles drag and drop interactions.
  public weak var dragDropHandler: TabCollectionDragDropHandler?

  /// Targeted scroll offset, used on iOS 16 only.
  /// On iOS 16, the scroll animation after opening a new tab is delayed.
  /// This variable ensures that the most recent scroll event is processed.
  private var targetedScrollOffsetiOS16: CGFloat = 0

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
    layout.leftStaticSeparator = leftStaticSeparator
    layout.rightStaticSeparator = rightStaticSeparator
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = UIColor(named: kGroupedPrimaryBackgroundColor)

    collectionView.translatesAutoresizingMaskIntoConstraints = false
    collectionView.clipsToBounds = true
    view.layer.masksToBounds = true

    collectionView.backgroundColor = .clear
    view.addSubview(collectionView)

    // Mirror the layer.
    rightStaticSeparator.transform = CGAffineTransformMakeScale(-1, 1)
    view.addSubview(leftStaticSeparator)
    view.addSubview(rightStaticSeparator)

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

      /// `leftStaticSeparator` constraints.
      leftStaticSeparator.leftAnchor.constraint(equalTo: collectionView.leftAnchor),
      leftStaticSeparator.bottomAnchor.constraint(
        equalTo: collectionView.bottomAnchor,
        constant: -TabStripConstants.StaticSeparator.bottomInset),
      /// `rightStaticSeparator` constraints.
      rightStaticSeparator.rightAnchor.constraint(equalTo: collectionView.rightAnchor),
      rightStaticSeparator.bottomAnchor.constraint(
        equalTo: collectionView.bottomAnchor,
        constant: -TabStripConstants.StaticSeparator.bottomInset),
    ])
  }

  override func viewWillTransition(
    to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator
  ) {
    super.viewWillTransition(to: size, with: coordinator)
    weak var weakSelf = self
    coordinator.animate(alongsideTransition: nil) { _ in
      // The tab cell size must be updated after the transition completes.
      // Otherwise the collection view width won't be updated.
      weakSelf?.layout.calculateTabCellSize()
      weakSelf?.layout.invalidateLayout()
    }
  }

  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    self.ensureSelectedItemIsSelected()
  }

  // MARK: - TabStripConsumer

  func populate(items: [TabSwitcherItem]?, selectedItem: TabSwitcherItem?) {
    guard let items = items else {
      return
    }
    var snapshot = NSDiffableDataSourceSnapshot<Section, TabSwitcherItem>()
    snapshot.appendSections([.tabs])
    snapshot.appendItems(items, toSection: .tabs)

    // TODO(crbug.com/325415449): Update this when #unavailable is rocognized by
    // the formatter.
    if #available(iOS 17.0, *) {
    } else {
      layout.cellAnimatediOS16 = true
    }

    // To make the animation smoother, try to select the item if it's already
    // present in the collection view.
    selectItem(selectedItem)
    applySnapshot(
      diffableDataSource: diffableDataSource, snapshot: snapshot, animatingDifferences: true)
    selectItem(selectedItem)

    /// Scroll to the end of the collection view if a new tab has been opened.
    if newTabOpened {
      newTabOpened = false

      // Don't scroll to the end of the collection view in RTL.
      let isRTL: Bool = self.collectionView.effectiveUserInterfaceLayoutDirection == .rightToLeft
      if isRTL { return }

      let offset = self.collectionView.contentSize.width - self.collectionView.frame.width
      if offset > 0 {
        if #available(iOS 17.0, *) {
          scrollToContentOffset(offset)
        } else {
          // On iOS 16, when the scroll animation and the insert animation
          // occur simultaneously, the resulting animation lacks of
          // smoothness.
          weak var weakSelf = self
          targetedScrollOffsetiOS16 = offset
          DispatchQueue.main.asyncAfter(
            deadline: .now() + TabStripConstants.CollectionView.scrollDelayAfterInsert
          ) {
            weakSelf?.scrollToContentOffset(offset)
          }
        }
      } else {
        layout.cellAnimatediOS16 = false
      }
    }
  }

  func selectItem(_ item: TabSwitcherItem?) {
    self.selectedItem = item
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

  // MARK: - UIScrollViewDelegate

  func scrollViewDidEndDragging(
    _ scrollView: UIScrollView,
    willDecelerate decelerate: Bool
  ) {
    layout.cellAnimatediOS16 = false
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

  /// Scrolls the collection view to the given horizontal `offset`.
  func scrollToContentOffset(_ offset: CGFloat) {
    // TODO(crbug.com/325415449): Update this when #unavailable is rocognized by
    // the formatter.
    if #available(iOS 17.0, *) {
    } else {
      if offset != targetedScrollOffsetiOS16 { return }
    }
    self.collectionView.setContentOffset(
      CGPoint(x: offset, y: 0),
      animated: true)
  }

  /// Ensures `collectionView.indexPathsForSelectedItems` is consistent with
  /// `self.selectedItem`.
  func ensureSelectedItemIsSelected() {
    guard let diffableDataSource = diffableDataSource else {
      return
    }

    let expectedIndexPathForSelectedItem =
      self.selectedItem.map { diffableDataSource.indexPath(for: $0) }
    let observedIndexPathForSelectedItem = collectionView.indexPathsForSelectedItems?.first

    // If the observed selected indexPath doesn't match the expected selected
    // indexPath, update the observed selected item.
    if expectedIndexPathForSelectedItem != observedIndexPathForSelectedItem {
      // Clear the selection.
      if let indexPaths = collectionView.indexPathsForSelectedItems {
        for indexPath in indexPaths {
          collectionView.deselectItem(at: indexPath, animated: false)
        }
      }

      // If `expectedIndexPathForSelectedItem` is not nil, select it.
      guard let expectedIndexPathForSelectedItem = expectedIndexPathForSelectedItem else { return }
      collectionView.selectItem(
        at: expectedIndexPathForSelectedItem, animated: false, scrollPosition: [])
    }

    /// Invalidate the layout to correctly recalculate the frame of the `selected` cell.
    layout.invalidateLayout()
  }

  // MARK: - TabStripNewTabButtonDelegate

  @objc func newTabButtonTapped() {
    UserMetricsUtils.recordAction("MobileTabSwitched")
    UserMetricsUtils.recordAction("MobileTabStripNewTab")

    newTabOpened = true
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
    contextMenuConfiguration configuration: UIContextMenuConfiguration,
    highlightPreviewForItemAt indexPath: IndexPath
  ) -> UITargetedPreview? {
    guard let cell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return UITargetedPreview(view: cell, parameters: cell.dragPreviewParameters)
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
    dragSessionIsRestrictedToDraggingApplication session: UIDragSession
  ) -> Bool {
    // Needed to avoid triggering new Chrome window opening when dragging
    // an item close to an edge of the collection view.
    // Dragged item can still be dropped in another Chrome window.
    return true
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragSessionWillBegin session: UIDragSession
  ) {
    dragEndAtNewIndex = false
    HistogramUtils.recordHistogram(
      kUmaTabStripViewDragDropTabs, withSample: DragDropTabs.dragBegin.rawValue,
      maxValue: DragDropTabs.maxValue.rawValue)
    dragDropHandler?.dragWillBegin(for: draggedItem)
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragSessionDidEnd session: UIDragSession
  ) {
    var dragEvent =
      dragEndAtNewIndex
      ? DragDropTabs.dragEndAtNewIndex
      : DragDropTabs.dragEndAtSameIndex

    // If a drop animation is in progress and the drag didn't end at a new index,
    // that means the item has been dropped outside of its collection view.
    if dropAnimationInProgress && !dragEndAtNewIndex {
      dragEvent = DragDropTabs.dragEndInOtherCollection
    }

    HistogramUtils.recordHistogram(
      kUmaTabStripViewDragDropTabs, withSample: dragEvent.rawValue,
      maxValue: DragDropTabs.maxValue.rawValue)

    dragDropHandler?.dragSessionDidEnd()
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragPreviewParametersForItemAt indexPath: IndexPath
  ) -> UIDragPreviewParameters? {
    guard let cell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return cell.dragPreviewParameters
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
    /// Use `insertIntoDestinationIndexPath` if the dragged item is not from the same
    /// collection view. This prevents having unwanted empty space in the collection view.
    return UICollectionViewDropProposal(
      operation: dropOperation,
      intent: dropOperation == .move
        ? .insertAtDestinationIndexPath : .insertIntoDestinationIndexPath)
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
      dragEndAtNewIndex = true

      // Drop synchronously if local object is available.
      if item.dragItem.localObject != nil {
        weak var weakSelf = self
        coordinator.drop(item.dragItem, toItemAt: dropIndexPah).addCompletion {
          UIViewAnimatingPosition in
          weakSelf?.dropAnimationInProgress = false
        }
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
