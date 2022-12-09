// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <stddef.h>

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/lazy_event_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

namespace {

// A dictionary of event names to lists of filters that this extension has
// registered from its lazy background page.
constexpr char kFilteredEvents[] = "filtered_events";

// Similar to |kFilteredEvents|, but applies to extension service worker events.
constexpr char kFilteredServiceWorkerEvents[] =
    "filtered_service_worker_events";

// A message when mojom::EventRouter::AddListenerForMainThread() is called with
// an invalid param.
constexpr char kAddEventListenerWithInvalidParam[] =
    "Tried to add an event listener without a valid extension ID nor listener "
    "URL";

// A message when mojom::EventRouter::AddListenerForServiceWorker() is called
// with an invalid worker scope URL.
constexpr char kAddEventListenerWithInvalidWorkerScopeURL[] =
    "Tried to add an event listener for a service worker without a valid "
    "worker scope URL.";

// A message when mojom::EventRouter::AddListenerForServiceWorker() is called
// with an invalid extension ID.
constexpr char kAddEventListenerWithInvalidExtensionID[] =
    "Tried to add an event listener for a service worker without a valid "
    "extension ID.";

// A message when mojom::EventRouter::RemoveListenerForMainThread() is called
// with an invalid param.
constexpr char kRemoveEventListenerWithInvalidParam[] =
    "Tried to remove an event listener without a valid extension ID nor "
    "listener URL";

// A message when mojom::EventRouter::RemoveListenerForServiceWorker() is called
// with an invalid worker scope URL.
constexpr char kRemoveEventListenerWithInvalidWorkerScopeURL[] =
    "Tried to remove an event listener for a service worker without a valid "
    "worker scope URL.";

// A message when mojom::EventRouter::RemoveListenerForServiceWorker() is called
// with an invalid extension ID.
constexpr char kRemoveEventListenerWithInvalidExtensionID[] =
    "Tried to remove an event listener for a service worker without a valid "
    "extension ID.";

// Sends a notification about an event to the API activity monitor and the
// ExtensionHost for |extension_id| on the UI thread. Can be called from any
// thread.
void NotifyEventDispatched(content::BrowserContext* browser_context,
                           const std::string& extension_id,
                           const std::string& event_name,
                           const base::Value::List& args) {
  // Notify the ApiActivityMonitor about the event dispatch.
  activity_monitor::OnApiEventDispatched(browser_context, extension_id,
                                         event_name, args);
}

// Browser context is required for lazy context id. Before adding browser
// context member to EventListener, callers must pass in the browser context as
// a parameter.
// TODO(richardzh): Once browser context is added as a member to EventListener,
//                  update this method to get browser_context from listener
//                  instead of parameter.
LazyContextId LazyContextIdForListener(const EventListener* listener,
                                       BrowserContext* browser_context) {
  auto* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);

  const Extension* extension =
      registry->enabled_extensions().GetByID(listener->extension_id());
  const bool is_service_worker_based_extension =
      extension && BackgroundInfo::IsServiceWorkerBased(extension);
  // Note: It is possible that the prefs' listener->is_for_service_worker() and
  // its extension background type do not agree. This happens when one changes
  // extension's manifest, typically during unpacked extension development.
  // Fallback to non-Service worker based LazyContextId to avoid surprising
  // ServiceWorkerTaskQueue (and crashing), see https://crbug.com/1239752 for
  // details.
  // TODO(lazyboy): Clean these inconsistencies across different types of event
  // listener and their corresponding background types.
  if (is_service_worker_based_extension && listener->is_for_service_worker()) {
    return LazyContextId(browser_context, listener->extension_id(),
                         listener->listener_url());
  }

  return LazyContextId(browser_context, listener->extension_id());
}

// A global identifier used to distinguish extension events.
base::AtomicSequenceNumber g_extension_event_id;

}  // namespace

const char EventRouter::kRegisteredLazyEvents[] = "events";
const char EventRouter::kRegisteredServiceWorkerEvents[] =
    "serviceworkerevents";

// static
void EventRouter::DispatchExtensionMessage(
    content::RenderProcessHost* rph,
    int worker_thread_id,
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    int event_id,
    const std::string& event_name,
    base::Value::List event_args,
    UserGestureState user_gesture,
    mojom::EventFilteringInfoPtr info) {
  NotifyEventDispatched(browser_context, extension_id, event_name, event_args);
  auto params = mojom::DispatchEventParams::New();
  params->worker_thread_id = worker_thread_id;
  params->extension_id = extension_id;
  params->event_name = event_name;
  params->event_id = event_id;
  params->is_user_gesture = user_gesture == USER_GESTURE_ENABLED;
  params->filtering_info = std::move(info);

  Get(browser_context)
      ->RouteDispatchEvent(rph, std::move(params), std::move(event_args));
}

void EventRouter::RouteDispatchEvent(content::RenderProcessHost* rph,
                                     mojom::DispatchEventParamsPtr params,
                                     base::Value::List event_args) {
  // TODO(crbug.com/1302000) Add bindings for worker threads to be directly
  // channel-associated.
  mojo::AssociatedRemote<mojom::EventDispatcher>& dispatcher =
      rph_dispatcher_map_[rph];
  if (!dispatcher.is_bound()) {
    IPC::ChannelProxy* channel = rph->GetChannel();
    if (!channel) {
      return;
    }
    channel->GetRemoteAssociatedInterface(
        dispatcher.BindNewEndpointAndPassReceiver());
  }
  dispatcher->DispatchEvent(std::move(params), std::move(event_args));
}

