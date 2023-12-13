// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// View Controller displaying the TabStrip.
@objcMembers
class TabStripViewController: UIViewController, TabStripCellDelegate,
  TabStripConsumer
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

  weak var mutator: TabStripMutator?
  weak var delegate: TabStripViewControllerDelegate?

  init() {
    layout = TabStripLayout()
    collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
    super.init(nibName: nil, bundle: nil)

    collectionView.delegate = self
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
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    collectionView.translatesAutoresizingMaskIntoConstraints = false
    self.view.addSubview(collectionView)
    self.view.addSubview(newTabButton)
    newTabButton.addTarget(self, action: #selector(newTabButtonTapped), for: .touchUpInside)

    NSLayoutConstraint.activate([
      self.view.leadingAnchor.constraint(equalTo: collectionView.leadingAnchor),
      self.view.topAnchor.constraint(equalTo: collectionView.topAnchor),
      self.view.bottomAnchor.constraint(equalTo: collectionView.bottomAnchor),
      self.view.trailingAnchor.constraint(equalTo: newTabButton.trailingAnchor),

      newTabButton.heightAnchor.constraint(equalTo: collectionView.heightAnchor),
      newTabButton.widthAnchor.constraint(equalTo: newTabButton.heightAnchor),
      newTabButton.leadingAnchor.constraint(equalTo: collectionView.trailingAnchor),
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
  }

  func selectItem(_ item: TabSwitcherItem?) {
    if let indexPaths = collectionView.indexPathsForSelectedItems {
      for indexPath in indexPaths {
        collectionView.deselectItem(at: indexPath, animated: true)
      }
    }
    guard
      let item = item, let diffableDataSource = diffableDataSource,
      let indexPath = diffableDataSource.indexPath(for: item)
    else { return }

    /// `.centeredHorizontally` is needed when the selected cell is not dequeued.
    /// If the item is dequeued `.centeredVertically` will not update the layout.
    let scrollPosition: UICollectionView.ScrollPosition =
      collectionView.cellForItem(at: indexPath) != nil
      ? .centeredVertically : .centeredHorizontally
    collectionView.selectItem(at: indexPath, animated: true, scrollPosition: scrollPosition)
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
  }

  /// Creates the registrations of the different cells used in the collection view.
  private func createRegistrations() {
    tabCellRegistration = UICollectionView.CellRegistration<TabStripCell, TabSwitcherItem> {
      (cell, indexPath, item) in
      cell.setTitle(item.title)
      cell.loading = item.showsActivity
      cell.delegate = self

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

  /// Called when the `newTabButton` has been tapped.
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
