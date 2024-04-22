// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_EVENT_LISTENERS_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_EVENT_LISTENERS_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8.h"

namespace extensions {
class ListenerTracker;

// A base class to hold listeners for a given event. This allows for adding,
// removing, and querying listeners in the list, and calling a callback when
// transitioning from 0 -> 1 or 1 -> 0 listeners.
class APIEventListeners {
 public:
  // The callback called when listeners change. |update_lazy_listeners|
  // indicates that the lazy listener count for the event should potentially be
  // updated. This is true if a) the event supports lazy listeners and b) the
  // change was "manual" (i.e., triggered by a direct call from the extension
  // rather than something like the context being destroyed).
  using ListenersUpdated =
      base::RepeatingCallback<void(const std::string& event_name,
                                   binding::EventListenersChanged,
                                   const base::Value::Dict* filter,
                                   bool update_lazy_listeners,
                                   v8::Local<v8::Context> context)>;

  // A callback to retrieve the identity of the context's owner. This allows us
  // to associate multiple listeners from different v8::Contexts with the same
  // owner (e.g., extension). This is used lazily, when listeners are first
  // added.
  // TODO(crbug.com/41410015): Ideally, we'd just pass in the context
  // owner to the event directly. However, this led to https://crbug.com/877401,
  // presumably because of https://crbug.com/877658. If we can fix that, we can
  // simplify this again.
  using ContextOwnerIdGetter =
      base::RepeatingCallback<std::string(v8::Local<v8::Context>)>;

  APIEventListeners(const APIEventListeners&) = delete;
  APIEventListeners& operator=(const APIEventListeners&) = delete;

  virtual ~APIEventListeners() = default;

  // Adds the given |listener| to the list, possibly associating it with the
  // given |filter|. Returns true if the listener is added. Populates |error|
  // with any errors encountered. Note that |error| is *not* always populated
  // if false is returned, since we don't consider trying to re-add a listener
  // to be an error.
  virtual bool AddListener(v8::Local<v8::Function> listener,
                           v8::Local<v8::Object> filter,
                           v8::Local<v8::Context> context,
                           std::string* error) = 0;

  // Removes the given |listener|, if it's present in the list.
  virtual void RemoveListener(v8::Local<v8::Function> listener,
                              v8::Local<v8::Context> context) = 0;

  // Returns true if the given |listener| is in the list.
  virtual bool HasListener(v8::Local<v8::Function> listener) = 0;

  // Returns the number of listeners in the list.
  virtual size_t GetNumListeners() = 0;

  // Returns the listeners that should be notified for the given |filter|.
  virtual v8::LocalVector<v8::Function> GetListeners(
      mojom::EventFilteringInfoPtr filter,
      v8::Local<v8::Context> context) = 0;

  // Invalidates the list.
  virtual void Invalidate(v8::Local<v8::Context> context) = 0;

 protected:
  APIEventListeners() {}
};

// A listener list implementation that doesn't support filtering. Each event
// dispatched is dispatched to all the associated listeners.
class UnfilteredEventListeners final : public APIEventListeners {
 public:
  UnfilteredEventListeners(ListenersUpdated listeners_updated,
                           const std::string& event_name,
                           ContextOwnerIdGetter context_owner_id_getter,
                           int max_listeners,
                           bool supports_lazy_listeners,
                           ListenerTracker* listener_tracker);

  UnfilteredEventListeners(const UnfilteredEventListeners&) = delete;
  UnfilteredEventListeners& operator=(const UnfilteredEventListeners&) = delete;

  ~UnfilteredEventListeners() override;

  bool AddListener(v8::Local<v8::Function> listener,
                   v8::Local<v8::Object> filter,
                   v8::Local<v8::Context> context,
                   std::string* error) override;
  void RemoveListener(v8::Local<v8::Function> listener,
                      v8::Local<v8::Context> context) override;
  bool HasListener(v8::Local<v8::Function> listener) override;
  size_t GetNumListeners() override;
  v8::LocalVector<v8::Function> GetListeners(
      mojom::EventFilteringInfoPtr filter,
      v8::Local<v8::Context> context) override;
  void Invalidate(v8::Local<v8::Context> context) override;