// static
EventRouter* EventRouter::Get(content::BrowserContext* browser_context) {
  return EventRouterFactory::GetForBrowserContext(browser_context);
}

// static
std::string EventRouter::GetBaseEventName(const std::string& full_event_name) {
  size_t slash_sep = full_event_name.find('/');
  return full_event_name.substr(0, slash_sep);
}

// static
void EventRouter::DispatchEventToSender(
    content::RenderProcessHost* rph,
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    int render_process_id,
    int worker_thread_id,
    int64_t service_worker_version_id,
    base::Value::List event_args,
    mojom::EventFilteringInfoPtr info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int event_id = g_extension_event_id.GetNext();

  DoDispatchEventToSenderBookkeeping(
      browser_context, extension_id, event_id, render_process_id,
      service_worker_version_id, histogram_value, event_name);

  DispatchExtensionMessage(rph, worker_thread_id, browser_context, extension_id,
                           event_id, event_name, std::move(event_args),
                           UserGestureState::USER_GESTURE_UNKNOWN,
                           std::move(info));
}

// static.
bool EventRouter::CanDispatchEventToBrowserContext(BrowserContext* context,
                                                   const Extension* extension,
                                                   const Event& event) {
  // Is this event from a different browser context than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool crosses_incognito = event.restrict_to_browser_context &&
                           context != event.restrict_to_browser_context;
  if (!crosses_incognito)
    return true;
  return ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(extension,
                                                                    context);
}

// static
void EventRouter::BindForRenderer(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::EventRouter> receiver) {
  auto* host = RenderProcessHost::FromID(render_process_id);
  // EventRouter might be null for some irregular profile, e.g. the System
  // Profile.
  EventRouter* event_router = EventRouter::Get(host->GetBrowserContext());
  if (!host || !event_router)
    return;

  event_router->receivers_.Add(event_router, std::move(receiver),
                               render_process_id);
}

EventRouter::EventRouter(BrowserContext* browser_context,
                         ExtensionPrefs* extension_prefs)
    : browser_context_(browser_context),
      extension_prefs_(extension_prefs),
      lazy_event_dispatch_util_(browser_context_) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
}

EventRouter::~EventRouter() {
  for (auto* process : observed_process_set_)
    process->RemoveObserver(this);
}

BrowserContext* EventRouter::GetIncognitoContextIfAccessible(
    const std::string& extension_id) {
  DCHECK(!extension_id.empty());
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return nullptr;
  if (!IncognitoInfo::IsSplitMode(extension))
    return nullptr;

  return GetIncognitoContext();
}

BrowserContext* EventRouter::GetIncognitoContext() {
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (!browser_client->HasOffTheRecordContext(browser_context_))
    return nullptr;

  return browser_client->GetOffTheRecordContext(browser_context_);
}

void EventRouter::AddListenerForMainThread(mojom::EventListenerParamPtr param,
                                           const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  if (param->is_extension_id() &&
      crx_file::id_util::IdIsValid(param->get_extension_id())) {
    AddEventListener(event_name, process, param->get_extension_id());
  } else if (param->is_listener_url() && param->get_listener_url().is_valid()) {
    AddEventListenerForURL(event_name, process, param->get_listener_url());
  } else {
    mojo::ReportBadMessage(kAddEventListenerWithInvalidParam);
  }
}

void EventRouter::AddListenerForServiceWorker(const std::string& extension_id,
                                              const GURL& worker_scope_url,
                                              const std::string& event_name,
                                              int64_t service_worker_version_id,
                                              int32_t worker_thread_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    if (!worker_scope_url.is_valid()) {
      mojo::ReportBadMessage(kAddEventListenerWithInvalidWorkerScopeURL);
      return;
    }
    AddServiceWorkerEventListener(event_name, process, extension_id,
                                  worker_scope_url, service_worker_version_id,
                                  worker_thread_id);
  } else {
    mojo::ReportBadMessage(kAddEventListenerWithInvalidExtensionID);
  }
}

void EventRouter::AddLazyListenerForMainThread(const std::string& extension_id,
                                               const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddLazyEventListener(event_name, extension_id);
}

void EventRouter::AddLazyListenerForServiceWorker(
    const std::string& extension_id,
    const GURL& worker_scope_url,
    const std::string& event_name) {
  // TODO(richardzh): Passing in browser context from the process.
  // Browser context is added to listener object in order to separate lazy
  // listeners for regular and incognito(split) context. The first step adds
  // browser context member to EventListener object. The next step is to
  // assign correct browser context and use it to create both lazy
  // listeners.
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(
          event_name, extension_id, nullptr, browser_context_, worker_scope_url,
          // Lazy listener, without worker version id and thread id.
          blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
          absl::nullopt);
  AddLazyEventListenerImpl(std::move(listener),
                           RegisteredEventType::kServiceWorker);
}

void EventRouter::AddFilteredListenerForMainThread(
    mojom::EventListenerParamPtr param,
    const std::string& event_name,
    base::Value::Dict filter,
    bool add_lazy_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  AddFilteredEventListener(event_name, process, std::move(param), absl::nullopt,
                           std::move(filter), add_lazy_listener);
}

void EventRouter::AddFilteredListenerForServiceWorker(
    const std::string& extension_id,
    const GURL& worker_scope_url,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int32_t worker_thread_id,
    base::Value::Dict filter,
    bool add_lazy_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  ServiceWorkerIdentifier sw_identifier;
  sw_identifier.scope = worker_scope_url;
  sw_identifier.thread_id = worker_thread_id;
  sw_identifier.version_id = service_worker_version_id;

  AddFilteredEventListener(
      event_name, process,
      mojom::EventListenerParam::NewExtensionId(extension_id), sw_identifier,
      std::move(filter), add_lazy_listener);
}

