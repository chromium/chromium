// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_PROVIDER_H_

#import <UIKit/UIKit.h>

class ModalsProvider {
 public:
  ModalsProvider();
  virtual ~ModalsProvider();

  // Dismisses any modals presented from a |collection_view| item.
  virtual void DismissModalsForCollectionView(
      UICollectionView* collection_view);

  // Dismisses any modals presented from a |table_view| cell.
  virtual void DismissModalsForTableView(UITableView* table_view);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MODALS_MODALS_PROVIDER_H_