 private:
  // Lazily sets |context_id_owner_| from |context_id_owner_getter_|.
  void LazilySetContextOwner(v8::Local<v8::Context> context);

  // Notifies that all the listeners for this context are now removed.
  void NotifyListenersEmpty(v8::Local<v8::Context> context,
                            bool update_lazy_listeners);

  // The event listeners associated with this event.
  // TODO(devlin): Having these listeners held as v8::Globals means that we
  // need to worry about cycles when a listener holds a reference to the event,
  // e.g. EventEmitter -> Listener -> EventEmitter. Right now, we handle that by
  // requiring Invalidate() to be called, but that means that events that aren't
  // Invalidate()'d earlier can leak until context destruction. We could
  // circumvent this by storing the listeners strongly in a private propery
  // (thus traceable by v8), and optionally keep a weak cache on this object.
  std::vector<v8::Global<v8::Function>> listeners_;

  ListenersUpdated listeners_updated_;

  // This event's name.
  std::string event_name_;

  // The getter for the owner of the context; called lazily when the first
  // listener is added. Only used when the event is managed (i.e.,
  // |listener_tracker_ is non-null|).
  ContextOwnerIdGetter context_owner_id_getter_;

  // The owner of the context these listeners belong to. Only used when the
  // event is managed (i.e., |listener_tracker_ is non-null|).
  std::string context_owner_id_;

  // The maximum number of supported listeners.
  int max_listeners_;

  // Whether the event supports lazy listeners.
  bool supports_lazy_listeners_;

  // The listener tracker to notify of added or removed listeners. This may be
  // null if this is a set of listeners for an unmanaged event. If
  // non-null, required to outlive this object.
  raw_ptr<ListenerTracker, DanglingUntriaged> listener_tracker_ = nullptr;
};

// A listener list implementation that supports filtering. Events should only
// be dispatched to those listeners whose filters match. Additionally, the
// updated callback is triggered any time a listener with a new filter is
// added, or the last listener with a given filter is removed.
class FilteredEventListeners final : public APIEventListeners {
 public:
  FilteredEventListeners(ListenersUpdated listeners_updated,
                         const std::string& event_name,
                         ContextOwnerIdGetter context_owner_id_getter,
                         int max_listeners,
                         bool supports_lazy_listeners,
                         ListenerTracker* listener_tracker);

  FilteredEventListeners(const FilteredEventListeners&) = delete;
  FilteredEventListeners& operator=(const FilteredEventListeners&) = delete;

  ~FilteredEventListeners() override;

  bool AddListener(v8::Local<v8::Function> listener,
                   v8::Local<v8::Object> filter,
                   v8::Local<v8::Context> context,
                   std::string* error) override;
  void RemoveListener(v8::Local<v8::Function> listener,
                      v8::Local<v8::Context> context) override;
  bool HasListener(v8::Local<v8::Function> listener) override;
  size_t GetNumListeners() override;
  v8::LocalVector<v8::Function> GetListeners(
      mojom::EventFilteringInfoPtr filter,
      v8::Local<v8::Context> context) override;
  void Invalidate(v8::Local<v8::Context> context) override;

 private:
  struct ListenerData;

  // Lazily sets |context_owner_id_| from |context_owner_id_getter_|.
  void LazilySetContextOwner(v8::Local<v8::Context> context);

  void InvalidateListener(const ListenerData& listener,
                          bool was_manual,
                          v8::Local<v8::Context> context);

  // Note: See TODO on UnfilteredEventListeners::listeners_.
  std::vector<ListenerData> listeners_;

  ListenersUpdated listeners_updated_;

  // This event's name.
  std::string event_name_;

  // The getter for the owner of the context; called lazily when the first
  // listener is added. This is always non-null.
  ContextOwnerIdGetter context_owner_id_getter_;

  // The owner of the context these listeners belong to. This should always be
  // non-empty after initialization.
  std::string context_owner_id_;

  // The maximum number of supported listeners.
  int max_listeners_;

  // Whether the event supports lazy listeners.
  bool supports_lazy_listeners_;

  // The listener tracker to notify of added or removed listeners. Required to
  // outlive this object. Must be non-null.
  raw_ptr<ListenerTracker> listener_tracker_ = nullptr;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_EVENT_LISTENERS_H_