void EventRouter::RemoveListenerForMainThread(
    mojom::EventListenerParamPtr param,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  if (param->is_extension_id() &&
      crx_file::id_util::IdIsValid(param->get_extension_id())) {
    RemoveEventListener(event_name, process, param->get_extension_id());
  } else if (param->is_listener_url() && param->get_listener_url().is_valid()) {
    RemoveEventListenerForURL(event_name, process, param->get_listener_url());
  } else {
    mojo::ReportBadMessage(kRemoveEventListenerWithInvalidParam);
  }
}

void EventRouter::RemoveListenerForServiceWorker(
    const std::string& extension_id,
    const GURL& worker_scope_url,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    if (!worker_scope_url.is_valid()) {
      mojo::ReportBadMessage(kRemoveEventListenerWithInvalidWorkerScopeURL);
      return;
    }
    RemoveServiceWorkerEventListener(
        event_name, process, extension_id, worker_scope_url,
        service_worker_version_id, worker_thread_id);
  } else {
    mojo::ReportBadMessage(kRemoveEventListenerWithInvalidExtensionID);
  }
}

void EventRouter::RemoveLazyListenerForMainThread(
    const std::string& extension_id,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RemoveLazyEventListener(event_name, extension_id);
}

void EventRouter::RemoveLazyListenerForServiceWorker(
    const std::string& extension_id,
    const GURL& worker_scope_url,
    const std::string& event_name) {
  // TODO(richardzh): Passing in browser context from the process.
  // Browser context is added to listener object in order to separate lazy
  // listeners for regular and incognito(split) context. The first step adds
  // browser context member to EventListener object. The next step is to
  // assign correct browser context and use it to create both lazy
  // listeners.
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(
          event_name, extension_id, nullptr, browser_context_, worker_scope_url,
          // Lazy listener, without worker version id and thread id.
          blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
          absl::nullopt);
  RemoveLazyEventListenerImpl(std::move(listener),
                              RegisteredEventType::kServiceWorker);
}

void EventRouter::RemoveFilteredListenerForMainThread(
    mojom::EventListenerParamPtr param,
    const std::string& event_name,
    base::Value::Dict filter,
    bool remove_lazy_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  RemoveFilteredEventListener(event_name, process, std::move(param),
                              absl::nullopt, std::move(filter),
                              remove_lazy_listener);
}

void EventRouter::RemoveFilteredListenerForServiceWorker(
    const std::string& extension_id,
    const GURL& worker_scope_url,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int32_t worker_thread_id,
    base::Value::Dict filter,
    bool remove_lazy_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(receivers_.current_context());
  if (!process)
    return;

  ServiceWorkerIdentifier sw_identifier;
  sw_identifier.scope = worker_scope_url;
  sw_identifier.thread_id = worker_thread_id;
  sw_identifier.version_id = service_worker_version_id;

  RemoveFilteredEventListener(
      event_name, process,
      mojom::EventListenerParam::NewExtensionId(extension_id), sw_identifier,
      std::move(filter), remove_lazy_listener);
}

void EventRouter::AddEventListener(const std::string& event_name,
                                   RenderProcessHost* process,
                                   const std::string& extension_id) {
  listeners_.AddListener(EventListener::ForExtension(event_name, extension_id,
                                                     process, absl::nullopt));
}

void EventRouter::AddServiceWorkerEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  listeners_.AddListener(EventListener::ForExtensionServiceWorker(
      event_name, extension_id, process, process->GetBrowserContext(),
      service_worker_scope, service_worker_version_id, worker_thread_id,
      absl::nullopt));
}

void EventRouter::RemoveEventListener(const std::string& event_name,
                                      RenderProcessHost* process,
                                      const std::string& extension_id) {
  std::unique_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, process, absl::nullopt);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RemoveServiceWorkerEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(
          event_name, extension_id, process, process->GetBrowserContext(),
          service_worker_scope, service_worker_version_id, worker_thread_id,
          absl::nullopt);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::AddEventListenerForURL(const std::string& event_name,
                                         RenderProcessHost* process,
                                         const GURL& listener_url) {
  listeners_.AddListener(
      EventListener::ForURL(event_name, listener_url, process, absl::nullopt));
}

void EventRouter::RemoveEventListenerForURL(const std::string& event_name,
                                            RenderProcessHost* process,
                                            const GURL& listener_url) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForURL(event_name, listener_url, process, absl::nullopt);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RegisterObserver(Observer* observer,
                                   const std::string& event_name) {
  // Observing sub-event names like "foo.onBar/123" is not allowed.
  DCHECK(event_name.find('/') == std::string::npos);
  auto& observers = observer_map_[event_name];
  if (!observers) {
    observers = std::make_unique<Observers>();
  }

  observers->AddObserver(observer);
}

void EventRouter::UnregisterObserver(Observer* observer) {
  for (auto& it : observer_map_) {
    it.second->RemoveObserver(observer);
  }
}

void EventRouter::AddObserverForTesting(TestObserver* observer) {
  test_observers_.AddObserver(observer);
}

void EventRouter::RemoveObserverForTesting(TestObserver* observer) {
  test_observers_.RemoveObserver(observer);
}

void EventRouter::OnListenerAdded(const EventListener* listener) {
  const EventListenerInfo details(
      listener->event_name(), listener->extension_id(),
      listener->listener_url(), listener->browser_context(),
      listener->worker_thread_id(), listener->service_worker_version_id(),
      listener->IsLazy());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  auto it = observer_map_.find(base_event_name);
  if (it != observer_map_.end()) {
    for (auto& observer : *it->second) {
      observer.OnListenerAdded(details);
    }
  }

  RenderProcessHost* process = listener->process();
  if (process) {
    bool inserted = observed_process_set_.insert(process).second;
    if (inserted)
      process->AddObserver(this);
  }
}

