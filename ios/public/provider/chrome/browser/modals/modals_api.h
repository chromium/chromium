// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Dismisses any modals presented from a `collection_view` item.
void DismissModalsForCollectionView(UICollectionView* collection_view);

// Dismisses any modals presented from a `table_view` cell.
void DismissModalsForTableView(UITableView* table_view);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_API_H_
