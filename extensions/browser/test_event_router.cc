// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_event_router.h"
#include "base/check_op.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"

namespace extensions {

TestEventRouter::EventObserver::~EventObserver() = default;

void TestEventRouter::EventObserver::OnDispatchEventToExtension(
    const ExtensionId& extension_id,
    const Event& event) {}

void TestEventRouter::EventObserver::OnBroadcastEvent(const Event& event) {}

TestEventRouter::TestEventRouter(content::BrowserContext* context)
    : EventRouter(context, ExtensionPrefs::Get(context)) {}

TestEventRouter::~TestEventRouter() = default;

int TestEventRouter::GetEventCount(std::string event_name) const {
  if (seen_events_.count(event_name) == 0) {
    return 0;
  }
  return seen_events_.find(event_name)->second;
}

void TestEventRouter::AddEventObserver(EventObserver* obs) {
  observers_.AddObserver(obs);
}

void TestEventRouter::RemoveEventObserver(EventObserver* obs) {
  observers_.RemoveObserver(obs);
}

void TestEventRouter::BroadcastEvent(std::unique_ptr<Event> event) {
  IncrementEventCount(event->event_name);

  for (auto& observer : observers_)
    observer.OnBroadcastEvent(*event);
}

void TestEventRouter::DispatchEventToExtension(const ExtensionId& extension_id,
                                               std::unique_ptr<Event> event) {
  if (!expected_extension_id_.empty()) {
    DCHECK_EQ(expected_extension_id_, extension_id);
  }

  IncrementEventCount(event->event_name);

  for (auto& observer : observers_)
    observer.OnDispatchEventToExtension(extension_id, *event);
}

void TestEventRouter::IncrementEventCount(const std::string& event_name) {
  if (seen_events_.count(event_name) == 0) {
    seen_events_[event_name] = 0;
  }
  seen_events_[event_name]++;
}

}  // namespace extensions
