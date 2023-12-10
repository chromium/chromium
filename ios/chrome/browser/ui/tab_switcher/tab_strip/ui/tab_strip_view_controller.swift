// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// View Controller displaying the TabStrip.
@objcMembers
class TabStripViewController: UIViewController, TabStripCellDelegate, TabStripConsumer,
  UICollectionViewDelegate
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
    NSLayoutConstraint.activate([
      self.view.leadingAnchor.constraint(equalTo: collectionView.leadingAnchor),
      self.view.trailingAnchor.constraint(equalTo: collectionView.trailingAnchor),
      self.view.topAnchor.constraint(equalTo: collectionView.topAnchor),
      self.view.bottomAnchor.constraint(equalTo: collectionView.bottomAnchor),
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
    diffableDataSource?.apply(snapshot)

    guard let selectedItem = selectedItem, let diffableDataSource = diffableDataSource else {
      return
    }
    let indexPath = diffableDataSource.indexPath(for: selectedItem)
    collectionView.selectItem(at: indexPath, animated: true, scrollPosition: [])
  }

  func selectItem(_ item: TabSwitcherItem?) {
    if let indexPaths = collectionView.indexPathsForSelectedItems {
      for indexPath in indexPaths {
        collectionView.deselectItem(at: indexPath, animated: true)
      }
    }
    guard let item = item, let diffableDataSource = diffableDataSource else {
      return
    }
    let indexPath = diffableDataSource.indexPath(for: item)
    collectionView.selectItem(at: indexPath, animated: true, scrollPosition: [])
  }

  func reloadItem(_ item: TabSwitcherItem?) {
    guard let item = item, let diffableDataSource = diffableDataSource else {
      return
    }

    var snapshot = diffableDataSource.snapshot()
    snapshot.reconfigureItems([item])
    diffableDataSource.apply(snapshot, animatingDifferences: false)
  }

  func replaceItem(_ oldItem: TabSwitcherItem?, withItem newItem: TabSwitcherItem?) {
    guard let oldItem = oldItem, let newItem = newItem, let diffableDataSource = diffableDataSource
    else {
      return
    }

    var snapshot = diffableDataSource.snapshot()
    snapshot.insertItems([newItem], beforeItem: oldItem)
    snapshot.deleteItems([oldItem])
    diffableDataSource.apply(snapshot, animatingDifferences: false)
  }

  // MARK: - UICollectionViewDelegate

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

}