void EventRouter::OnListenerRemoved(const EventListener* listener) {
  const EventListenerInfo details(
      listener->event_name(), listener->extension_id(),
      listener->listener_url(), listener->browser_context(),
      listener->worker_thread_id(), listener->service_worker_version_id(),
      listener->IsLazy());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  auto it = observer_map_.find(base_event_name);
  if (it != observer_map_.end()) {
    for (auto& observer : *it->second) {
      observer.OnListenerRemoved(details);
    }
  }
}

void EventRouter::RenderProcessExited(
    RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  rph_dispatcher_map_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::RenderProcessHostDestroyed(RenderProcessHost* host) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::AddLazyEventListener(const std::string& event_name,
                                       const ExtensionId& extension_id) {
  AddLazyEventListenerImpl(EventListener::ForExtension(event_name, extension_id,
                                                       nullptr, absl::nullopt),
                           RegisteredEventType::kLazy);
}

void EventRouter::RemoveLazyEventListener(const std::string& event_name,
                                          const ExtensionId& extension_id) {
  RemoveLazyEventListenerImpl(
      EventListener::ForExtension(event_name, extension_id, nullptr,
                                  absl::nullopt),
      RegisteredEventType::kLazy);
}

void EventRouter::AddFilteredEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    mojom::EventListenerParamPtr param,
    absl::optional<ServiceWorkerIdentifier> sw_identifier,
    const base::Value::Dict& filter,
    bool add_lazy_listener) {
  const bool is_for_service_worker = sw_identifier.has_value();
  std::unique_ptr<EventListener> regular_listener;
  std::unique_ptr<EventListener> lazy_listener;
  if (is_for_service_worker && param->is_extension_id()) {
    regular_listener = EventListener::ForExtensionServiceWorker(
        event_name, param->get_extension_id(), process,
        process->GetBrowserContext(), sw_identifier->scope,
        sw_identifier->version_id, sw_identifier->thread_id, filter.Clone());
    if (add_lazy_listener) {
      // TODO(richardzh): take browser context from the process instead of the
      // regular browser context attached to the event router. Browser context
      // is introduced to listener in order to separate lazy listeners for
      // regular and incognito(split) context. The first step is adding the
      // browser context as a member of EventListener object. The next step is
      // to assign correct browser context and use it to create both lazy
      // listeners.
      lazy_listener = EventListener::ForExtensionServiceWorker(
          event_name, param->get_extension_id(), nullptr, browser_context_,
          sw_identifier->scope,
          // Lazy listener, without worker version id and thread id.
          blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
          filter.Clone());
    }
  } else if (param->is_extension_id()) {
    regular_listener = EventListener::ForExtension(
        event_name, param->get_extension_id(), process, filter.Clone());
    if (add_lazy_listener) {
      lazy_listener =
          EventListener::ForExtension(event_name, param->get_extension_id(),
                                      nullptr,  // Lazy, without process.
                                      filter.Clone());
    }
  } else if (param->is_listener_url() && !add_lazy_listener) {
    regular_listener = EventListener::ForURL(
        event_name, param->get_listener_url(), process, filter.Clone());
  } else {
    mojo::ReportBadMessage(kAddEventListenerWithInvalidParam);
    return;
  }
  listeners_.AddListener(std::move(regular_listener));

  DCHECK_EQ(add_lazy_listener, !!lazy_listener);
  if (lazy_listener) {
    bool added = listeners_.AddListener(std::move(lazy_listener));
    if (added) {
      AddFilterToEvent(event_name, param->get_extension_id(),
                       is_for_service_worker, filter);
    }
  }
}

void EventRouter::RemoveFilteredEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    mojom::EventListenerParamPtr param,
    absl::optional<ServiceWorkerIdentifier> sw_identifier,
    const base::Value::Dict& filter,
    bool remove_lazy_listener) {
  const bool is_for_service_worker = sw_identifier.has_value();
  std::unique_ptr<EventListener> listener;
  if (is_for_service_worker && param->is_extension_id()) {
    listener = EventListener::ForExtensionServiceWorker(
        event_name, param->get_extension_id(), process,
        process->GetBrowserContext(), sw_identifier->scope,
        sw_identifier->version_id, sw_identifier->thread_id, filter.Clone());
  } else if (param->is_extension_id()) {
    listener = EventListener::ForExtension(
        event_name, param->get_extension_id(), process, filter.Clone());

  } else if (param->is_listener_url() && !remove_lazy_listener) {
    listener = EventListener::ForURL(event_name, param->get_listener_url(),
                                     process, filter.Clone());
  } else {
    mojo::ReportBadMessage(kRemoveEventListenerWithInvalidParam);
    return;
  }

  listeners_.RemoveListener(listener.get());

  if (remove_lazy_listener) {
    listener->MakeLazy();
    bool removed = listeners_.RemoveListener(listener.get());

    if (removed) {
      RemoveFilterFromEvent(event_name, param->get_extension_id(),
                            is_for_service_worker, filter);
    }
  }
}

bool EventRouter::HasEventListener(const std::string& event_name) const {
  return listeners_.HasListenerForEvent(event_name);
}

bool EventRouter::ExtensionHasEventListener(
    const std::string& extension_id,
    const std::string& event_name) const {
  return listeners_.HasListenerForExtension(extension_id, event_name);
}

