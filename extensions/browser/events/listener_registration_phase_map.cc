// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/listener_registration_phase_map.h"

#include "base/check.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ListenerRegistrationPhaseMap::ListenerRegistrationPhaseMap() = default;

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
  // TODO(crbug.com/509627729): Replace the persisted routing state with the
  // set of listeners registered during this instance.
  it->second.state = State::kCommitted;
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
