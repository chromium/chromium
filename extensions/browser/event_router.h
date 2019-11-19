// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_EVENT_ROUTER_H_

#include <set>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/events/event_ack_data.h"
#include "extensions/browser/events/lazy_event_dispatch_util.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/constants.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/features/feature.h"
#include "ipc/ipc_sender.h"
#include "url/gurl.h"

class GURL;
struct ServiceWorkerIdentifier;

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {
class Extension;
class ExtensionPrefs;

struct Event;
struct EventListenerInfo;

// TODO(lazyboy): Document how extension events work, including how listeners
// are registered and how listeners are tracked in renderer and browser process.
class EventRouter : public KeyedService,
                    public ExtensionRegistryObserver,
                    public EventListenerMap::Delegate,
                    public content::RenderProcessHostObserver {
 public:
  // These constants convey the state of our knowledge of whether we're in
  // a user-caused gesture as part of DispatchEvent.
  enum UserGestureState {
    USER_GESTURE_UNKNOWN = 0,
    USER_GESTURE_ENABLED = 1,
    USER_GESTURE_NOT_ENABLED = 2,
  };

  // The pref key for the list of event names for which an extension has
  // registered from its lazy background page.
  static const char kRegisteredLazyEvents[];
  // The pref key for the list of event names for which an extension has
  // registered from its service worker.
  static const char kRegisteredServiceWorkerEvents[];

  // Observers register interest in events with a particular name and are
  // notified when a listener is added or removed. Observers are matched by
  // the base name of the event (e.g. adding an event listener for event name
  // "foo.onBar/123" will trigger observers registered for "foo.onBar").
  class Observer {
   public:
    // Called when a listener is added.
    virtual void OnListenerAdded(const EventListenerInfo& details) {}
    // Called when a listener is removed.
    virtual void OnListenerRemoved(const EventListenerInfo& details) {}

   protected:
    virtual ~Observer() {}
  };

  // A test observer to monitor event dispatching.
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnWillDispatchEvent(const Event& event) = 0;
    virtual void OnDidDispatchEventToProcess(const Event& event) = 0;
  };

  // Gets the EventRouter for |browser_context|.
  static EventRouter* Get(content::BrowserContext* browser_context);

  // Converts event names like "foo.onBar/123" into "foo.onBar". Event names
  // without a "/" are returned unchanged.
  static std::string GetBaseEventName(const std::string& full_event_name);

  // Sends an event via ipc_sender to the given extension. Can be called on any
  // thread.
  //
  // It is very rare to call this function directly. Instead use the instance
  // methods BroadcastEvent or DispatchEventToExtension.
  // Note that this method will dispatch the event with
  // UserGestureState:USER_GESTURE_UNKNOWN.
  static void DispatchEventToSender(IPC::Sender* ipc_sender,
                                    void* browser_context_id,
                                    const std::string& extension_id,
                                    events::HistogramValue histogram_value,
                                    const std::string& event_name,
                                    int render_process_id,
                                    int worker_thread_id,
                                    int64_t service_worker_version_id,
                                    std::unique_ptr<base::ListValue> event_args,
                                    const EventFilteringInfo& info);

  // Returns false when the event is scoped to a context and the listening
  // extension does not have access to events from that context.
  static bool CanDispatchEventToBrowserContext(content::BrowserContext* context,
                                               const Extension* extension,
                                               const Event& event);

  // An EventRouter is shared between |browser_context| and its associated
  // incognito context. |extension_prefs| may be NULL in tests.
  EventRouter(content::BrowserContext* browser_context,
              ExtensionPrefs* extension_prefs);
  ~EventRouter() override;

  // Add or remove an extension as an event listener for |event_name|.
  //
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        content::RenderProcessHost* process,
                        const ExtensionId& extension_id);
  void AddServiceWorkerEventListener(const std::string& event_name,
                                     content::RenderProcessHost* process,
                                     const ExtensionId& extension_id,
                                     const GURL& service_worker_scope,
                                     int64_t service_worker_version_id,
                                     int worker_thread_id);
  void RemoveEventListener(const std::string& event_name,
                           content::RenderProcessHost* process,
                           const ExtensionId& extension_id);
  void RemoveServiceWorkerEventListener(const std::string& event_name,
                                        content::RenderProcessHost* process,
                                        const ExtensionId& extension_id,
                                        const GURL& service_worker_scope,
                                        int64_t service_worker_version_id,
                                        int worker_thread_id);

  // Add or remove a URL as an event listener for |event_name|.
  void AddEventListenerForURL(const std::string& event_name,
                              content::RenderProcessHost* process,
                              const GURL& listener_url);
  void RemoveEventListenerForURL(const std::string& event_name,
                                 content::RenderProcessHost* process,
                                 const GURL& listener_url);

  EventListenerMap& listeners() { return listeners_; }

  // Registers an observer to be notified when an event listener for
  // |event_name| is added or removed. There can currently be only one observer
  // for each distinct |event_name|.
  void RegisterObserver(Observer* observer, const std::string& event_name);

  // Unregisters an observer from all events.
  void UnregisterObserver(Observer* observer);

  // Adds/removes test observers.
  void AddObserverForTesting(TestObserver* observer);
  void RemoveObserverForTesting(TestObserver* observer);

  // Add or remove the extension as having a lazy background page that listens
  // to the event. The difference from the above methods is that these will be
  // remembered even after the process goes away. We use this list to decide
  // which extension pages to load when dispatching an event.
  void AddLazyEventListener(const std::string& event_name,
                            const ExtensionId& extension_id);
  void RemoveLazyEventListener(const std::string& event_name,
                               const ExtensionId& extension_id);
  // Similar to Add/RemoveLazyEventListener, but applies to extension service
  // workers.
  void AddLazyServiceWorkerEventListener(const std::string& event_name,
                                         const ExtensionId& extension_id,
                                         const GURL& service_worker_scope);
  void RemoveLazyServiceWorkerEventListener(const std::string& event_name,
                                            const ExtensionId& extension_id,
                                            const GURL& service_worker_scope);

  // If |add_lazy_listener| is true also add the lazy version of this listener.
  void AddFilteredEventListener(
      const std::string& event_name,
      content::RenderProcessHost* process,
      const std::string& extension_id,
      base::Optional<ServiceWorkerIdentifier> sw_identifier,
      const base::DictionaryValue& filter,
      bool add_lazy_listener);

  // If |remove_lazy_listener| is true also remove the lazy version of this
  // listener.
  void RemoveFilteredEventListener(
      const std::string& event_name,
      content::RenderProcessHost* process,
      const std::string& extension_id,
      base::Optional<ServiceWorkerIdentifier> sw_identifier,
      const base::DictionaryValue& filter,
      bool remove_lazy_listener);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name) const;

  // Returns true if the extension is listening to the given event.
  // (virtual for testing only.)
  virtual bool ExtensionHasEventListener(const std::string& extension_id,
                                         const std::string& event_name) const;

  // Broadcasts an event to every listener registered for that event.
  virtual void BroadcastEvent(std::unique_ptr<Event> event);

  // Dispatches an event to the given extension.
  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        std::unique_ptr<Event> event);

  // Dispatches |event| to the given extension as if the extension has a lazy
  // listener for it. NOTE: This should be used rarely, for dispatching events
  // to extensions that haven't had a chance to add their own listeners yet, eg:
  // newly installed extensions.
  void DispatchEventWithLazyListener(const std::string& extension_id,
                                     std::unique_ptr<Event> event);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnEventAck(content::BrowserContext* context,
                  const std::string& extension_id,
                  const std::string& event_name);

  // Returns whether or not the given extension has any registered events.
  bool HasRegisteredEvents(const ExtensionId& extension_id) const;

  // Clears registered events for testing purposes.
  void ClearRegisteredEventsForTest(const ExtensionId& extension_id);

  // Reports UMA for an event dispatched to |extension| with histogram value
  // |histogram_value|. Must be called on the UI thread.
  //
  // |did_enqueue| should be true if the event was queued waiting for a process
  // to start, like an event page.
  void ReportEvent(events::HistogramValue histogram_value,
                   const Extension* extension,
                   bool did_enqueue);

  LazyEventDispatchUtil* lazy_event_dispatch_util() {
    return &lazy_event_dispatch_util_;
  }

  EventAckData* event_ack_data() { return &event_ack_data_; }

  // Returns true if there is a registered lazy/non-lazy listener for the given
  // |event_name|.
  bool HasLazyEventListenerForTesting(const std::string& event_name);
  bool HasNonLazyEventListenerForTesting(const std::string& event_name);

 private:
  friend class EventRouterFilterTest;
  friend class EventRouterTest;

  enum class RegisteredEventType {
    kLazy,
    kServiceWorker,
  };

  // TODO(gdk): Document this.
  static void DispatchExtensionMessage(
      IPC::Sender* ipc_sender,
      int worker_thread_id,
      void* browser_context_id,
      const std::string& extension_id,
      int event_id,
      const std::string& event_name,
      base::ListValue* event_args,
      UserGestureState user_gesture,
      const extensions::EventFilteringInfo& info);

  // Returns or sets the list of events for which the given extension has
  // registered.
  std::set<std::string> GetRegisteredEvents(const std::string& extension_id,
                                            RegisteredEventType type) const;
  void SetRegisteredEvents(const std::string& extension_id,
                           const std::set<std::string>& events,
                           RegisteredEventType type);

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  void AddLazyEventListenerImpl(std::unique_ptr<EventListener> listener,
                                RegisteredEventType type);
  void RemoveLazyEventListenerImpl(std::unique_ptr<EventListener> listener,
                                   RegisteredEventType type);

  // Shared by all event dispatch methods. If |restrict_to_extension_id| is
  // empty, the event is broadcast.  An event that just came off the pending
  // list may not be delayed again.
  void DispatchEventImpl(const std::string& restrict_to_extension_id,
                         std::unique_ptr<Event> event);

  // Dispatches the event to the specified extension or URL running in
  // |process|.
  void DispatchEventToProcess(const std::string& extension_id,
                              const GURL& listener_url,
                              content::RenderProcessHost* process,
                              int64_t service_worker_version_id,
                              int worker_thread_id,
                              Event* event,
                              const base::DictionaryValue* listener_filter,
                              bool did_enqueue);

  // Adds a filter to an event.
  void AddFilterToEvent(const std::string& event_name,
                        const std::string& extension_id,
                        bool is_for_service_worker,
                        const base::DictionaryValue* filter);

  // Removes a filter from an event.
  void RemoveFilterFromEvent(const std::string& event_name,
                             const std::string& extension_id,
                             bool is_for_service_worker,
                             const base::DictionaryValue* filter);

  // Returns the dictionary of event filters that the given extension has
  // registered.
  const base::DictionaryValue* GetFilteredEvents(
      const std::string& extension_id,
      RegisteredEventType type);

  // Track the dispatched events that have not yet sent an ACK from the
  // renderer.
  void IncrementInFlightEvents(content::BrowserContext* context,
                               content::RenderProcessHost* process,
                               const Extension* extension,
                               int event_id,
                               const std::string& event_name,
                               int64_t service_worker_version_id);

  // static
  static void DoDispatchEventToSenderBookkeepingOnUI(
      void* browser_context_id,
      const std::string& extension_id,
      int event_id,
      int render_process_id,
      int64_t service_worker_version_id,
      events::HistogramValue histogram_value,
      const std::string& event_name);

  void DispatchPendingEvent(
      std::unique_ptr<Event> event,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> params);

  // Implementation of EventListenerMap::Delegate.
  void OnListenerAdded(const EventListener* listener) override;
  void OnListenerRemoved(const EventListener* listener) override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  content::BrowserContext* const browser_context_;

  // The ExtensionPrefs associated with |browser_context_|. May be NULL in
  // tests.
  ExtensionPrefs* const extension_prefs_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  EventListenerMap listeners_{this};

  // Map from base event name to observer.
  using ObserverMap = std::unordered_map<std::string, Observer*>;
  ObserverMap observers_;

  base::ObserverList<TestObserver>::Unchecked test_observers_;

  std::set<content::RenderProcessHost*> observed_process_set_;

  LazyEventDispatchUtil lazy_event_dispatch_util_;

  EventAckData event_ack_data_;

  base::WeakPtrFactory<EventRouter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventRouter);
};

