// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit
import ios_chrome_browser_shared_ui_util_util_swift
import ios_chrome_browser_ui_tab_switcher_tab_strip_ui_swift_constants

/// View Controller displaying the TabStrip.
@objcMembers
class TabStripViewController: UIViewController,
  TabStripConsumer, TabStripNewTabButtonDelegate, TabStripGroupCellDelegate, TabStripTabCellDelegate
{

  // The enum used by the data source to manage the sections.
  enum Section: Int {
    case tabs
  }

  private let layout: TabStripLayout
  // The CollectionView used to display the items.
  private let collectionView: UICollectionView
  // The DataSource for this collection view.
  private lazy var dataSource = createDataSource(
    tabCellRegistration: tabCellRegistration, groupCellRegistration: groupCellRegistration)
  private lazy var tabCellRegistration = createTabCellRegistration()
  private lazy var groupCellRegistration = createGroupCellRegistration()

  // Associates data to `dataSource` items, to reconfigure cells.
  private let itemData = NSMutableDictionary()

  // The New tab button.
  private let newTabButton: TabStripNewTabButton = TabStripNewTabButton()

  // Static decoration views that border the collection view. They are
  // visible when the selected cell reaches an edge of the collection view and
  // if the collection view can be scrolled.
  private let leadingStaticSeparator = TabStripDecorationView()
  private let trailingStaticSeparator = TabStripDecorationView()

  // Latest dragged item. This property is set when the item
  // is long pressed which does not always result in a drag action.
  private var draggedItemIdentifier: TabStripItemIdentifier?

  // The item currently selected in the tab strip.
  // The collection view appears to sometimes forget what item is selected,
  // so it is better to store this information here rather than directly using
  // `collectionView.selectItem:` and `collectionView.deselectItem:`.
  // `self.ensureSelectedItemIsSelected()` is used to ensure the value of
  // `collectionView.indexPathsForSelectedItems` remains consistent with `selectedItem`.
  private var selectedItem: TabSwitcherItem? {
    didSet { self.ensureSelectedItemIsSelected() }
  }

  /// `true` if the dragged tab moved to a new index.
  private var dragEndAtNewIndex: Bool = false

  /// `true` if a drop animation is in progress.
  private var dropAnimationInProgress: Bool = false

  /// Targeted scroll offset, used on iOS 16 only.
  /// On iOS 16, the scroll animation after opening a new tab is delayed.
  /// This variable ensures that the most recent scroll event is processed.
  private var targetedScrollOffsetiOS16: CGFloat = 0

  /// `true` if the user is in incognito.
  public var isIncognito: Bool = false

  /// A dictionary that maps each tab item identifier to its index,
  /// either in the set of ungrouped tabs or in its group.
  private var tabIndices: [TabStripItemIdentifier: Int] = [:]
  /// A dictionary that maps each group item identifier to the number of tabs it contains.
  private var numberOfTabsPerGroup: [TabStripItemIdentifier: Int] = [:]
  /// The number of tabs that are not part of any group.
  private var numberOfUngroupedTabs = 0

  /// Handles model updates.
  public weak var mutator: TabStripMutator?
  /// Handles drag and drop interactions.
  public weak var dragDropHandler: TabCollectionDragDropHandler?
  /// Provides context menu for tab strip items.
  public weak var contextMenuProvider: TabStripContextMenuProvider?

  /// Handler for tab group confirmation commands.
  public weak var tabGroupConfirmationHandler: TabGroupConfirmationCommands?

  /// The LayoutGuideCenter.
  @objc public var layoutGuideCenter: LayoutGuideCenter? {
    didSet {
      layoutGuideCenter?.reference(view: newTabButton.layoutGuideView, under: kNewTabButtonGuide)
    }
  }

  init() {
    layout = TabStripLayout()
    collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
    super.init(nibName: nil, bundle: nil)

    layout.dataSource = dataSource
    layout.leadingStaticSeparator = leadingStaticSeparator
    layout.trailingStaticSeparator = trailingStaticSeparator
    layout.newTabButton = newTabButton

    collectionView.delegate = self
    collectionView.dragDelegate = self
    collectionView.dropDelegate = self
    collectionView.showsHorizontalScrollIndicator = false
    collectionView.allowsMultipleSelection = false
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = TabStripHelper.backgroundColor

    // Don't clip to bound the collection view to allow the shadow of the long press to be displayed fully.
    // The trailing placeholder will ensure that the cells aren't displayed out of the bounds.
    collectionView.clipsToBounds = false
    collectionView.translatesAutoresizingMaskIntoConstraints = false
    collectionView.backgroundColor = .clear
    view.addSubview(collectionView)

    let trailingPlaceholder = UIView()
    trailingPlaceholder.translatesAutoresizingMaskIntoConstraints = false
    trailingPlaceholder.backgroundColor = view.backgroundColor
    // Add the placeholder above the collection view but below the other elements.
    view.insertSubview(trailingPlaceholder, aboveSubview: collectionView)

    // Mirror the layer.
    trailingStaticSeparator.transform = CGAffineTransformMakeScale(-1, 1)
    view.addSubview(leadingStaticSeparator)
    view.addSubview(trailingStaticSeparator)

    newTabButton.delegate = self
    newTabButton.isIncognito = isIncognito
    view.addSubview(newTabButton)

    if TabStripFeaturesUtils.isModernTabStripNewTabButtonDynamic {
      NSLayoutConstraint.activate([
        collectionView.trailingAnchor.constraint(
          equalTo: view.trailingAnchor, constant: -TabStripConstants.NewTabButton.width),
        newTabButton.leadingAnchor.constraint(
          greaterThanOrEqualTo: view.leadingAnchor),
        newTabButton.trailingAnchor.constraint(lessThanOrEqualTo: view.trailingAnchor),
      ])
    } else {
      NSLayoutConstraint.activate([
        newTabButton.leadingAnchor.constraint(
          equalTo: collectionView.trailingAnchor),
        newTabButton.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      ])
    }

    NSLayoutConstraint.activate(
      [
        /// `trailingPlaceholder` constraints.
        trailingPlaceholder.leadingAnchor.constraint(equalTo: collectionView.trailingAnchor),
        trailingPlaceholder.trailingAnchor.constraint(equalTo: view.trailingAnchor),
        trailingPlaceholder.topAnchor.constraint(equalTo: view.topAnchor),
        trailingPlaceholder.bottomAnchor.constraint(equalTo: view.bottomAnchor),

        /// `collectionView` constraints.
        collectionView.leadingAnchor.constraint(
          equalTo: view.leadingAnchor),
        collectionView.topAnchor.constraint(
          equalTo: view.topAnchor),
        collectionView.bottomAnchor.constraint(equalTo: view.bottomAnchor),

        /// `newTabButton` constraints.
        newTabButton.leadingAnchor.constraint(
          greaterThanOrEqualTo: view.leadingAnchor),
        newTabButton.trailingAnchor.constraint(lessThanOrEqualTo: view.trailingAnchor),
        newTabButton.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        newTabButton.topAnchor.constraint(equalTo: view.topAnchor),
        newTabButton.widthAnchor.constraint(equalToConstant: TabStripConstants.NewTabButton.width),

        /// `leadingStaticSeparator` constraints.
        leadingStaticSeparator.leadingAnchor.constraint(equalTo: collectionView.leadingAnchor),
        leadingStaticSeparator.bottomAnchor.constraint(
          equalTo: collectionView.bottomAnchor,
          constant: -TabStripConstants.StaticSeparator.bottomInset),
        /// `trailingStaticSeparator` constraints.
        trailingStaticSeparator.trailingAnchor.constraint(equalTo: collectionView.trailingAnchor),
        trailingStaticSeparator.bottomAnchor.constraint(
          equalTo: collectionView.bottomAnchor,
          constant: -TabStripConstants.StaticSeparator.bottomInset),
      ])
  }

  override func viewWillTransition(
    to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator
  ) {
    // Dismisses the confirmation dialog for tab group if it's displayed.
    tabGroupConfirmationHandler?.dismissTabGroupConfirmation()

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
    // In case the device orientation was updated while the tab strip was not
    // visible, recalculate the item size.
    layout.needsSizeUpdate = true
    NotificationCenter.default.addObserver(
      self, selector: #selector(voiceOverChanged),
      name: UIAccessibility.voiceOverStatusDidChangeNotification, object: nil)
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    layout.needsSizeUpdate = false
  }

  override func viewDidDisappear(_ animated: Bool) {
    super.viewDidDisappear(animated)
    NotificationCenter.default.removeObserver(
      self, name: UIAccessibility.voiceOverStatusDidChangeNotification, object: nil)
  }

  // MARK: - TabStripConsumer

  func populate(
    items itemIdentifiers: [TabStripItemIdentifier]?, selectedItem: TabSwitcherItem?,
    itemData: [TabStripItemIdentifier: TabStripItemData],
    itemParents: [TabStripItemIdentifier: TabGroupItem]
  ) {
    guard let itemIdentifiers = itemIdentifiers else {
      return
    }
    var snapshot = NSDiffableDataSourceSectionSnapshot<TabStripItemIdentifier>()
    for itemIdentifier in itemIdentifiers {
      switch itemIdentifier.item {
      case .group(let tabGroupItem):
        snapshot.append([itemIdentifier])
        if !tabGroupItem.collapsed {
          snapshot.expand([itemIdentifier])
        }
      case .tab(_):
        snapshot.append([itemIdentifier], to: TabStripItemIdentifier(itemParents[itemIdentifier]))
      }
    }
    self.itemData.setDictionary(itemData)

    // TODO(crbug.com/325415449): Update this when #unavailable is rocognized by
    // the formatter.
    if #available(iOS 17.0, *) {
    } else {
      layout.cellAnimatediOS16 = true
    }

    // To make the animation smoother, try to select the item if it's already
    // present in the collection view.
    selectItem(selectedItem)
    reconfigureItems(itemIdentifiers)
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot,
      animatingDifferences: !UIAccessibility.isReduceMotionEnabled,
      numberOfVisibleItemsChanged: true)
    selectItem(selectedItem)
  }

  func selectItem(_ item: TabSwitcherItem?) {
    self.selectedItem = item
  }

  func reconfigureItems(_ items: [TabStripItemIdentifier]) {
    var snapshot = dataSource.snapshot()
    let itemsInSnapshot = Set(snapshot.itemIdentifiers)
    let itemsToReconfigure = Set(items).intersection(itemsInSnapshot)
    snapshot.reconfigureItems(Array(itemsToReconfigure))
    dataSource.apply(snapshot)
  }

  func moveItem(
    _ itemToMoveIdentifier: TabStripItemIdentifier,
    beforeItem destinationItemIdentifier: TabStripItemIdentifier?
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    let itemIsExpanded = snapshot.isExpanded(itemToMoveIdentifier)
    let childItems = snapshot.snapshot(of: itemToMoveIdentifier).items
    snapshot.delete([itemToMoveIdentifier])
    if let destinationItemIdentifier = destinationItemIdentifier {
      snapshot.insert([itemToMoveIdentifier], before: destinationItemIdentifier)
    } else {
      snapshot.append([itemToMoveIdentifier])
    }
    if itemIsExpanded {
      snapshot.expand([itemToMoveIdentifier])
    }
    if !childItems.isEmpty {
      snapshot.append(childItems, to: itemToMoveIdentifier)
    }
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot, animatingDifferences: true)
    layout.invalidateLayout()
  }

  func moveItem(
    _ itemToMoveIdentifier: TabStripItemIdentifier,
    afterItem destinationItemIdentifier: TabStripItemIdentifier?
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    let itemIsExpanded = snapshot.isExpanded(itemToMoveIdentifier)
    let childItems = snapshot.snapshot(of: itemToMoveIdentifier).items
    snapshot.delete([itemToMoveIdentifier])
    if let destinationItemIdentifier = destinationItemIdentifier {
      snapshot.insert([itemToMoveIdentifier], after: destinationItemIdentifier)
    } else if let firstItemIdentifier = snapshot.items.first {
      snapshot.insert([itemToMoveIdentifier], before: firstItemIdentifier)
    } else {
      snapshot.append([itemToMoveIdentifier])
    }
    if itemIsExpanded {
      snapshot.expand([itemToMoveIdentifier])
    }
    if !childItems.isEmpty {
      snapshot.append(childItems, to: itemToMoveIdentifier)
    }
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot, animatingDifferences: true)
    layout.invalidateLayout()
  }

  func moveItem(
    _ itemToMoveIdentifier: TabStripItemIdentifier, insideGroup parentItem: TabGroupItem
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.delete([itemToMoveIdentifier])
    snapshot.append([itemToMoveIdentifier], to: TabStripItemIdentifier(parentItem))
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot, animatingDifferences: true)
    layout.invalidateLayout()
  }

  func insertItems(
    _ itemIdentifiers: [TabStripItemIdentifier],
    beforeItem destinationItemIdentifier: TabStripItemIdentifier?
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    if let destinationItemIdentifier = destinationItemIdentifier {
      snapshot.insert(itemIdentifiers, before: destinationItemIdentifier)
    } else {
      snapshot.append(itemIdentifiers)
    }
    snapshot.expand(itemIdentifiers.lazy.filter { $0.itemType == .group })
    let insertedLast = snapshot.items.last == itemIdentifiers.last
    insertItemsUsingSnapshot(snapshot, insertedLast: insertedLast)
  }

  func insertItems(
    _ itemIdentifiers: [TabStripItemIdentifier],
    afterItem destinationItemIdentifier: TabStripItemIdentifier?
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    if let destinationItemIdentifier = destinationItemIdentifier {
      snapshot.insert(itemIdentifiers, after: destinationItemIdentifier)
    } else if let firstItemIdentifier = snapshot.items.first {
      snapshot.insert(itemIdentifiers, before: firstItemIdentifier)
    } else {
      snapshot.append(itemIdentifiers)
    }
    snapshot.expand(itemIdentifiers.lazy.filter { $0.itemType == .group })
    let insertedLast = snapshot.items.last == itemIdentifiers.last
    insertItemsUsingSnapshot(snapshot, insertedLast: insertedLast)
  }

  func insertItems(
    _ itemIdentifiers: [TabStripItemIdentifier],
    insideGroup parentItem: TabGroupItem
  ) {
    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.append(itemIdentifiers, to: TabStripItemIdentifier(parentItem))
    let insertedLast = snapshot.items.last == itemIdentifiers.last
    insertItemsUsingSnapshot(snapshot, insertedLast: insertedLast)
  }

  func removeItems(_ items: [TabStripItemIdentifier]?) {
    guard let items = items else { return }

    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.delete(items)
    itemData.removeObjects(forKeys: items)
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot,
      animatingDifferences: !UIAccessibility.isReduceMotionEnabled,
      numberOfVisibleItemsChanged: true)
  }

  func replaceItem(_ oldItem: TabSwitcherItem?, withItem newItem: TabSwitcherItem?) {
    guard let oldItem = TabStripItemIdentifier.tabIdentifier(oldItem),
      let newItem = TabStripItemIdentifier.tabIdentifier(newItem)
    else {
      return
    }

    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.insert([newItem], before: oldItem)
    snapshot.delete([oldItem])
    itemData.removeObject(forKey: oldItem)
    applySnapshot(dataSource: dataSource, snapshot: snapshot)
  }

  func updateItemData(
    _ updatedItemData: [TabStripItemIdentifier: TabStripItemData], reconfigureItems: Bool = false
  ) {
    itemData.addEntries(from: updatedItemData)
    if reconfigureItems {
      self.reconfigureItems(Array(updatedItemData.keys))
    }
  }

  func collapseGroup(_ group: TabGroupItem) {
    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.collapse([TabStripItemIdentifier(group)])
    layout.prepareForItemsCollapsing()
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot, animatingDifferences: true,
      numberOfVisibleItemsChanged: true)
    layout.finalizeItemsCollapsing()
  }

  func expandGroup(_ group: TabGroupItem) {
    var snapshot = dataSource.snapshot(for: .tabs)
    snapshot.expand([TabStripItemIdentifier(group)])
    layout.prepareForItemsExpanding()
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot, animatingDifferences: true,
      numberOfVisibleItemsChanged: true)
    layout.finalizeItemsExpanding()
  }

  // MARK: - Public
  // MARK: - TabStripGroupCellDelegate

  func collapseOrExpandTapped(for cell: TabStripGroupCell?) {
    guard let cell = cell,
      let indexPath = collectionView.indexPath(for: cell)
    else { return }
    collapseOrExpandGroup(at: indexPath)
  }

  // MARK: - TabStripTabCellDelegate

  func closeButtonTapped(for cell: TabStripTabCell?) {
    guard let cell = cell,
      let indexPath = collectionView.indexPath(for: cell),
      let item = dataSource.itemIdentifier(for: indexPath)?.tabSwitcherItem
    else {
      return
    }
    mutator?.close(item)
  }

  // MARK: - UIScrollViewDelegate

  func scrollViewDidEndDragging(
    _ scrollView: UIScrollView,
    willDecelerate decelerate: Bool
  ) {
    layout.cellAnimatediOS16 = false
    UserMetricsUtils.recordAction("MobileTabStripScrollDidEnd")
  }

  // MARK: - Private

  /// Collapses or expands the group at `indexPath`.
  func collapseOrExpandGroup(at indexPath: IndexPath) {
    guard let tabGroupItem = dataSource.itemIdentifier(for: indexPath)?.tabGroupItem else {
      return
    }
    if tabGroupItem.collapsed {
      mutator?.expandGroup(tabGroupItem)
    } else {
      mutator?.collapseGroup(tabGroupItem)
    }
  }

  /// Applies `snapshot` to `dataSource` and updates the collection view layout.
  private func applySnapshot(
    dataSource: UICollectionViewDiffableDataSource<Section, TabStripItemIdentifier>,
    snapshot: NSDiffableDataSourceSectionSnapshot<TabStripItemIdentifier>,
    animatingDifferences: Bool = false,
    numberOfVisibleItemsChanged: Bool = false
  ) {
    if #available(iOS 17.0, *) {
      if numberOfVisibleItemsChanged {
        layout.needsSizeUpdate = true
      }
    } else {
      /// On iOS 16, the layout fails to invalidate when it should.
      /// To fix this, we set `needsSizeUpdate` to `true` whenever a snapshot
      /// is applied.
      layout.needsSizeUpdate = true
    }

    dataSource.apply(snapshot, to: .tabs, animatingDifferences: animatingDifferences)
    layout.needsSizeUpdate = false

    ensureSelectedItemIsSelected()
    updateVisibleCellIdentifiers()
    if UIAccessibility.isVoiceOverRunning {
      updateTabIndices()
      updateVisibleCellTabIndices()
    }
  }

  /// Creates and returns the data source for the collection view.
  private func createDataSource(
    tabCellRegistration: UICollectionView.CellRegistration<TabStripTabCell, TabStripItemIdentifier>,
    groupCellRegistration: UICollectionView.CellRegistration<
      TabStripGroupCell, TabStripItemIdentifier
    >
  ) -> UICollectionViewDiffableDataSource<Section, TabStripItemIdentifier> {
    let dataSource = UICollectionViewDiffableDataSource<Section, TabStripItemIdentifier>(
      collectionView: collectionView
    ) {
      (
        collectionView: UICollectionView, indexPath: IndexPath,
        itemIdentifier: TabStripItemIdentifier
      )
        -> UICollectionViewCell? in
      return Self.getCell(
        collectionView: collectionView, indexPath: indexPath, itemIdentifier: itemIdentifier,
        tabCellRegistration: tabCellRegistration, groupCellRegistration: groupCellRegistration)
    }

    var snapshot = dataSource.snapshot()
    snapshot.appendSections([.tabs])
    dataSource.apply(snapshot)

    return dataSource
  }

  /// Creates the registrations of tab cells used in the collection view.
  private func createTabCellRegistration()
    -> UICollectionView.CellRegistration<TabStripTabCell, TabStripItemIdentifier>
  {
    let tabCellRegistration = UICollectionView.CellRegistration<
      TabStripTabCell, TabStripItemIdentifier
    > {
      (cell, indexPath, itemIdentifier) in
      guard let item = itemIdentifier.tabSwitcherItem else { return }
      let itemData = self.itemData[itemIdentifier] as? TabStripItemData
      cell.title = item.title
      cell.groupStrokeColor = itemData?.groupStrokeColor
      cell.isFirstTabInGroup = itemData?.isFirstTabInGroup == true
      cell.isLastTabInGroup = itemData?.isLastTabInGroup == true
      cell.loading = item.showsActivity
      cell.delegate = self
      cell.accessibilityIdentifier = self.tabTripTabCellAccessibilityIdentifier(
        index: indexPath.item)
      cell.item = item
      if UIAccessibility.isVoiceOverRunning {
        cell.tabIndex = self.tabIndices[itemIdentifier] ?? 0
        let snapshot = self.dataSource.snapshot(for: .tabs)
        if let parentGroup = snapshot.parent(of: itemIdentifier) {
          cell.numberOfTabs = self.numberOfTabsPerGroup[parentGroup] ?? 0
        } else {
          cell.numberOfTabs = self.numberOfUngroupedTabs
        }
      }

      item.fetchFavicon { (item: TabSwitcherItem?, image: UIImage?) -> Void in
        if let item = item, item == cell.item {
          cell.setFaviconImage(image)
        }
      }
    }

    // UICollectionViewDropPlaceholder uses a TabStripTabCell and needs the class to be
    // registered.
    collectionView.register(
      TabStripTabCell.self,
      forCellWithReuseIdentifier: TabStripConstants.CollectionView.tabStripTabCellReuseIdentifier)

    return tabCellRegistration
  }

  /// Creates the registrations of group cells used in the collection view.
  private func createGroupCellRegistration()
    -> UICollectionView.CellRegistration<
      TabStripGroupCell, TabStripItemIdentifier
    >
  {
    return UICollectionView.CellRegistration<
      TabStripGroupCell, TabStripItemIdentifier
    > {
      (cell, indexPath, itemIdentifier) in
      guard let item = itemIdentifier.tabGroupItem else { return }
      let itemData = self.itemData[itemIdentifier] as? TabStripItemData
      cell.title = item.title
      cell.titleContainerBackgroundColor = item.groupColor
      cell.titleTextColor = item.foregroundColor
      cell.collapsed = item.collapsed
      cell.delegate = self
      cell.groupStrokeColor = itemData?.groupStrokeColor
      cell.accessibilityIdentifier = self.tabTripGroupCellAccessibilityIdentifier(
        index: indexPath.item)
    }
  }

  /// Retuns the cell to be used in the collection view.
  static private func getCell(
    collectionView: UICollectionView, indexPath: IndexPath, itemIdentifier: TabStripItemIdentifier,
    tabCellRegistration: UICollectionView.CellRegistration<TabStripTabCell, TabStripItemIdentifier>,
    groupCellRegistration: UICollectionView.CellRegistration<
      TabStripGroupCell, TabStripItemIdentifier
    >
  ) -> UICollectionViewCell? {
    switch itemIdentifier.item {
    case .tab(_):
      return collectionView.dequeueConfiguredReusableCell(
        using: tabCellRegistration,
        for: indexPath,
        item: itemIdentifier)
    case .group(_):
      return collectionView.dequeueConfiguredReusableCell(
        using: groupCellRegistration,
        for: indexPath,
        item: itemIdentifier)
    }
  }

  // Updates visible cells identifier, following a reorg of cells.
  func updateVisibleCellIdentifiers() {
    for indexPath in collectionView.indexPathsForVisibleItems {
      switch collectionView.cellForItem(at: indexPath) {
      case let tabCell as TabStripTabCell:
        tabCell.accessibilityIdentifier = tabTripTabCellAccessibilityIdentifier(
          index: indexPath.item)
      case let groupCell as TabStripGroupCell:
        groupCell.accessibilityIdentifier = tabTripGroupCellAccessibilityIdentifier(
          index: indexPath.item)
      default:
        continue
      }
    }
  }

  // Updates `tabIndex` and `numberOfTabs` for visible tab cells, according to `tabIndices`,
  // `numberOfTabsPerGroup` and `numberOfUngroupedTabs`, following a reorg of cells.
  func updateVisibleCellTabIndices() {
    let snapshot = dataSource.snapshot(for: .tabs)
    for indexPath in collectionView.indexPathsForVisibleItems {
      if let tabCell = collectionView.cellForItem(at: indexPath) as? TabStripTabCell,
        let itemIdentifier = dataSource.itemIdentifier(for: indexPath),
        let tabIndex = tabIndices[itemIdentifier]
      {
        tabCell.tabIndex = tabIndex
        if let parentGroup = snapshot.parent(of: itemIdentifier) {
          tabCell.numberOfTabs = numberOfTabsPerGroup[parentGroup] ?? 0
        } else {
          tabCell.numberOfTabs = numberOfUngroupedTabs
        }
      }
    }
  }

  // Updates `tabIndices`, `numberOfUngroupedTabs` and `numberOfTabsPerGroup` according to `dataSource`.
  func updateTabIndices() {
    let snapshot = dataSource.snapshot(for: .tabs)
    var currentGroupItemIdentifier: TabStripItemIdentifier? = nil
    // Updates `tabIndices` and counts grouped and ungrouped tabs.
    tabIndices = [:]
    numberOfTabsPerGroup = [:]
    numberOfUngroupedTabs = 0
    for itemIdentifier in snapshot.items {
      if snapshot.level(of: itemIdentifier) == 0 {
        currentGroupItemIdentifier = nil
      }
      switch itemIdentifier.item {
      case .tab(_):
        if let currentGroupItemIdentifier = currentGroupItemIdentifier {
          numberOfTabsPerGroup[currentGroupItemIdentifier, default: 0] += 1
          tabIndices[itemIdentifier] = numberOfTabsPerGroup[currentGroupItemIdentifier]
        } else {
          numberOfUngroupedTabs += 1
          tabIndices[itemIdentifier] = numberOfUngroupedTabs
        }
      case .group(_):
        currentGroupItemIdentifier = itemIdentifier
      }
    }
  }

  // Returns the accessibility identifier to set on a TabStripTabCell when
  // positioned at the given index.
  func tabTripTabCellAccessibilityIdentifier(index: Int) -> String {
    return "\(TabStripConstants.CollectionView.tabStripTabCellPrefixIdentifier)\(index)"
  }

  // Returns the accessibility identifier to set on a TabStripGroupCell when
  // positioned at the given index.
  func tabTripGroupCellAccessibilityIdentifier(index: Int) -> String {
    return "\(TabStripConstants.CollectionView.tabStripGroupCellPrefixIdentifier)\(index)"
  }

  /// Scrolls the collection view to the given horizontal `offset`.
  func scrollToContentOffset(_ offset: CGFloat) {
    // TODO(crbug.com/325415449): Update this when #unavailable is recognized by
    // the formatter.
    if #available(iOS 17.0, *) {
    } else {
      if offset != targetedScrollOffsetiOS16 { return }
    }
    self.collectionView.setContentOffset(
      CGPoint(x: offset, y: 0),
      animated: !UIAccessibility.isReduceMotionEnabled)
  }

  /// Ensures `collectionView.indexPathsForSelectedItems` is consistent with
  /// `self.selectedItem`.
  func ensureSelectedItemIsSelected() {
    let expectedIndexPathForSelectedItem =
      TabStripItemIdentifier(selectedItem).flatMap { dataSource.indexPath(for: $0) }
    let observedIndexPathForSelectedItem = collectionView.indexPathsForSelectedItems?.first

    // If the observed selected indexPath doesn't match the expected selected
    // indexPath, update the observed selected item.
    if expectedIndexPathForSelectedItem != observedIndexPathForSelectedItem {
      collectionView.selectItem(
        at: expectedIndexPathForSelectedItem, animated: false, scrollPosition: [])
    }

    /// Invalidate the layout to correctly recalculate the frame of the `selected` cell.
    layout.selectedItem = selectedItem
    layout.invalidateLayout()
  }

  /// Inserts items by applying `snapshot`, updates `numberOfTabs` and optionally scrolls to the end of the collection view if `insertedLast` is true.
  /// To use this method, create a snapshot from `dataSource`, insert items in the snapshot and pass it as the first argument `snapshot`.
  func insertItemsUsingSnapshot(
    _ snapshot: NSDiffableDataSourceSectionSnapshot<TabStripItemIdentifier>, insertedLast: Bool
  ) {
    applySnapshot(
      dataSource: dataSource, snapshot: snapshot,
      animatingDifferences: !UIAccessibility.isReduceMotionEnabled,
      numberOfVisibleItemsChanged: true)

    if insertedLast {
      let offset = collectionView.contentSize.width - collectionView.frame.width
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

  // Called when voice over is activated.
  @objc func voiceOverChanged() {
    guard UIAccessibility.isVoiceOverRunning else { return }
    self.updateTabIndices()
    self.updateVisibleCellTabIndices()
  }

  // MARK: - TabStripNewTabButtonDelegate

  @objc func newTabButtonTapped() {
    UserMetricsUtils.recordAction("MobileTabSwitched")
    UserMetricsUtils.recordAction("MobileTabStripNewTab")
    UserMetricsUtils.recordAction("MobileTabNewTab")

    mutator?.addNewItem()
  }

}

// MARK: - UICollectionViewDelegateFlowLayout

extension TabStripViewController: UICollectionViewDelegateFlowLayout {

  func collectionView(_ collectionView: UICollectionView, shouldSelectItemAt indexPath: IndexPath)
    -> Bool
  {
    switch dataSource.itemIdentifier(for: indexPath)?.item {
    case .tab(_):
      // Only tabs are selectable since being selected is equivalent to having
      // the associated WebState being active.
      return true
    default:
      return false
    }
  }

  func collectionView(
    _ collectionView: UICollectionView, performPrimaryActionForItemAt indexPath: IndexPath
  ) {
    guard let itemIdentifier = dataSource.itemIdentifier(for: indexPath) else { return }
    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      mutator?.activate(tabSwitcherItem)
    case .group(_):
      collapseOrExpandGroup(at: indexPath)
    }
  }

  func collectionView(
    _ collectionView: UICollectionView,
    contextMenuConfiguration configuration: UIContextMenuConfiguration,
    highlightPreviewForItemAt indexPath: IndexPath
  ) -> UITargetedPreview? {
    guard let tabStripCell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return UITargetedPreview(view: tabStripCell, parameters: tabStripCell.dragPreviewParameters)
  }

  func collectionView(
    _ collectionView: UICollectionView,
    contextMenuConfiguration configuration: UIContextMenuConfiguration,
    dismissalPreviewForItemAt indexPath: IndexPath
  ) -> UITargetedPreview? {
    guard let tabStripCell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return UITargetedPreview(view: tabStripCell, parameters: tabStripCell.dragPreviewParameters)
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
    guard let indexPath = indexPaths.first,
      let itemIdentifier = dataSource.itemIdentifier(for: indexPath),
      let cell = collectionView.cellForItem(at: indexPath)
    else {
      return nil
    }
    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      return contextMenuProvider?.contextMenuConfiguration(
        for: tabSwitcherItem, originView: cell, menuScenario: kMenuScenarioHistogramTabStripEntry)
    case .group(let tabGroupItem):
      return contextMenuProvider?.contextMenuConfiguration(
        for: tabGroupItem, originView: cell, menuScenario: kMenuScenarioHistogramTabStripEntry)
    }
  }

  func collectionView(
    _ collectionView: UICollectionView,
    layout collectionViewLayout: UICollectionViewLayout,
    sizeForItemAt indexPath: IndexPath
  ) -> CGSize {
    switch dataSource.itemIdentifier(for: indexPath)?.item {
    case .tab(let tabSwitcherItem):
      return layout.calculateCellSizeForTabSwitcherItem(tabSwitcherItem)
    case .group(let tabGroupItem):
      return layout.calculateCellSizeForTabGroupItem(tabGroupItem)
    case nil:
      return CGSizeZero
    }
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
    guard let draggedItemIdentifier = draggedItemIdentifier else { return }
    dragEndAtNewIndex = false
    switch draggedItemIdentifier.item {
    case .tab(let tabSwitcherItem):
      dragDropHandler?.dragWillBegin?(for: tabSwitcherItem)
      HistogramUtils.recordHistogram(
        kUmaTabStripViewDragDropTabsEvent, withSample: DragDropTabs.dragBegin.rawValue,
        maxValue: DragDropTabs.maxValue.rawValue)
    case .group(let tabGroupItem):
      dragDropHandler?.dragWillBegin?(for: tabGroupItem)
      HistogramUtils.recordHistogram(
        kUmaTabStripViewDragDropGroupsEvent, withSample: DragDropTabs.dragBegin.rawValue,
        maxValue: DragDropTabs.maxValue.rawValue)
    }
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

    dragDropHandler?.dragSessionDidEnd?()

    switch draggedItemIdentifier?.item {
    case .tab(_):
      HistogramUtils.recordHistogram(
        kUmaTabStripViewDragDropTabsEvent, withSample: dragEvent.rawValue,
        maxValue: DragDropTabs.maxValue.rawValue)
    case .group(let tabGroupItem):
      HistogramUtils.recordHistogram(
        kUmaTabStripViewDragDropGroupsEvent, withSample: dragEvent.rawValue,
        maxValue: DragDropTabs.maxValue.rawValue)
      if !tabGroupItem.collapsed, let draggedItemIdentifier = draggedItemIdentifier {
        // If the dragged item is a group and the group was expanded before it started being dragged, then expand it back.
        var snapshot = dataSource.snapshot(for: .tabs)
        snapshot.expand([draggedItemIdentifier])
        applySnapshot(
          dataSource: dataSource, snapshot: snapshot, animatingDifferences: true,
          numberOfVisibleItemsChanged: false)
      }
    default:
      break
    }

    // Reset the current dragged item.
    draggedItemIdentifier = nil
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dragPreviewParametersForItemAt indexPath: IndexPath
  ) -> UIDragPreviewParameters? {
    guard let tabStripCell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return tabStripCell.dragPreviewParameters
  }

  func collectionView(
    _ collectionView: UICollectionView,
    itemsForBeginning session: UIDragSession,
    at indexPath: IndexPath
  ) -> [UIDragItem] {
    guard let itemIdentifier = dataSource.itemIdentifier(for: indexPath) else {
      return []
    }
    let dragItem: UIDragItem?
    switch itemIdentifier.item {
    case .tab(let tabSwitcherItem):
      dragItem = dragDropHandler?.dragItem?(for: tabSwitcherItem)
    case .group(let tabGroupItem):
      dragItem = dragDropHandler?.dragItem?(for: tabGroupItem)
    }
    guard let dragItem = dragItem else {
      return []
    }
    draggedItemIdentifier = itemIdentifier
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
    dropPreviewParametersForItemAt indexPath: IndexPath
  ) -> UIDragPreviewParameters? {
    guard let tabStripCell = collectionView.cellForItem(at: indexPath) as? TabStripCell else {
      return nil
    }
    return tabStripCell.dragPreviewParameters
  }

  func collectionView(
    _ collectionView: UICollectionView,
    dropSessionDidUpdate session: UIDropSession,
    withDestinationIndexPath destinationIndexPath: IndexPath?
  ) -> UICollectionViewDropProposal {
    // Calculating location in view
    let location = session.location(in: collectionView)
    var destinationIndexPath: IndexPath?
    collectionView.performUsingPresentationValues {
      destinationIndexPath = collectionView.indexPathForItem(at: location)
    }
    guard let destinationIndexPath = destinationIndexPath else {
      return UICollectionViewDropProposal(operation: .cancel, intent: .unspecified)
    }
    guard
      let dropOperation: UIDropOperation = dragDropHandler?.dropOperation(
        for: session, to: UInt(destinationIndexPath.item))
    else {
      return UICollectionViewDropProposal(operation: .cancel)
    }
    /// Use `insertIntoDestinationIndexPath` if the dragged item is not from the same
    /// collection view. This prevents having unwanted empty space in the collection view.
    return UICollectionViewDropProposal(
      operation: dropOperation,
      intent: draggedItemIdentifier != nil
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
      let dropIndexPath = dropIndexPath(item: item, destinationIndex: destinationIndex)
      dragEndAtNewIndex = true

      // Drop synchronously if local object is available.
      if item.dragItem.localObject != nil {
        weak var weakSelf = self
        coordinator.drop(item.dragItem, toItemAt: dropIndexPath).addCompletion {
          _ in
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
          insertionIndexPath: dropIndexPath,
          reuseIdentifier: TabStripConstants.CollectionView.tabStripTabCellReuseIdentifier)
        placeholder.previewParametersProvider = {
          (placeholderCell: UICollectionViewCell) -> UIDragPreviewParameters? in
          guard let tabStripCell = placeholderCell as? TabStripTabCell else {
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

  // MARK: - Private

  /// Determines the IndexPath where a dropped UICollectionViewDropItem should be inserted.
  private func dropIndexPath(item: UICollectionViewDropItem, destinationIndex: Int) -> IndexPath {
    let defaultIndexPath = IndexPath(item: destinationIndex, section: 0)

    // Item originates from a different collection view.
    guard let sourceIndexPath = item.sourceIndexPath else {
      return defaultIndexPath
    }

    // Item is dropped before its original position.
    if sourceIndexPath.item > destinationIndex {
      return defaultIndexPath
    }

    guard let draggedItemIdentifier = draggedItemIdentifier,
      let itemData = self.itemData[draggedItemIdentifier] as? TabStripItemData
    else {
      return defaultIndexPath
    }

    // If the tab item is the only item in its group, adjust drop position.
    if itemData.groupStrokeColor != nil && itemData.isFirstTabInGroup && itemData.isLastTabInGroup {
      return IndexPath(item: destinationIndex - 1, section: 0)
    }

    return defaultIndexPath
  }

}
