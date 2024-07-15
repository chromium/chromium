// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_EVENT_ROUTER_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
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
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "url/gurl.h"

class GURL;

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace ash {
namespace file_system_provider {
class FileSystemProviderProvidedFileSystemTest;
}  // namespace file_system_provider
}  // namespace ash

namespace extensions {
class Extension;
class ExtensionPrefs;

struct Event;
struct EventListenerInfo;

// Which flow an event follows when being sent to the event router. Used in
// UMA histogram logic.
enum class EventDispatchSource : int {
  // Event went through EventRouter::DispatchEventToProcess() dispatch flow.
  kDispatchEventToProcess,
  // Event went through EventRouter::DispatchEventToSender() dispatch flow.
  kDispatchEventToSender,
};

// The upper bound of time allowed for event dispatch histograms. Also used in
// histograms for determining when an event is "late" (it has not been acked by
// the renderer to the browser by this time).
inline base::TimeDelta kEventAckMetricTimeLimit = base::Minutes(5);

// TODO(lazyboy): Document how extension events work, including how listeners
// are registered and how listeners are tracked in renderer and browser process.
class EventRouter : public KeyedService,
                    public ExtensionRegistryObserver,
                    public EventListenerMap::Delegate,
                    public content::RenderProcessHostObserver,
                    public mojom::EventRouter {
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
  class Observer : public base::CheckedObserver {
   public:
    // Called when a listener is added.
    virtual void OnListenerAdded(const EventListenerInfo& details) {}
    // Called when a listener is removed.
    virtual void OnListenerRemoved(const EventListenerInfo& details) {}

   protected:
    ~Observer() override = default;
  };

  // A test observer to monitor event dispatching.
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnWillDispatchEvent(const Event& event) = 0;
    virtual void OnDidDispatchEventToProcess(const Event& event,
                                             int process_id) = 0;
    virtual void OnNonExtensionEventDispatched(const std::string& event_name) {}
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
  // Note: `browser_context` may differ from `browser_context_` since the
  // `EventRouter` is shared between on- and off-the-record contexts.
  void DispatchEventToSender(content::RenderProcessHost* rph,
                             content::BrowserContext* browser_context,
                             const mojom::HostID& host_id,
                             events::HistogramValue histogram_value,
                             const std::string& event_name,
                             int worker_thread_id,
                             int64_t service_worker_version_id,
                             base::Value::List event_args,
                             mojom::EventFilteringInfoPtr info);

  // Returns false when the event is scoped to a context and the listening
  // extension does not have access to events from that context.
  static bool CanDispatchEventToBrowserContext(content::BrowserContext* context,
                                               const Extension* extension,
                                               const Event& event);

  static void BindForRenderer(
      int process_id,
      mojo::PendingAssociatedReceiver<mojom::EventRouter> receiver);

  // An EventRouter is shared between |browser_context| and its associated
  // incognito context. |extension_prefs| may be NULL in tests.
  EventRouter(content::BrowserContext* browser_context,
              ExtensionPrefs* extension_prefs);

  EventRouter(const EventRouter&) = delete;
  EventRouter& operator=(const EventRouter&) = delete;

  ~EventRouter() override;

  // mojom::EventRouter:
  void AddListenerForMainThread(
      mojom::EventListenerPtr event_listener) override;

  void AddListenerForServiceWorker(
      mojom::EventListenerPtr event_listener) override;

  void AddLazyListenerForMainThread(const ExtensionId& extension_id,
                                    const std::string& name) override;

  void AddLazyListenerForServiceWorker(const ExtensionId& extension_id,
                                       const GURL& worker_scope_url,
                                       const std::string& name) override;

  void AddFilteredListenerForMainThread(
      mojom::EventListenerOwnerPtr listener_owner,
      const std::string& name,
      base::Value::Dict filter,
      bool add_lazy_listener) override;

  void AddFilteredListenerForServiceWorker(
      const ExtensionId& extension_id,
      const std::string& name,
      mojom::ServiceWorkerContextPtr service_worker_context,
      base::Value::Dict filter,
      bool add_lazy_listener) override;

  void RemoveListenerForMainThread(
      mojom::EventListenerPtr event_listener) override;

  void RemoveListenerForServiceWorker(
      mojom::EventListenerPtr event_listener) override;

  void RemoveLazyListenerForMainThread(const ExtensionId& extension_id,
                                       const std::string& name) override;

  void RemoveLazyListenerForServiceWorker(const ExtensionId& extension_id,
                                          const GURL& worker_scope_url,
                                          const std::string& name) override;

  void RemoveFilteredListenerForMainThread(
      mojom::EventListenerOwnerPtr listener_owner,
      const std::string& name,
      base::Value::Dict filter,
      bool remove_lazy_listener) override;

  void RemoveFilteredListenerForServiceWorker(
      const ExtensionId& extension_id,
      const std::string& name,
      mojom::ServiceWorkerContextPtr service_worker_context,
      base::Value::Dict filter,
      bool remove_lazy_listener) override;

  // Removes an extension as an event listener for |event_name|.
  //
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void RemoveEventListener(const std::string& event_name,
                           content::RenderProcessHost* process,
                           const ExtensionId& extension_id);
  void RemoveServiceWorkerEventListener(mojom::EventListenerPtr event_listener,
                                        content::RenderProcessHost* process);

  // Add or remove a URL as an event listener for |event_name|.
  void AddEventListenerForURL(const std::string& event_name,
                              content::RenderProcessHost* process,
                              const GURL& listener_url);
  void RemoveEventListenerForURL(const std::string& event_name,
                                 content::RenderProcessHost* process,
                                 const GURL& listener_url);

  EventListenerMap& listeners() { return listeners_; }

  // Registers an observer to be notified when an event listener for
  // |event_name| is added or removed. There can currently be multiple
  // observers for each distinct |event_name|.
  void RegisterObserver(Observer* observer, const std::string& event_name);

  // Unregisters an observer from all events.
  void UnregisterObserver(Observer* observer);

  // Adds/removes test observers.
  void AddObserverForTesting(TestObserver* observer);
  void RemoveObserverForTesting(TestObserver* observer);

  // If |add_lazy_listener| is true also add the lazy version of this listener.
  void AddFilteredEventListener(
      const std::string& event_name,
      content::RenderProcessHost* process,
      mojom::EventListenerOwnerPtr listener_owner,
      mojom::ServiceWorkerContext* service_worker_context,
      const base::Value::Dict& filter,
      bool add_lazy_listener);

  // If |remove_lazy_listener| is true also remove the lazy version of this
  // listener.
  void RemoveFilteredEventListener(
      const std::string& event_name,
      content::RenderProcessHost* process,
      mojom::EventListenerOwnerPtr listener_owner,
      mojom::ServiceWorkerContext* service_worker_context,
      const base::Value::Dict& filter,
      bool remove_lazy_listener);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name) const;

