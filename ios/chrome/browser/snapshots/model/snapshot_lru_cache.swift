// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// A LRU (Least Recently Used) cache for snapshot images with a limited size.
// Once the cache reach its size limit, it will start to evict items in a
// Least Recently Used order. (where the term "used" is determined in terms
// of query to the cache).
@objcMembers public class SnapshotLRUCache: NSObject {
  // The number of snapshot images stored in this cache.
  private var count: Int
  // The cache that stores snapshot images.
  private let cache: NSCache<SnapshotIDWrapper, UIImage>

  init(size: Int) {
    self.count = 0
    self.cache = NSCache<SnapshotIDWrapper, UIImage>()
    self.cache.countLimit = size
  }

  func getObject(forKey: SnapshotIDWrapper) -> UIImage? {
    return self.cache.object(forKey: forKey)
  }

  func setObject(value: UIImage, forKey: SnapshotIDWrapper) {
    if self.cache.object(forKey: forKey) == nil {
      self.count = min(maxCacheSize(), self.count + 1)
    }
    self.cache.setObject(value, forKey: forKey)
  }

  func removeObject(forKey: SnapshotIDWrapper) {
    if self.cache.object(forKey: forKey) != nil {
      self.count = max(0, self.count - 1)
    }
    self.cache.removeObject(forKey: forKey)
  }

  func removeAllObjects() {
    self.count = 0
    self.cache.removeAllObjects()
  }

  func maxCacheSize() -> Int {
    return self.cache.countLimit
  }

  func getCount() -> Int {
    return self.count
  }
}