bool EventRouter::URLHasEventListener(const GURL& url,
                                      const std::string& event_name) const {
  return listeners_.HasListenerForURL(url, event_name);
}

std::set<std::string> EventRouter::GetRegisteredEvents(
    const std::string& extension_id,
    RegisteredEventType type) const {
  std::set<std::string> events;
  if (!extension_prefs_)
    return events;

  const char* pref_key = type == RegisteredEventType::kLazy
                             ? kRegisteredLazyEvents
                             : kRegisteredServiceWorkerEvents;
  const base::Value::List* events_value =
      extension_prefs_->ReadPrefAsList(extension_id, pref_key);
  if (!events_value)
    return events;

  for (const base::Value& event_val : *events_value) {
    const std::string* event = event_val.GetIfString();
    if (event)
      events.insert(*event);
  }
  return events;
}

void EventRouter::ClearRegisteredEventsForTest(
    const ExtensionId& extension_id) {
  SetRegisteredEvents(extension_id, std::set<std::string>(),
                      RegisteredEventType::kLazy);
  SetRegisteredEvents(extension_id, std::set<std::string>(),
                      RegisteredEventType::kServiceWorker);
}

bool EventRouter::HasLazyEventListenerForTesting(
    const std::string& event_name) {
  const EventListenerMap::ListenerList& listeners =
      listeners_.GetEventListenersByName(event_name);
  return base::ranges::any_of(
      listeners, [](const std::unique_ptr<EventListener>& listener) {
        return listener->IsLazy();
      });
}

bool EventRouter::HasNonLazyEventListenerForTesting(
    const std::string& event_name) {
  const EventListenerMap::ListenerList& listeners =
      listeners_.GetEventListenersByName(event_name);
  return base::ranges::any_of(
      listeners, [](const std::unique_ptr<EventListener>& listener) {
        return !listener->IsLazy();
      });
}

void EventRouter::RemoveFilterFromEvent(const std::string& event_name,
                                        const std::string& extension_id,
                                        bool is_for_service_worker,
                                        const base::Value::Dict& filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id,
      is_for_service_worker ? kFilteredServiceWorkerEvents : kFilteredEvents);
  auto filtered_events = update.Create();
  base::Value::List* filter_list = nullptr;
  if (!filtered_events ||
      !filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    return;
  }
  filter_list->erase(base::ranges::find(*filter_list, filter));
}

const base::Value::Dict* EventRouter::GetFilteredEvents(
    const std::string& extension_id,
    RegisteredEventType type) {
  const char* pref_key = type == RegisteredEventType::kLazy
                             ? kFilteredEvents
                             : kFilteredServiceWorkerEvents;
  return extension_prefs_->ReadPrefAsDict(extension_id, pref_key);
}

void EventRouter::BroadcastEvent(std::unique_ptr<Event> event) {
  DispatchEventImpl(std::string(), GURL(), std::move(event));
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  DispatchEventImpl(extension_id, GURL(), std::move(event));
}

void EventRouter::DispatchEventToURL(const GURL& url,
                                     std::unique_ptr<Event> event) {
  DCHECK(!url.is_empty());
  DispatchEventImpl(std::string(), url, std::move(event));
}

void EventRouter::DispatchEventWithLazyListener(const std::string& extension_id,
                                                std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;
  const bool is_service_worker_based_background =
      BackgroundInfo::IsServiceWorkerBased(extension);

  std::string event_name = event->event_name;
  const bool has_listener = ExtensionHasEventListener(extension_id, event_name);
  if (!has_listener) {
    if (is_service_worker_based_background) {
      AddLazyListenerForServiceWorker(
          extension_id, Extension::GetBaseURLFromExtensionId(extension_id),
          event_name);
    } else {
      AddLazyEventListener(event_name, extension_id);
    }
  }

  DispatchEventToExtension(extension_id, std::move(event));

  if (!has_listener) {
    if (is_service_worker_based_background) {
      RemoveLazyListenerForServiceWorker(
          extension_id, Extension::GetBaseURLFromExtensionId(extension_id),
          event_name);
    } else {
      RemoveLazyEventListener(event_name, extension_id);
    }
  }
}

