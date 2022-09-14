// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/modals/modals_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

void DismissModalsForCollectionView(UICollectionView*) {
  // Test implementation does nothing.
}

void DismissModalsForTableView(UITableView*) {
  // Test implementation does nothing.
}

}  // namespace provider
}  // namespace ios
