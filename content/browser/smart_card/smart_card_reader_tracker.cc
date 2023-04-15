// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_reader_tracker.h"
#include "content/browser/smart_card/smart_card_reader_tracker_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/smart_card_delegate.h"

namespace content {
namespace {
static constexpr char kSmartCardReaderTrackerKey[] =
    "SmartCardReaderTrackerKey";

std::unique_ptr<SmartCardReaderTracker> CreateSmartCardReaderTracker(
    BrowserContext& browser_context,
    SmartCardDelegate& delegate) {
  return std::make_unique<SmartCardReaderTrackerImpl>(
      delegate.GetSmartCardContextFactory(browser_context),
      delegate.SupportsReaderAddedRemovedNotifications());
}
}  // namespace

// static
const void* SmartCardReaderTracker::user_data_key_for_testing() {
  return kSmartCardReaderTrackerKey;
}

// static
SmartCardReaderTracker& SmartCardReaderTracker::GetForBrowserContext(
    BrowserContext& browser_context,
    SmartCardDelegate& delegate) {
  if (!browser_context.GetUserData(kSmartCardReaderTrackerKey)) {
    browser_context.SetUserData(
        kSmartCardReaderTrackerKey,
        CreateSmartCardReaderTracker(browser_context, delegate));
  }

  return *static_cast<SmartCardReaderTracker*>(
      browser_context.GetUserData(kSmartCardReaderTrackerKey));
}

SmartCardReaderTracker::ObserverList::ObserverList() = default;
SmartCardReaderTracker::ObserverList::~ObserverList() = default;

void SmartCardReaderTracker::ObserverList::AddObserverIfMissing(
    Observer* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void SmartCardReaderTracker::ObserverList::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SmartCardReaderTracker::ObserverList::NotifyReaderAdded(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (Observer& obs : observers_) {
    obs.OnReaderAdded(reader_info);
  }
}

void SmartCardReaderTracker::ObserverList::NotifyReaderChanged(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (Observer& obs : observers_) {
    obs.OnReaderChanged(reader_info);
  }
}

void SmartCardReaderTracker::ObserverList::NotifyReaderRemoved(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (Observer& obs : observers_) {
    obs.OnReaderRemoved(reader_info);
  }
}

void SmartCardReaderTracker::ObserverList::NotifyError(
    device::mojom::SmartCardError error) {
  for (Observer& obs : observers_) {
    obs.OnError(error);
  }
}
}  // namespace content