void EventRouter::DispatchEventImpl(const std::string& restrict_to_extension_id,
                                    const GURL& restrict_to_url,
                                    std::unique_ptr<Event> event) {
  DCHECK(event);
  // We don't expect to get events from a completely different browser context.
  DCHECK(!event->restrict_to_browser_context ||
         ExtensionsBrowserClient::Get()->IsSameContext(
             browser_context_, event->restrict_to_browser_context));

  // Don't dispatch events to observers if the browser is shutting down.
  if (browser_context_->ShutdownStarted())
    return;

  for (TestObserver& observer : test_observers_)
    observer.OnWillDispatchEvent(*event);

  std::set<const EventListener*> listeners(
      listeners_.GetEventListeners(*event));

  LazyEventDispatcher lazy_event_dispatcher(
      browser_context_, base::BindRepeating(&EventRouter::DispatchPendingEvent,
                                            weak_factory_.GetWeakPtr()));

  // We dispatch events for lazy background pages first because attempting to do
  // so will cause those that are being suspended to cancel that suspension.
  // As canceling a suspension entails sending an event to the affected
  // background page, and as that event needs to be delivered before we dispatch
  // the event we are dispatching here, we dispatch to the lazy listeners here
  // first.
  for (const EventListener* listener : listeners) {
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (!restrict_to_url.is_empty() &&
        !url::IsSameOriginWith(restrict_to_url, listener->listener_url())) {
      continue;
    }
    if (!listener->IsLazy())
      continue;

    // TODO(richardzh): Move cross browser context check (by calling
    // EventRouter::CanDispatchEventToBrowserContext) from
    // LazyEventDispatcher to here. So the check happens before instead of
    // during the dispatch.

    // Lazy listeners don't have a process, take the stored browser context
    // for lazy context.
    lazy_event_dispatcher.Dispatch(
        *event, LazyContextIdForListener(listener, browser_context_),
        listener->filter());

    // Dispatch to lazy listener in the incognito context.
    // We need to use the incognito context in the case of split-mode
    // extensions.
    BrowserContext* incognito_context =
        GetIncognitoContextIfAccessible(listener->extension_id());
    if (incognito_context) {
      lazy_event_dispatcher.Dispatch(
          *event, LazyContextIdForListener(listener, incognito_context),
          listener->filter());
    }
  }

  for (const EventListener* listener : listeners) {
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (!restrict_to_url.is_empty() &&
        !url::IsSameOriginWith(restrict_to_url, listener->listener_url())) {
      continue;
    }
    if (listener->IsLazy())
      continue;
    // Non-lazy listeners take the process browser context for
    // lazy context
    if (lazy_event_dispatcher.HasAlreadyDispatched(LazyContextIdForListener(
            listener, listener->process()->GetBrowserContext()))) {
      continue;
    }

    DispatchEventToProcess(
        listener->extension_id(), listener->listener_url(), listener->process(),
        listener->service_worker_version_id(), listener->worker_thread_id(),
        *event, listener->filter(), false /* did_enqueue */);
  }
}

void EventRouter::DispatchEventToProcess(
    const std::string& extension_id,
    const GURL& listener_url,
    RenderProcessHost* process,
    int64_t service_worker_version_id,
    int worker_thread_id,
    const Event& event,
    const base::Value::Dict* listener_filter,
    bool did_enqueue) {
  BrowserContext* listener_context = process->GetBrowserContext();
  ProcessMap* process_map = ProcessMap::Get(listener_context);

  // NOTE: |extension| being NULL does not necessarily imply that this event
  // shouldn't be dispatched. Events can be dispatched to WebUI and webviews as
  // well.  It all depends on what GetMostLikelyContextType returns.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);

  if (!extension && !extension_id.empty()) {
    // Trying to dispatch an event to an extension that doesn't exist. The
    // extension could have been removed, but we do not unregister it until the
    // extension process is unloaded.
    return;
  }

  if (extension) {
    // Extension-specific checks.
    // Firstly, if the event is for a URL, the Extension must have permission
    // to access that URL.
    if (!event.event_url.is_empty() &&
        event.event_url.host() != extension->id() &&  // event for self is ok
        !extension->permissions_data()
             ->active_permissions()
             .HasEffectiveAccessToURL(event.event_url)) {
      return;
    }
    // Secondly, if the event is for incognito mode, the Extension must be
    // enabled in incognito mode.
    if (!CanDispatchEventToBrowserContext(listener_context, extension, event)) {
      return;
    }
  }

  // TODO(ortuno): |listener_url| is passed in from the renderer so it can't
  // fully be trusted. We should retrieve the URL from the browser process.
  const GURL* url =
      service_worker_version_id == blink::mojom::kInvalidServiceWorkerVersionId
          ? &listener_url
          : nullptr;
  Feature::Context target_context =
      process_map->GetMostLikelyContextType(extension, process->GetID(), url);

  // We shouldn't be dispatching an event to a webpage, since all such events
  // (e.g.  messaging) don't go through EventRouter.
  DCHECK_NE(Feature::WEB_PAGE_CONTEXT, target_context)
      << "Trying to dispatch event " << event.event_name << " to a webpage,"
      << " but this shouldn't be possible";

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event.event_name, extension, target_context, listener_url,
          CheckAliasStatus::ALLOWED,
          util::GetBrowserContextId(browser_context_));
  if (!availability.is_available()) {
    // It shouldn't be possible to reach here, because access is checked on
    // registration. However, for paranoia, check on dispatch as well.
    NOTREACHED() << "Trying to dispatch event " << event.event_name
                 << " which the target does not have access to: "
                 << availability.message();
    return;
  }

  absl::optional<base::Value::List> modified_event_args;
  mojom::EventFilteringInfoPtr modified_event_filter_info;
  if (!event.will_dispatch_callback.is_null() &&
      !event.will_dispatch_callback.Run(
          listener_context, target_context, extension, listener_filter,
          modified_event_args, modified_event_filter_info)) {
    return;
  }

  base::Value::List event_args_to_use = modified_event_args
                                            ? std::move(*modified_event_args)
                                            : event.event_args.Clone();

  mojom::EventFilteringInfoPtr filter_info =
      modified_event_filter_info ? std::move(modified_event_filter_info)
                                 : event.filter_info.Clone();

  int event_id = g_extension_event_id.GetNext();
  DispatchExtensionMessage(process, worker_thread_id, listener_context,
                           extension_id, event_id, event.event_name,
                           std::move(event_args_to_use), event.user_gesture,
                           std::move(filter_info));

  if (!event.did_dispatch_callback.is_null()) {
    event.did_dispatch_callback.Run(EventTarget{extension_id, process->GetID(),
                                                service_worker_version_id,
                                                worker_thread_id});
  }

  for (TestObserver& observer : test_observers_)
    observer.OnDidDispatchEventToProcess(event);

  // TODO(lazyboy): This is wrong for extensions SW events. We need to:
  // 1. Increment worker ref count
  // 2. Add EventAck IPC to decrement that ref count.
  if (extension) {
    ReportEvent(event.histogram_value, extension, did_enqueue);

    IncrementInFlightEvents(listener_context, process, extension, event_id,
                            event.event_name, service_worker_version_id);
  }
}

