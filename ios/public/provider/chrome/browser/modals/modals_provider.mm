// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/modals/modals_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ModalsProvider::ModalsProvider() = default;

ModalsProvider::~ModalsProvider() = default;

void ModalsProvider::DismissModalsForCollectionView(
    UICollectionView* collection_view) {}

void ModalsProvider::DismissModalsForTableView(UITableView* table_view) {}