struct Event {
  // This callback should return true if the event should be dispatched to the
  // given context and extension, and false otherwise.
  using WillDispatchCallback =
      base::RepeatingCallback<bool(content::BrowserContext*,
                                   Feature::Context,
                                   const Extension*,
                                   Event*,
                                   const base::DictionaryValue*)>;

  // The identifier for the event, for histograms. In most cases this
  // correlates 1:1 with |event_name|, in some cases events will generate
  // their own names, but they cannot generate their own identifier.
  const events::HistogramValue histogram_value;

  // The event to dispatch.
  const std::string event_name;

  // Arguments to send to the event listener.
  std::unique_ptr<base::ListValue> event_args;

  // If non-null, then the event will not be sent to other BrowserContexts
  // unless the extension has permission (e.g. incognito tab update -> normal
  // tab only works if extension is allowed incognito access).
  content::BrowserContext* const restrict_to_browser_context;

  // If not empty, the event is only sent to extensions with host permissions
  // for this url.
  GURL event_url;

  // Whether a user gesture triggered the event.
  EventRouter::UserGestureState user_gesture;

  // Extra information used to filter which events are sent to the listener.
  EventFilteringInfo filter_info;

  // If specified, this is called before dispatching an event to each
  // extension. The third argument is a mutable reference to event_args,
  // allowing the caller to provide different arguments depending on the
  // extension and profile. This is guaranteed to be called synchronously with
  // DispatchEvent, so callers don't need to worry about lifetime.
  //
  // NOTE: the Extension argument to this may be NULL because it's possible for
  // this event to be dispatched to non-extension processes, like WebUI.
  WillDispatchCallback will_dispatch_callback;

