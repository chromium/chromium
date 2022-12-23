// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CoreSpotlight
import UIKit

/// Displays details of a single CSSearchableItem.
class SearchableItemDetailViewController: UIViewController {

  let item: CSSearchableItem

  init(with item: CSSearchableItem) {
    self.item = item
    super.init(nibName: nil, bundle: nil)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  enum Section: CaseIterable {
    case main
  }

  struct SearchableItemDetail: Hashable {
    let title: String  // Used as unique ID
    let value: String?

    func hash(into hasher: inout Hasher) {
      hasher.combine(title)
    }
    static func == (lhs: SearchableItemDetail, rhs: SearchableItemDetail) -> Bool {
      return lhs.title == rhs.title
    }
  }

  var dataSource: UICollectionViewDiffableDataSource<Section, SearchableItemDetail>! = nil
  var collectionView: UICollectionView! = nil

  override func viewDidLoad() {
    super.viewDidLoad()
    navigationItem.title = item.attributeSet.title ?? item.uniqueIdentifier
    configureHierarchy()
    configureDataSource()
  }

  func createLayout() -> UICollectionViewLayout {
    let config = UICollectionLayoutListConfiguration(appearance: .insetGrouped)
    return UICollectionViewCompositionalLayout.list(using: config)
  }

  private func configureHierarchy() {
    collectionView = UICollectionView(frame: view.bounds, collectionViewLayout: createLayout())
    collectionView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    view.addSubview(collectionView)
  }

  private func configureDataSource() {

    let cellRegistration = UICollectionView.CellRegistration<
      UICollectionViewListCell, SearchableItemDetail
    > { (cell, indexPath, detail) in
      var content = cell.defaultContentConfiguration()
      content.text = detail.title
      content.secondaryText = detail.value
      cell.contentConfiguration = content
    }

    dataSource = UICollectionViewDiffableDataSource<Section, SearchableItemDetail>(
      collectionView: collectionView
    ) {
      (collectionView: UICollectionView, indexPath: IndexPath, identifier: SearchableItemDetail)
        -> UICollectionViewCell? in

      return collectionView.dequeueConfiguredReusableCell(
        using: cellRegistration, for: indexPath, item: identifier)
    }

    var snapshot = NSDiffableDataSourceSnapshot<Section, SearchableItemDetail>()
    snapshot.appendSections([SearchableItemDetailViewController.Section.main])

    snapshot.appendItems([
      SearchableItemDetail(title: "displayName", value: item.attributeSet.displayName),
      SearchableItemDetail(title: "domain", value: item.domainIdentifier),

      SearchableItemDetail(title: "title", value: item.attributeSet.title),
      SearchableItemDetail(title: "id", value: item.uniqueIdentifier),
      SearchableItemDetail(title: "URL", value: item.attributeSet.url?.absoluteString),
      SearchableItemDetail(title: "description", value: item.attributeSet.contentDescription),
      SearchableItemDetail(title: "thumbnail data", value: thumbnailDescription()),
    ])

    dataSource.apply(snapshot, animatingDifferences: false)
  }

  private func thumbnailDescription() -> String {
    guard let data = item.attributeSet.thumbnailData else {
      return "not available"
    }

    guard let image = UIImage(data: data) else {
      return "corrupt image data"
    }

    return "image, \(image.size.width)x\(image.size.height)pt.@\(image.scale)x"
  }
}