  // Returns true if the extension is listening to the given event.
  // (virtual for testing only.)
  virtual bool ExtensionHasEventListener(const ExtensionId& extension_id,
                                         const std::string& event_name) const;

  // Returns true if the URL is listening to the given event.
  // (virtual for testing only.)
  virtual bool URLHasEventListener(const GURL& url,
                                   const std::string& event_name) const;

  // Broadcasts an event to every listener registered for that event.
  virtual void BroadcastEvent(std::unique_ptr<Event> event);

  // Dispatches an event to the given extension.
  virtual void DispatchEventToExtension(const ExtensionId& extension_id,
                                        std::unique_ptr<Event> event);

  // Dispatches an event to the given url.
  virtual void DispatchEventToURL(const GURL& owner_url,
                                  std::unique_ptr<Event> event);

  // Dispatches |event| to the given extension as if the extension has a lazy
  // listener for it. NOTE: This should be used rarely, for dispatching events
  // to extensions that haven't had a chance to add their own listeners yet, eg:
  // newly installed extensions.
  void DispatchEventWithLazyListener(const ExtensionId& extension_id,
                                     std::unique_ptr<Event> event);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnEventAck(content::BrowserContext* context,
                  const ExtensionId& extension_id,
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

  void BindServiceWorkerEventDispatcher(
      int render_process_id,
      int worker_thread_id,
      mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher);

  void SetEventAckMetricTimeLimitForTesting(base::TimeDelta time_limit) {
    kEventAckMetricTimeLimit = time_limit;
  }

 private:
  friend class EventRouterFilterTest;
  friend class EventRouterTest;
  friend class ash::file_system_provider::
      FileSystemProviderProvidedFileSystemTest;
  friend class UpdateInstallGateTest;
  friend class DownloadExtensionTest;
  friend class SystemInfoAPITest;
  FRIEND_TEST_ALL_PREFIXES(EventRouterTest, MultipleEventRouterObserver);
  FRIEND_TEST_ALL_PREFIXES(EventRouterDispatchTest, TestDispatch);
  FRIEND_TEST_ALL_PREFIXES(EventRouterDispatchTest, TestDispatchCallback);
  FRIEND_TEST_ALL_PREFIXES(
      DeveloperPrivateApiUnitTest,
      UpdateHostAccess_UnrequestedHostsDispatchUpdateEvents);
  FRIEND_TEST_ALL_PREFIXES(DeveloperPrivateApiUnitTest,
                           ExtensionUpdatedEventOnPermissionsChange);
  FRIEND_TEST_ALL_PREFIXES(DeveloperPrivateApiUnitTest,
                           OnUserSiteSettingsChanged);
  FRIEND_TEST_ALL_PREFIXES(DeveloperPrivateApiUnitTest,
                           ExtensionUpdatedEventOnPinnedActionsChange);
  FRIEND_TEST_ALL_PREFIXES(DeveloperPrivateApiAllowlistUnitTest,
                           ExtensionUpdatedEventOnAllowlistWarningChange);
  FRIEND_TEST_ALL_PREFIXES(DeveloperPrivateApiWithPermittedSitesUnitTest,
                           OnUserSiteSettingsChanged);
  FRIEND_TEST_ALL_PREFIXES(StorageApiUnittest, StorageAreaOnChanged);
  FRIEND_TEST_ALL_PREFIXES(StorageApiUnittest,
                           StorageAreaOnChangedOtherListener);
  FRIEND_TEST_ALL_PREFIXES(StorageApiUnittest,
                           StorageAreaOnChangedOnlyOneListener);
  FRIEND_TEST_ALL_PREFIXES(WMDesksPrivateEventsUnitTest,
                           DispatchEventOnDeskAdded);
  FRIEND_TEST_ALL_PREFIXES(WMDesksPrivateEventsUnitTest,
                           DispatchEventOnDeskRemoved);
  FRIEND_TEST_ALL_PREFIXES(WMDesksPrivateEventsUnitTest,
                           DispatchEventOnDeskSwitched);
  FRIEND_TEST_ALL_PREFIXES(WMDesksPrivateEventsUnitTest,
                           DispatchEventOnDeskRemovalUndone);

  enum class RegisteredEventType {
    kLazy,
    kServiceWorker,
  };

  // TODO(gdk): Document this.
  void DispatchExtensionMessage(
      content::RenderProcessHost* rph,
      int worker_thread_id,
      content::BrowserContext* browser_context,
      const mojom::HostID& host_id,
      int event_id,
      const std::string& event_name,
      base::Value::List event_args,
      UserGestureState user_gesture,
      extensions::mojom::EventFilteringInfoPtr info,
      mojom::EventDispatcher::DispatchEventCallback callback);

  void ObserveProcess(content::RenderProcessHost* process);
  content::RenderProcessHost* GetRenderProcessHostForCurrentReceiver();

  // Gets off-the-record browser context if
  //     - The extension has incognito mode set to "split"
  //     - The on-the-record browser context has an off-the-record context
  //       attached
  content::BrowserContext* GetIncognitoContextIfAccessible(
      const ExtensionId& extension_id);

  // Returns the off-the-record context for the BrowserContext associated
  // with this EventRouter, if any.
  content::BrowserContext* GetIncognitoContext();

  // Adds an extension as an event listener for |event_name|.
  //
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        content::RenderProcessHost* process,
                        const ExtensionId& extension_id);
  void AddServiceWorkerEventListener(mojom::EventListenerPtr event_listener,
                                     content::RenderProcessHost* process);

