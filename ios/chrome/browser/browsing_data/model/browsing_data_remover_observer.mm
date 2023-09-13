// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer.h"

BrowsingDataRemoverObserver::~BrowsingDataRemoverObserver() {
  CHECK(!IsInObserverList())
      << "BrowsingDataRemoverObserver needs to be removed from "
         "BrowsingDataRemover observer list before their destruction.";
}