// static
void EventRouter::DoDispatchEventToSenderBookkeeping(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    int event_id,
    int render_process_id,
    int64_t service_worker_version_id,
    events::HistogramValue histogram_value,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  auto* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  EventRouter* event_router = EventRouter::Get(browser_context);
  DCHECK(event_router);
  event_router->IncrementInFlightEvents(
      browser_context, RenderProcessHost::FromID(render_process_id), extension,
      event_id, event_name, service_worker_version_id);
  event_router->ReportEvent(histogram_value, extension, /*did_enqueue=*/false);
}

void EventRouter::IncrementInFlightEvents(BrowserContext* context,
                                          RenderProcessHost* process,
                                          const Extension* extension,
                                          int event_id,
                                          const std::string& event_name,
                                          int64_t service_worker_version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only increment in-flight events if the lazy background page is active,
  // because that's the only time we'll get an ACK.
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager* pm = ProcessManager::Get(context);
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension->id());
    if (host) {
      pm->IncrementLazyKeepaliveCount(extension, Activity::EVENT, event_name);
      host->OnBackgroundEventDispatched(event_name, event_id);
    }
  } else if (service_worker_version_id !=
             blink::mojom::kInvalidServiceWorkerVersionId) {
    // Check to make sure the rendered process hasn't gone away by the time
    // we've gotten here. (It's possible it has crashed, etc.) If that's
    // happened, we don't want to track the expected ACK, since we'll never
    // get it.
    if (process) {
      content::ServiceWorkerContext* service_worker_context =
          process->GetStoragePartition()->GetServiceWorkerContext();
      event_ack_data_.IncrementInflightEvent(
          service_worker_context, process->GetID(), service_worker_version_id,
          event_id);
    }
  }
}

void EventRouter::OnEventAck(BrowserContext* context,
                             const std::string& extension_id,
                             const std::string& event_name) {
  ProcessManager* pm = ProcessManager::Get(context);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  // TODO(mpcomplete): We should never get this message unless
  // HasLazyBackgroundPage is true. Find out why we're getting it anyway.
  if (host->extension() &&
      BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    pm->DecrementLazyKeepaliveCount(host->extension(), Activity::EVENT,
                                    event_name);
}

bool EventRouter::HasRegisteredEvents(const ExtensionId& extension_id) const {
  return !GetRegisteredEvents(extension_id, RegisteredEventType::kLazy)
              .empty() ||
         !GetRegisteredEvents(extension_id, RegisteredEventType::kServiceWorker)
              .empty();
}

void EventRouter::ReportEvent(events::HistogramValue histogram_value,
                              const Extension* extension,
                              bool did_enqueue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record every event fired.
  UMA_HISTOGRAM_ENUMERATION("Extensions.Events.Dispatch", histogram_value,
                            events::ENUM_BOUNDARY);

  bool is_component = Manifest::IsComponentLocation(extension->location());

  // Record events for component extensions. These should be kept to a minimum,
  // especially if they wake its event page. Component extensions should use
  // declarative APIs as much as possible.
  if (is_component) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.Events.DispatchToComponent",
                              histogram_value, events::ENUM_BOUNDARY);
  }

  // Record events for background pages, if any. The most important statistic
  // is DispatchWithSuspendedEventPage. Events reported there woke an event
  // page. Implementing either filtered or declarative versions of these events
  // should be prioritised.
  //
  // Note: all we know is that the extension *has* a persistent or event page,
  // not that the event is being dispatched *to* such a page. However, this is
  // academic, since extensions with any background page have that background
  // page running (or in the case of suspended event pages, must be started)
  // regardless of where the event is being dispatched. Events are dispatched
  // to a *process* not a *frame*.
  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.Events.DispatchWithPersistentBackgroundPage",
        histogram_value, events::ENUM_BOUNDARY);
  } else if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    if (did_enqueue) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithSuspendedEventPage", histogram_value,
          events::ENUM_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithRunningEventPage", histogram_value,
          events::ENUM_BOUNDARY);
    }
  } else if (BackgroundInfo::IsServiceWorkerBased(extension)) {
    base::UmaHistogramEnumeration(
        "Extensions.Events.DispatchWithServiceWorkerBackground",
        histogram_value, events::ENUM_BOUNDARY);
  }
}

void EventRouter::DispatchPendingEvent(
    std::unique_ptr<Event> event,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> params) {
  if (!params)
    return;
  DCHECK(event);
  if (listeners_.HasProcessListener(params->render_process_host,
                                    params->worker_thread_id,
                                    params->extension_id)) {
    DispatchEventToProcess(
        params->extension_id, params->url, params->render_process_host,
        params->service_worker_version_id, params->worker_thread_id, *event,
        nullptr, true /* did_enqueue */);
  }
}

void EventRouter::SetRegisteredEvents(const std::string& extension_id,
                                      const std::set<std::string>& events,
                                      RegisteredEventType type) {
  base::Value events_value(base::Value::Type::LIST);
  for (auto iter = events.cbegin(); iter != events.cend(); ++iter) {
    events_value.Append(*iter);
  }
  const char* pref_key = type == RegisteredEventType::kLazy
                             ? kRegisteredLazyEvents
                             : kRegisteredServiceWorkerEvents;
  extension_prefs_->UpdateExtensionPref(
      extension_id, pref_key,
      base::Value::ToUniquePtrValue(std::move(events_value)));
}