  // Returns or sets the list of events for which the given extension has
  // registered.
  std::set<std::string> GetRegisteredEvents(const ExtensionId& extension_id,
                                            RegisteredEventType type) const;
  void SetRegisteredEvents(const ExtensionId& extension_id,
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

  // Shared by all event dispatch methods. If |restrict_to_extension_id|  and
  // |restrict_to_url| is empty, the event is broadcast.  An event that just
  // came off the pending list may not be delayed again.
  void DispatchEventImpl(const std::string& restrict_to_extension_id,
                         const GURL& restrict_to_url,
                         std::unique_ptr<Event> event);

  // Dispatches the event to the specified extension or URL running in
  // |process|.
  void DispatchEventToProcess(const ExtensionId& extension_id,
                              const GURL& listener_url,
                              content::RenderProcessHost* process,
                              int64_t service_worker_version_id,
                              int worker_thread_id,
                              const Event& event,
                              const base::Value::Dict* listener_filter,
                              bool did_enqueue);

  // Adds a filter to an event.
  void AddFilterToEvent(const std::string& event_name,
                        const ExtensionId& extension_id,
                        bool is_for_service_worker,
                        const base::Value::Dict& filter);

  // Removes a filter from an event.
  void RemoveFilterFromEvent(const std::string& event_name,
                             const ExtensionId& extension_id,
                             bool is_for_service_worker,
                             const base::Value::Dict& filter);

  // Returns the dictionary of event filters that the given extension has
  // registered.
  const base::Value::Dict* GetFilteredEvents(const ExtensionId& extension_id,
                                             RegisteredEventType type);

  // Track the dispatched events that have not yet sent an ACK from the
  // renderer.
  void IncrementInFlightEvents(content::BrowserContext* context,
                               content::RenderProcessHost* process,
                               const Extension* extension,
                               int event_id,
                               const std::string& event_name,
                               base::TimeTicks dispatch_start_time,
                               int64_t service_worker_version_id,
                               EventDispatchSource dispatch_source,
                               bool lazy_background_active_on_dispatch,
                               events::HistogramValue histogram_value);
  void DecrementInFlightEventsForServiceWorker(
      const WorkerId& worker_id,
      int event_id,
      // Always false since this is only possibly true for lazy background page.
      bool event_will_run_in_lazy_background_page_script);
  void DecrementInFlightEventsForRenderFrameHost(
      int render_process_host,
      const ExtensionId& extension_id,
      int event_id,
      bool event_will_run_in_lazy_background_page_script);

  void RouteDispatchEvent(
      content::RenderProcessHost* rph,
      mojom::DispatchEventParamsPtr params,
      base::Value::List event_args,
      mojom::EventDispatcher::DispatchEventCallback callback);

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

  void UnbindServiceWorkerEventDispatcher(content::RenderProcessHost* host,
                                          int worker_thread_id);

  const raw_ptr<content::BrowserContext> browser_context_;

  // The ExtensionPrefs associated with |browser_context_|. May be NULL in
  // tests.
  const raw_ptr<ExtensionPrefs> extension_prefs_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  EventListenerMap listeners_{this};

  // Map from base event name to observer.
  using Observers = base::ObserverList<Observer>;
  using ObserverMap = base::flat_map<std::string, std::unique_ptr<Observers>>;
  ObserverMap observer_map_;

  base::ObserverList<TestObserver>::Unchecked test_observers_;

  std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>
      observed_process_set_;

  LazyEventDispatchUtil lazy_event_dispatch_util_;

  EventAckData event_ack_data_;

  using DispatcherMap =
      std::map<int /*worker_thread_id*/,
               mojo::AssociatedRemote<mojom::EventDispatcher>>;
  std::map<content::RenderProcessHost*, DispatcherMap> rph_dispatcher_map_;

  // All the Mojo receivers for the EventRouter. Keeps track of the render
  // process id.
  mojo::AssociatedReceiverSet<mojom::EventRouter, int /*render_process_id*/>
      receivers_;

  base::WeakPtrFactory<EventRouter> weak_factory_{this};
};

// Describes the process an |Event| was dispatched to.
struct EventTarget {
  ExtensionId extension_id;
  int render_process_id;
  int64_t service_worker_version_id;
  int worker_thread_id;
};

struct Event {
  // This callback should return true if the event should be dispatched to the
  // given context and extension, and false otherwise.
  using WillDispatchCallback = base::RepeatingCallback<bool(
      content::BrowserContext*,
      mojom::ContextType,
      const Extension*,
      const base::Value::Dict*,
      std::optional<base::Value::List>& event_args_out,
      mojom::EventFilteringInfoPtr& event_filtering_info_out)>;

