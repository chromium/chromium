// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import StoreKit
import UIKit

// An adapter class around the StoreKit::AppStore enum. Apple's AppStore enumeration was introduced in iOS15 and is Swift-only.
@available(iOS 18.0, *)
public final class AppStoreReviewAdapter: NSObject {
  // Requests an App Store rating/review from the user using `scene`.
  @MainActor
  @objc(requestReviewInScene:)
  public static func requestReview(in scene: UIWindowScene) {
    AppStore.requestReview(in: scene)
  }
}
