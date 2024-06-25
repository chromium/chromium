// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CoreSpotlight
import UIKit

/// Controller backed by SpotlightLogger known items, with filter function.
class ItemsController {

  var query: CSSearchQuery? = nil
  var allItems = [CSSearchableItem]()

  func filteredItems(with filter: String? = nil, limit: Int? = nil) -> [CSSearchableItem] {
    let filtered = allItems.filter { $0.contains(filter) }
    if let limit = limit {
      return Array(filtered.prefix(through: limit))
    } else {
      return filtered
    }
  }

  func fetchAllItems(completionHandler: @escaping () -> Void) {
    self.allItems = []
    let queryString = "title == *"
    let context = CSSearchQueryContext()
    context.fetchAttributes = [
      "uniqueIdentifier",
      "title", "domain", "id", "URL", "description",
      "thumbnail data", "displayName", "keywords",
      "contentType", "domainIdentifier", "identifier", "rankingHint",
    ]
    self.query = CSSearchQuery(
      queryString: queryString,
      queryContext: context)
    self.query?.foundItemsHandler = { (items: [CSSearchableItem]) -> Void in
      self.allItems.append(contentsOf: items)
    }
    self.query?.completionHandler = { (error: Error?) -> Void in
      DispatchQueue.main.async {
        completionHandler()
      }
    }
    self.query?.start()
  }
}

/// Displays a list of all searchable items from the spotlight logger.
class SpotlightDebuggerAllItemsViewController: UIViewController {

  enum Section: CaseIterable {
    case main
  }
  let itemsController = ItemsController()
  let searchBar = UISearchBar(frame: .zero)
  var collectionView: UICollectionView!
  var dataSource: UICollectionViewDiffableDataSource<Section, CSSearchableItem>!
  var nameFilter: String?

  override func viewDidLoad() {
    super.viewDidLoad()
    navigationItem.title = "Donated Items"
    configureHierarchy()
    configureDataSource()
    performQuery(with: nil)
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    collectionView.deselectAllItems(animated: animated)
    itemsController.fetchAllItems {
      // Reload data by executing an empty filter query.
      self.performQuery(with: "")
    }
  }
}

extension UICollectionView {
  func deselectAllItems(animated: Bool) {
    guard let selectedItems = indexPathsForSelectedItems else { return }
    for indexPath in selectedItems { deselectItem(at: indexPath, animated: animated) }
  }
}

extension SpotlightDebuggerAllItemsViewController {
  func configureDataSource() {

    let cellRegistration = UICollectionView.CellRegistration<
      UICollectionViewListCell, CSSearchableItem
    > { (cell, indexPath, item) in
      var content = cell.defaultContentConfiguration()
      content.text = item.attributeSet.title
      if let data = item.attributeSet.thumbnailData {
        content.image = UIImage(data: data)
        content.imageProperties.maximumSize = CGSize(width: 40, height: 40)
      } else {
        content.image = UIImage(systemName: "questionmark.diamond")
      }

      cell.accessories = [.disclosureIndicator()]

      cell.contentConfiguration = content
    }

    dataSource = UICollectionViewDiffableDataSource<Section, CSSearchableItem>(
      collectionView: collectionView
    ) {
      (collectionView: UICollectionView, indexPath: IndexPath, identifier: CSSearchableItem)
        -> UICollectionViewCell? in
      // Return the cell.
      return collectionView.dequeueConfiguredReusableCell(
        using: cellRegistration, for: indexPath, item: identifier)
    }
  }

  func performQuery(with filter: String?) {
    let items = itemsController.filteredItems(with: filter).sorted {
      $0.uniqueIdentifier < $1.uniqueIdentifier
    }

    var snapshot = NSDiffableDataSourceSnapshot<Section, CSSearchableItem>()
    snapshot.appendSections([.main])
    snapshot.appendItems(items)
    dataSource.apply(snapshot, animatingDifferences: true)
  }
}

extension SpotlightDebuggerAllItemsViewController {
  func createLayout() -> UICollectionViewLayout {
    let config = UICollectionLayoutListConfiguration(appearance: .insetGrouped)
    return UICollectionViewCompositionalLayout.list(using: config)
  }

  func configureHierarchy() {
    view.backgroundColor = .systemBackground
    let layout = createLayout()
    let collectionView = UICollectionView(frame: view.bounds, collectionViewLayout: layout)
    collectionView.translatesAutoresizingMaskIntoConstraints = false
    searchBar.translatesAutoresizingMaskIntoConstraints = false
    collectionView.backgroundColor = .systemBackground
    collectionView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    collectionView.delegate = self
    view.addSubview(collectionView)
    view.addSubview(searchBar)

    let views = ["cv": collectionView, "searchBar": searchBar]
    var constraints = [NSLayoutConstraint]()
    constraints.append(
      contentsOf: NSLayoutConstraint.constraints(
        withVisualFormat: "H:|[cv]|", options: [], metrics: nil, views: views))
    constraints.append(
      contentsOf: NSLayoutConstraint.constraints(
        withVisualFormat: "H:|[searchBar]|", options: [], metrics: nil, views: views))
    constraints.append(
      contentsOf: NSLayoutConstraint.constraints(
        withVisualFormat: "V:[searchBar]-20-[cv]|", options: [], metrics: nil, views: views))
    constraints.append(
      searchBar.topAnchor.constraint(
        equalToSystemSpacingBelow: view.safeAreaLayoutGuide.topAnchor, multiplier: 1.0))
    NSLayoutConstraint.activate(constraints)
    self.collectionView = collectionView

    searchBar.delegate = self
  }
}

extension SpotlightDebuggerAllItemsViewController: UICollectionViewDelegate {
  func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
    guard let item = self.dataSource.itemIdentifier(for: indexPath) else {
      collectionView.deselectItem(at: indexPath, animated: true)
      return
    }
    let detailViewController = SearchableItemDetailViewController(with: item)
    self.navigationController?.pushViewController(detailViewController, animated: true)
  }
}

extension SpotlightDebuggerAllItemsViewController: UISearchBarDelegate {
  func searchBar(_ searchBar: UISearchBar, textDidChange searchText: String) {
    performQuery(with: searchText)
  }
}

extension CSSearchableItem {

  func contains(_ filter: String?) -> Bool {
    guard let filterText = filter else { return true }
    if filterText.isEmpty { return true }
    let lowercasedFilter = filterText.lowercased()

    return (attributeSet.title?.lowercased().contains(lowercasedFilter) ?? false)
      || (attributeSet.url?.absoluteString.lowercased().contains(lowercasedFilter) ?? false)
      || (domainIdentifier?.lowercased().contains(lowercasedFilter) ?? false)
      || uniqueIdentifier.lowercased().contains(lowercasedFilter)
  }
}