  using DidDispatchCallback = base::RepeatingCallback<void(const EventTarget&)>;

  using CannotDispatchCallback = base::RepeatingCallback<void()>;

  // The identifier for the event, for histograms. In most cases this
  // correlates 1:1 with |event_name|, in some cases events will generate
  // their own names, but they cannot generate their own identifier.
  const events::HistogramValue histogram_value;

  // The event to dispatch.
  const std::string event_name;

  // Arguments to send to the event listener.
  base::Value::List event_args;

  // If non-null, then the event will not be sent to other BrowserContexts
  // unless the extension has permission (e.g. incognito tab update -> normal
  // tab only works if extension is allowed incognito access).
  const raw_ptr<content::BrowserContext> restrict_to_browser_context;

  // If present, then the event will only be sent to this context type.
  const std::optional<mojom::ContextType> restrict_to_context_type;

  // If not empty, the event is only sent to extensions with host permissions
  // for this url.
  GURL event_url;

  // When the event router received the event to be dispatched to the extension.
  // Used in UMA histograms.
  base::TimeTicks dispatch_start_time;

  // `true` if the event was dispatched to a active/running lazy background.
  // This is only used for lazy background contexts (event pages and service
  // workers), it is unused for persistent background pages. Used in UMA
  // histograms.
  bool lazy_background_active_on_dispatch;

