// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// View Controller displaying the TabStrip.
@objcMembers
class TabStripViewController: UIViewController, TabStripConsumer {

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

  init() {
    layout = TabStripLayout()
    collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
    super.init(nibName: nil, bundle: nil)

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

    // TODO(crbug.com/1490555): Handle the selected item.
  }

  func selectItem(_ item: TabSwitcherItem?) {
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

  // MARK: - Private

  /// Creates the registrations of the different cells used in the collection view.
  func createRegistrations() {
    tabCellRegistration = UICollectionView.CellRegistration<TabStripCell, TabSwitcherItem> {
      (cell, indexPath, item) in
      cell.titleLabel.text = item.title
    }
  }

  /// Retuns the cell to be used in the collection view.
  func getCell(
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
}