  // TODO(lazyboy): This sets |restrict_to_browser_context| to nullptr, this
  // will dispatch the event to unrelated profiles, not just incognito. Audit
  // and limit usages of this constructor and introduce "include incognito"
  // option to a constructor version for clients that need to disptach events to
  // related browser_contexts. See https://crbug.com/726022.
  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args);

  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args,
        content::BrowserContext* restrict_to_browser_context);

  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args,
        content::BrowserContext* restrict_to_browser_context,
        const GURL& event_url,
        EventRouter::UserGestureState user_gesture,
        const EventFilteringInfo& info);

  ~Event();

  // Makes a deep copy of this instance.
  std::unique_ptr<Event> DeepCopy() const;
};

struct EventListenerInfo {
  // Constructor for a listener from a non-ServiceWorker context (background
  // page, popup, tab, etc)
  EventListenerInfo(const std::string& event_name,
                    const std::string& extension_id,
                    const GURL& listener_url,
                    content::BrowserContext* browser_context);

  // Constructor for a listener from a ServiceWorker context.
  EventListenerInfo(const std::string& event_name,
                    const std::string& extension_id,
                    const GURL& listener_url,
                    content::BrowserContext* browser_context,
                    int worker_thread_id,
                    int64_t service_worker_version_id);

  // The event name including any sub-event, e.g. "runtime.onStartup" or
  // "webRequest.onCompleted/123".
  const std::string event_name;
  const std::string extension_id;
  const GURL listener_url;
  content::BrowserContext* const browser_context;
  const int worker_thread_id;
  const int64_t service_worker_version_id;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENT_ROUTER_H_