  // Whether a user gesture triggered the event.
  EventRouter::UserGestureState user_gesture;

  // Extra information used to filter which events are sent to the listener.
  mojom::EventFilteringInfoPtr filter_info;

  // If specified, this is called before dispatching an event to each
  // extension. This is guaranteed to be called synchronously with
  // DispatchEvent, so callers don't need to worry about lifetime.
  // The args |event_args_out|, |event_filtering_info_out| allows caller to
  // provide modified `Event::event_args`, `Event::filter_info` depending on the
  // extension and profile.
  //
  // NOTE: the Extension argument to this may be NULL because it's possible for
  // this event to be dispatched to non-extension processes, like WebUI.
  WillDispatchCallback will_dispatch_callback;

  // If specified, this is called after dispatching an event to each target.
  DidDispatchCallback did_dispatch_callback;

  // Called if the event cannot be dispatched to a lazy listener. This happens
  // if e.g. the extension registers an event listener from a lazy context
  // asynchronously, which results in the active listener not being registered
  // at the time the lazy context is spun back up.
  CannotDispatchCallback cannot_dispatch_callback;

  // TODO(lazyboy): This sets |restrict_to_browser_context| to nullptr, this
  // will dispatch the event to unrelated profiles, not just incognito. Audit
  // and limit usages of this constructor and introduce "include incognito"
  // option to a constructor version for clients that need to disptach events to
  // related browser_contexts. See https://crbug.com/726022.
  Event(events::HistogramValue histogram_value,
        std::string_view event_name,
        base::Value::List event_args);

  Event(events::HistogramValue histogram_value,
        std::string_view event_name,
        base::Value::List event_args,
        content::BrowserContext* restrict_to_browser_context,
        std::optional<mojom::ContextType> restrict_to_context_type =
            std::nullopt);

  Event(events::HistogramValue histogram_value,
        std::string_view event_name,
        base::Value::List event_args,
        content::BrowserContext* restrict_to_browser_context,
        std::optional<mojom::ContextType> restrict_to_context_type,
        const GURL& event_url,
        EventRouter::UserGestureState user_gesture,
        mojom::EventFilteringInfoPtr info,
        bool lazy_background_active_on_dispatch = false,
        base::TimeTicks dispatch_start_time = base::TimeTicks{});

  ~Event();

  // Makes a deep copy of this instance.
  std::unique_ptr<Event> DeepCopy() const;
};

struct EventListenerInfo {
  // Constructor used by tests, for a listener from a non-ServiceWorker
  // context (background page, popup, tab, etc).
  EventListenerInfo(const std::string& event_name,
                    const ExtensionId& extension_id,
                    const GURL& listener_url,
                    content::BrowserContext* browser_context);

  EventListenerInfo(const std::string& event_name,
                    const ExtensionId& extension_id,
                    const GURL& listener_url,
                    content::BrowserContext* browser_context,
                    int worker_thread_id,
                    int64_t service_worker_version_id,
                    bool is_lazy);

  // The event name including any sub-event, e.g. "runtime.onStartup" or
  // "webRequest.onCompleted/123".
  const std::string event_name;
  const ExtensionId extension_id;
  const GURL listener_url;
  const raw_ptr<content::BrowserContext> browser_context;
  const int worker_thread_id;
  const int64_t service_worker_version_id;
  const bool is_lazy;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENT_ROUTER_H_
