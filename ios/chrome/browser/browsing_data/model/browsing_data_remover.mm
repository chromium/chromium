// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer.h"

BrowsingDataRemover::BrowsingDataRemover() = default;

BrowsingDataRemover::~BrowsingDataRemover() = default;

base::WeakPtr<BrowsingDataRemover> BrowsingDataRemover::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BrowsingDataRemover::AddObserver(BrowsingDataRemoverObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(
    BrowsingDataRemoverObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowsingDataRemover::NotifyBrowsingDataRemoved(
    BrowsingDataRemoveMask mask) {
  for (BrowsingDataRemoverObserver& observer : observers_) {
    observer.OnBrowsingDataRemoved(this, mask);
  }
}
