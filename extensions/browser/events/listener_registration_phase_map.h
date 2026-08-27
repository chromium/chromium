// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_
#define EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_

#include <map>
#include <optional>
#include <utility>

#include "base/unguessable_token.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Tracks the listener registration phases of extension service workers.
//
// Every service worker instance of an extension using
// `background.async_listener_registration` has a listener registration
// phase. The phase begins when the worker context initializes and ends when
// the extension calls `runtime.markListenerRegistrationComplete()` (commit)
// or when the worker stops beforehand (abort). EventRouter owns this map and
// ServiceWorkerTaskQueue drives the transitions.
//
// The map retains the latest phase for each key until replaced by a new
// instance, or removed on extension unload or context shutdown. This allows
// distinguishing a late signal from a finished worker instance from one for a
// phase that never started.
//
// Phases are keyed by extension ID and the unique token of the worker's
// BrowserContext (`content::BrowserContext::UniqueToken()`) to keep
// split-mode incognito worker instances separate.
class ListenerRegistrationPhaseMap {
 public:
  // The state of a listener registration phase.
  enum class State {
    // The worker context initialized and is registering listeners.
    kStarted,
    // The extension called `runtime.markListenerRegistrationComplete()`.
    kCommitted,
    // The worker stopped (or was replaced by a new instance) before completion.
    kAborted,
  };

  ListenerRegistrationPhaseMap();
  ~ListenerRegistrationPhaseMap();

  ListenerRegistrationPhaseMap(const ListenerRegistrationPhaseMap&) = delete;
  ListenerRegistrationPhaseMap& operator=(const ListenerRegistrationPhaseMap&) =
      delete;

  // Starts a registration phase for a worker instance of `extension_id`,
  // replacing any existing phase for this key (e.g. from a previous instance
  // whose stop signal was not received).
  void Start(const ExtensionId& extension_id,
             content::BrowserContext& browser_context,
             const blink::ServiceWorkerToken& service_worker_token);

  // Commits the registration phase for `extension_id`. Returns false if no
  // phase is currently started, or if `service_worker_token` does not match
  // the active worker instance.
  bool Commit(const ExtensionId& extension_id,
              content::BrowserContext& browser_context,
              const blink::ServiceWorkerToken& service_worker_token);

  // Aborts any started registration phase for `extension_id`.
  void Abort(const ExtensionId& extension_id,
             content::BrowserContext& browser_context);

  // Aborts the registration phase for `extension_id` if it is started and
  // matches the worker instance identified by `service_worker_token`.
  void AbortForInstance(const ExtensionId& extension_id,
                        content::BrowserContext& browser_context,
                        const blink::ServiceWorkerToken& service_worker_token);

  // Returns the state of the latest registration phase for `extension_id`, or
  // std::nullopt if no phase exists.
  std::optional<State> GetState(const ExtensionId& extension_id,
                                content::BrowserContext& browser_context) const;

  // Removes registration phases for `extension_id` across all BrowserContexts
  // on extension unload.
  void RemoveAllForExtension(const ExtensionId& extension_id);

  // Removes all registration phases in `browser_context` on context shutdown.
  void RemoveAllForContext(content::BrowserContext& browser_context);

 private:
  struct Phase {
    blink::ServiceWorkerToken service_worker_token;
    State state = State::kStarted;
  };

  // Keyed by extension ID and the unique token of the worker's BrowserContext.
  std::map<std::pair<ExtensionId, base::UnguessableToken>, Phase> phases_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_
