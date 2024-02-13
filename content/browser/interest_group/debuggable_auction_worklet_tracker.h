// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_TRACKER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_TRACKER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class DebuggableAuctionWorklet;

// This keeps track of when worklets are created and destroyed, and lets it be
// observed via Observer subscriptions or explicit enumeration.
class CONTENT_EXPORT DebuggableAuctionWorkletTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a worklet is about to be launched. An implementation can
    // set `should_pause_on_start` to ask the worklet to not do anything yet
    // until resumed via devtools protocol).
    virtual void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                                       bool& should_pause_on_start) {}

    // Called when the worklet is being destroyed. It should not be accessed
    // afterwards.
    virtual void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) {}

   protected:
    ~Observer() override = default;
  };

  explicit DebuggableAuctionWorkletTracker(
      const DebuggableAuctionWorkletTracker&) = delete;
  DebuggableAuctionWorkletTracker& operator=(
      const DebuggableAuctionWorkletTracker&) = delete;

  static DebuggableAuctionWorkletTracker* GetInstance();

  std::vector<DebuggableAuctionWorklet*> GetAll();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class DebuggableAuctionWorklet;
  friend class base::NoDestructor<DebuggableAuctionWorkletTracker>;
  DebuggableAuctionWorkletTracker();
  ~DebuggableAuctionWorkletTracker();

  void NotifyCreated(DebuggableAuctionWorklet* worklet,
                     bool& should_pause_on_start);
  void NotifyDestroyed(DebuggableAuctionWorklet* worklet);

  base::ObserverList<Observer> observer_list_;
  std::set<raw_ptr<DebuggableAuctionWorklet, SetExperimental>> live_worklets_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_TRACKER_H_
