// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_
#define EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "base/values.h"
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
// phase that never started. Each phase also tracks the worker's lazy listeners.
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

  // Tracks a set of lazy listeners using the same representation as
  // ExtensionPrefs: unfiltered event names in `events`, and a dict mapping
  // event names to lists of filter dicts in `filtered_events`.
  //
  // For `Has()`, `Add()`, and `Remove()`, a null `filter` refers to an
  // unfiltered listener.
  struct ListenerSet {
    bool Has(const std::string& event_name,
             const base::DictValue* filter) const;
    void Add(const std::string& event_name, const base::DictValue* filter);
    void Remove(const std::string& event_name, const base::DictValue* filter);
    void AddAll(const ListenerSet& other);

    ListenerSet Clone() const;

    bool operator==(const ListenerSet&) const = default;

    std::set<std::string> events;
    base::DictValue filtered_events;
  };

  using OnFinishedCallback = base::RepeatingCallback<
      void(const ExtensionId&, content::BrowserContext&, State)>;

  // `on_finished` is called when `Commit()`, `Abort()`, or
  // `AbortForInstance()` finishes a started phase (not when `Start()` replaces
  // a phase or `RemoveAllFor*()` erases it).
  explicit ListenerRegistrationPhaseMap(OnFinishedCallback on_finished);
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

  // Returns true if the latest registration phase for `extension_id` is
  // started.
  bool IsStarted(const ExtensionId& extension_id,
                 content::BrowserContext& browser_context) const;

  // Returns the listener set for the latest registration phase of
  // `extension_id`, or nullptr if no phase exists. `EventRouter` updates this
  // set as the worker adds and removes lazy listeners.
  ListenerSet* GetListeners(const ExtensionId& extension_id,
                            content::BrowserContext& browser_context);

  // Removes registration phases for `extension_id` across all BrowserContexts
  // on extension unload.
  void RemoveAllForExtension(const ExtensionId& extension_id);

  // Removes all registration phases in `browser_context` on context shutdown.
  void RemoveAllForContext(content::BrowserContext& browser_context);

 private:
  struct Phase {
    blink::ServiceWorkerToken service_worker_token;
    State state = State::kStarted;
    ListenerSet listeners;
  };

  OnFinishedCallback on_finished_;

  // Keyed by extension ID and the unique token of the worker's BrowserContext.
  std::map<std::pair<ExtensionId, base::UnguessableToken>, Phase> phases_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_LISTENER_REGISTRATION_PHASE_MAP_H_
