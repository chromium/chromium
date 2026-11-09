// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/listener_registration_phase_map.h"

#include "base/check.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

bool ListenerRegistrationPhaseMap::ListenerSet::Has(
    const std::string& event_name,
    const base::DictValue* filter) const {
  if (!filter) {
    return events.contains(event_name);
  }
  const base::ListValue* filters = filtered_events.FindList(event_name);
  return filters && filters->contains(*filter);
}

void ListenerRegistrationPhaseMap::ListenerSet::Add(
    const std::string& event_name,
    const base::DictValue* filter) {
  if (!filter) {
    events.insert(event_name);
  } else if (!Has(event_name, filter)) {
    filtered_events.EnsureList(event_name)->Append(filter->Clone());
  }
}

void ListenerRegistrationPhaseMap::ListenerSet::Remove(
    const std::string& event_name,
    const base::DictValue* filter) {
  if (!filter) {
    events.erase(event_name);
    return;
  }
  base::ListValue* filters = filtered_events.FindList(event_name);
  if (!filters) {
    return;
  }
  filters->EraseValue(base::Value(filter->Clone()));
  if (filters->empty()) {
    filtered_events.Remove(event_name);
  }
}

void ListenerRegistrationPhaseMap::ListenerSet::AddAll(
    const ListenerSet& other) {
  events.insert(other.events.begin(), other.events.end());
  // `other` can come from prefs, so skip malformed entries.
  for (const auto [event_name, value] : other.filtered_events) {
    const base::ListValue* filters = value.GetIfList();
    if (!filters) {
      continue;
    }
    for (const base::Value& filter : *filters) {
      if (const base::DictValue* filter_dict = filter.GetIfDict()) {
        Add(event_name, filter_dict);
      }
    }
  }
}

ListenerRegistrationPhaseMap::ListenerSet
ListenerRegistrationPhaseMap::ListenerSet::Clone() const {
  return {events, filtered_events.Clone()};
}

ListenerRegistrationPhaseMap::ListenerRegistrationPhaseMap(
    OnFinishedCallback on_finished)
    : on_finished_(std::move(on_finished)) {}

ListenerRegistrationPhaseMap::~ListenerRegistrationPhaseMap() = default;

void ListenerRegistrationPhaseMap::Start(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context,
    const blink::ServiceWorkerToken& service_worker_token) {
  // A worker instance initializes once; duplicate initialization for the same
  // token should never occur.
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  DCHECK(it == phases_.end() ||
         it->second.service_worker_token != service_worker_token);
  phases_.insert_or_assign({extension_id, browser_context.UniqueToken()},
                           Phase{service_worker_token});
}

bool ListenerRegistrationPhaseMap::Commit(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context,
    const blink::ServiceWorkerToken& service_worker_token) {
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  if (it == phases_.end() || it->second.state != State::kStarted ||
      it->second.service_worker_token != service_worker_token) {
    // Ignore if no registration phase is currently in progress for this worker
    // instance.
    return false;
  }
  it->second.state = State::kCommitted;
  on_finished_.Run(extension_id, browser_context, State::kCommitted);
  return true;
}

void ListenerRegistrationPhaseMap::Abort(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context) {
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  if (it == phases_.end() || it->second.state != State::kStarted) {
    // Nothing to abort if no registration phase is currently in progress.
    return;
  }
  it->second.state = State::kAborted;
  on_finished_.Run(extension_id, browser_context, State::kAborted);
}

void ListenerRegistrationPhaseMap::AbortForInstance(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context,
    const blink::ServiceWorkerToken& service_worker_token) {
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  if (it == phases_.end() || it->second.state != State::kStarted ||
      it->second.service_worker_token != service_worker_token) {
    // Nothing to abort if no registration phase is in progress for this worker
    // instance.
    return;
  }
  it->second.state = State::kAborted;
  on_finished_.Run(extension_id, browser_context, State::kAborted);
}

std::optional<ListenerRegistrationPhaseMap::State>
ListenerRegistrationPhaseMap::GetState(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context) const {
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  if (it == phases_.end()) {
    return std::nullopt;
  }
  return it->second.state;
}

bool ListenerRegistrationPhaseMap::IsStarted(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context) const {
  if (phases_.empty()) {
    return false;
  }
  return GetState(extension_id, browser_context) == State::kStarted;
}

ListenerRegistrationPhaseMap::ListenerSet*
ListenerRegistrationPhaseMap::GetListeners(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context) {
  auto it = phases_.find({extension_id, browser_context.UniqueToken()});
  if (it == phases_.end()) {
    return nullptr;
  }
  return &it->second.listeners;
}

void ListenerRegistrationPhaseMap::RemoveAllForExtension(
    const ExtensionId& extension_id) {
  std::erase_if(phases_, [&](const auto& entry) {
    return entry.first.first == extension_id;
  });
}

void ListenerRegistrationPhaseMap::RemoveAllForContext(
    content::BrowserContext& browser_context) {
  const base::UnguessableToken& context_token = browser_context.UniqueToken();
  std::erase_if(phases_, [&](const auto& entry) {
    return entry.first.second == context_token;
  });
}

}  // namespace extensions
