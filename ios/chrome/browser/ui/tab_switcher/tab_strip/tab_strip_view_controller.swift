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
  private let diffableDataSource: UICollectionViewDiffableDataSource<Section, TabSwitcherItem>

  weak var mutator: TabStripMutator?

  init() {
    layout = TabStripLayout()
    collectionView = UICollectionView(frame: .zero, collectionViewLayout: layout)
    diffableDataSource = UICollectionViewDiffableDataSource<Section, TabSwitcherItem>(
      collectionView: collectionView
    ) {
      (collectionView: UICollectionView, indexPath: IndexPath, itemIdentifier: TabSwitcherItem)
        -> UICollectionViewCell? in
      return nil
    }
    layout.dataSource = diffableDataSource
    super.init(nibName: nil, bundle: nil)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  // MARK: - TabStripConsumer

  func populate(items: [TabSwitcherItem]?, selectedItem: TabSwitcherItem?) {
    guard let items = items else {
      return
    }
    var snapshot = NSDiffableDataSourceSnapshot<Section, TabSwitcherItem>()
    snapshot.appendSections([.tabs])
    snapshot.appendItems(items, toSection: .tabs)
    diffableDataSource.apply(snapshot)
  }

  func reloadItem(_ item: TabSwitcherItem?) {
    // TODO(crbug.com/1490555): Implement this.
  }

  func selectItem(_ item: TabSwitcherItem?) {
    guard let item = item else {
      return
    }
    let indexPath = diffableDataSource.indexPath(for: item)
    collectionView.selectItem(at: indexPath, animated: true, scrollPosition: [])
  }

}
