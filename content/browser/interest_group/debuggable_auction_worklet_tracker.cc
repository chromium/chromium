// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace content {

DebuggableAuctionWorkletTracker*
DebuggableAuctionWorkletTracker::GetInstance() {
  static base::NoDestructor<DebuggableAuctionWorkletTracker> instance;
  return &*instance;
}

std::vector<DebuggableAuctionWorklet*>
DebuggableAuctionWorkletTracker::GetAll() {
  std::vector<DebuggableAuctionWorklet*> result;
  for (DebuggableAuctionWorklet* item : live_worklets_)
    result.push_back(item);
  return result;
}

void DebuggableAuctionWorkletTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DebuggableAuctionWorkletTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DebuggableAuctionWorkletTracker::NotifyCreated(
    DebuggableAuctionWorklet* worklet,
    bool& should_pause_on_start) {
  live_worklets_.insert(worklet);
  for (auto& observer : observer_list_) {
    bool local_should_pause_on_start = false;
    observer.AuctionWorkletCreated(worklet, local_should_pause_on_start);
    should_pause_on_start |= local_should_pause_on_start;
  }
}

void DebuggableAuctionWorkletTracker::NotifyDestroyed(
    DebuggableAuctionWorklet* worklet) {
  size_t result = live_worklets_.erase(worklet);
  CHECK_EQ(result, 1u);
  for (auto& observer : observer_list_)
    observer.AuctionWorkletDestroyed(worklet);
}

DebuggableAuctionWorkletTracker::DebuggableAuctionWorkletTracker() = default;
DebuggableAuctionWorkletTracker::~DebuggableAuctionWorkletTracker() = default;

}  // namespace content