void EventRouter::AddFilterToEvent(const std::string& event_name,
                                   const std::string& extension_id,
                                   bool is_for_service_worker,
                                   const base::Value::Dict& filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id,
      is_for_service_worker ? kFilteredServiceWorkerEvents : kFilteredEvents);
  auto filtered_events = update.Create();

  base::Value::List* filter_list = nullptr;
  if (!filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    filtered_events->SetKey(event_name, base::Value(base::Value::List()));
    filtered_events->GetListWithoutPathExpansion(event_name, &filter_list);
  }

  filter_list->Append(filter.Clone());
}

void EventRouter::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // TODO(richardzh): revisit here once we create separate lazy listeners for
  // regular and incognito(split) context. How do we ensure lazy listeners and
  // regular listeners are loaded for both browser context.

  // Add all registered lazy listeners to our cache.
  std::set<std::string> registered_events =
      GetRegisteredEvents(extension->id(), RegisteredEventType::kLazy);
  listeners_.LoadUnfilteredLazyListeners(extension->id(), registered_events);

  std::set<std::string> registered_worker_events =
      GetRegisteredEvents(extension->id(), RegisteredEventType::kServiceWorker);
  listeners_.LoadUnfilteredWorkerListeners(browser_context, extension->id(),
                                           registered_worker_events);

  const base::Value::Dict* filtered_events =
      GetFilteredEvents(extension->id(), RegisteredEventType::kLazy);
  if (filtered_events)
    listeners_.LoadFilteredLazyListeners(browser_context, extension->id(),
                                         false /* is_for_service_worker */,
                                         *filtered_events);

  const base::Value::Dict* filtered_worker_events =
      GetFilteredEvents(extension->id(), RegisteredEventType::kServiceWorker);
  if (filtered_worker_events)
    listeners_.LoadFilteredLazyListeners(browser_context, extension->id(),
                                         true /* is_for_service_worker */,
                                         *filtered_worker_events);
}

void EventRouter::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  // Remove all registered listeners from our cache.
  listeners_.RemoveListenersForExtension(extension->id());
}

void EventRouter::AddLazyEventListenerImpl(
    std::unique_ptr<EventListener> listener,
    RegisteredEventType type) {
  const ExtensionId extension_id = listener->extension_id();
  const std::string event_name = listener->event_name();
  bool is_new = listeners_.AddListener(std::move(listener));
  if (is_new) {
    std::set<std::string> events = GetRegisteredEvents(extension_id, type);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      SetRegisteredEvents(extension_id, events, type);
  }
}

void EventRouter::RemoveLazyEventListenerImpl(
    std::unique_ptr<EventListener> listener,
    RegisteredEventType type) {
  const ExtensionId extension_id = listener->extension_id();
  const std::string event_name = listener->event_name();
  bool did_exist = listeners_.RemoveListener(listener.get());
  if (did_exist) {
    std::set<std::string> events = GetRegisteredEvents(extension_id, type);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    SetRegisteredEvents(extension_id, events, type);
  }
}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             base::Value::List event_args)
    : Event(histogram_value, event_name, std::move(event_args), nullptr) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             base::Value::List event_args,
             content::BrowserContext* restrict_to_browser_context)
    : Event(histogram_value,
            event_name,
            std::move(event_args),
            restrict_to_browser_context,
            GURL(),
            EventRouter::USER_GESTURE_UNKNOWN,
            mojom::EventFilteringInfo::New()) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             base::Value::List event_args_tmp,
             content::BrowserContext* restrict_to_browser_context,
             const GURL& event_url,
             EventRouter::UserGestureState user_gesture,
             mojom::EventFilteringInfoPtr info)
    : histogram_value(histogram_value),
      event_name(event_name),
      event_args(std::move(event_args_tmp)),
      restrict_to_browser_context(restrict_to_browser_context),
      event_url(event_url),
      user_gesture(user_gesture),
      filter_info(std::move(info)) {
  DCHECK_NE(events::UNKNOWN, histogram_value)
      << "events::UNKNOWN cannot be used as a histogram value.\n"
      << "If this is a test, use events::FOR_TEST.\n"
      << "If this is production code, it is important that you use a realistic "
      << "value so that we can accurately track event usage. "
      << "See extension_event_histogram_value.h for inspiration.";
}

Event::~Event() = default;

std::unique_ptr<Event> Event::DeepCopy() const {
  auto copy =
      std::make_unique<Event>(histogram_value, event_name, event_args.Clone(),
                              restrict_to_browser_context, event_url,
                              user_gesture, filter_info.Clone());
  copy->will_dispatch_callback = will_dispatch_callback;
  copy->did_dispatch_callback = did_dispatch_callback;
  return copy;
}

// This constructor is only used by tests, for non-ServiceWorker context
// (background page, popup, tab, etc).
// is_lazy flag default to false.
EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id,
                                     const GURL& listener_url,
                                     content::BrowserContext* browser_context)
    : event_name(event_name),
      extension_id(extension_id),
      listener_url(listener_url),
      browser_context(browser_context),
      worker_thread_id(kMainThreadId),
      service_worker_version_id(blink::mojom::kInvalidServiceWorkerVersionId),
      is_lazy(false) {}

EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id,
                                     const GURL& listener_url,
                                     content::BrowserContext* browser_context,
                                     int worker_thread_id,
                                     int64_t service_worker_version_id,
                                     bool is_lazy)
    : event_name(event_name),
      extension_id(extension_id),
      listener_url(listener_url),
      browser_context(browser_context),
      worker_thread_id(worker_thread_id),
      service_worker_version_id(service_worker_version_id),
      is_lazy(is_lazy) {}

}  // namespace extensions
