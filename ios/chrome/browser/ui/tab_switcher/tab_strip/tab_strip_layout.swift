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
    super.init { (Int, NSCollectionLayoutEnvironment) -> NSCollectionLayoutSection? in
      return nil
    }

  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) is not supported")
  }

}
