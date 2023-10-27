// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// Layout used for the TabStrip.
class TabStripLayout: UICollectionViewCompositionalLayout {

  // The data source used for the collection view.
  weak var dataSource:
    UICollectionViewDiffableDataSource<TabStripViewController.Section, TabSwitcherItem>?

  required init() {
    let configuration = UICollectionViewCompositionalLayoutConfiguration()
    configuration.scrollDirection = .horizontal
    // Use a `futureSelf` variable as the super init requires a closure and as
    // init is not instantiated yet, we can't use it.
    weak var futureSelf: TabStripLayout?
    super.init(
      sectionProvider: {
        (
          sectionIndex: Int,
          layoutEnvironment: NSCollectionLayoutEnvironment
        ) -> NSCollectionLayoutSection? in
        return futureSelf?.getSection(
          sectionIndex: sectionIndex, layoutEnvironment: layoutEnvironment)
      }, configuration: configuration)
    futureSelf = self
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

  // MARK: - Private

  func getSection(
    sectionIndex: Int,
    layoutEnvironment: NSCollectionLayoutEnvironment
  ) -> NSCollectionLayoutSection? {
    // TODO(crbug.com/1490555): For now this layout doesn't really make sense. Update it to make it look like the mocks.
    let itemSize = NSCollectionLayoutSize(
      widthDimension: .fractionalWidth(1.0),
      heightDimension: .fractionalHeight(1.0))
    let item = NSCollectionLayoutItem(layoutSize: itemSize)

    let groupSize = NSCollectionLayoutSize(
      widthDimension: .absolute(150),
      heightDimension: .absolute(39))
    let group = NSCollectionLayoutGroup.vertical(
      layoutSize: groupSize,
      subitem: item,
      count: 1)

    let section = NSCollectionLayoutSection(group: group)
    return section
  }
}
