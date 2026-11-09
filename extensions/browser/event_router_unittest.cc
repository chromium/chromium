// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/listener_registration_phase_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-blink-forward.h"

using base::Value;

namespace extensions {

namespace {

constexpr char kTestEventName[] = "testapi.onEvent";
constexpr SimpleFeatureData kTestEventFeatureData = {
    .feature = {.name = kTestEventName}};

// A simple mock to keep track of listener additions and removals.
class MockEventRouterObserver : public EventRouter::Observer {
 public:
  MockEventRouterObserver()
      : listener_added_count_(0),
        listener_removed_count_(0),
        listener_updated_count_(0) {}

  MockEventRouterObserver(const MockEventRouterObserver&) = delete;
  MockEventRouterObserver& operator=(const MockEventRouterObserver&) = delete;

  ~MockEventRouterObserver() override = default;

  int listener_added_count() const { return listener_added_count_; }
  int listener_removed_count() const { return listener_removed_count_; }
  int listener_updated_count() const { return listener_updated_count_; }
  const std::string& last_event_name() const { return last_event_name_; }

  void Reset() {
    listener_added_count_ = 0;
    listener_removed_count_ = 0;
    listener_updated_count_ = 0;
    last_event_name_.clear();
  }

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override {
    listener_added_count_++;
    last_event_name_ = details.event_name;
  }

  void OnListenerRemoved(const EventListenerInfo& details) override {
    listener_removed_count_++;
    last_event_name_ = details.event_name;
  }

  void OnListenerUpdated(const EventListenerInfo& details) override {
    listener_updated_count_++;
    last_event_name_ = details.event_name;
  }

 private:
  int listener_added_count_;
  int listener_removed_count_;
  int listener_updated_count_;
  std::string last_event_name_;
};

class MockEventDispatcher : public mojom::EventDispatcher {
 public:
  MockEventDispatcher() = default;
  ~MockEventDispatcher() override = default;

  mojo::PendingAssociatedRemote<mojom::EventDispatcher> BindAndPassRemote() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

  // mojom::EventDispatcher:
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     const scoped_refptr<const EventArgs>& event_args,
                     DispatchEventCallback callback) override {
    std::move(callback).Run(
        /*event_will_run_in_lazy_background_page_script=*/false);
  }

 private:
  mojo::AssociatedReceiver<mojom::EventDispatcher> receiver_{this};
};

using EventListenerConstructor =
    base::RepeatingCallback<std::unique_ptr<EventListener>(
        const std::string& /* event_name */,
        content::RenderProcessHost* /* process */,
        base::DictValue /* filter */)>;

std::unique_ptr<EventListener> CreateEventListenerForExtension(
    const ExtensionId& extension_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::DictValue filter) {
  return EventListener::ForExtension(event_name, extension_id, process,
                                     std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForURL(
    const GURL& listener_url,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::DictValue filter) {
  return EventListener::ForURL(event_name, listener_url, process,
                               std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForExtensionServiceWorker(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::DictValue filter) {
  content::BrowserContext* browser_context =
      process ? process->GetBrowserContext() : nullptr;
  return EventListener::ForExtensionServiceWorker(
      event_name, extension_id, process, browser_context,
      Extension::GetBaseURLFromExtensionId(extension_id),
      service_worker_version_id, worker_thread_id, std::move(filter));
}

// Creates an extension.  If |component| is true, it is created as a component
// extension.  If |persistent| is true, it is created with a persistent
// background page; otherwise it is created with an event page.
scoped_refptr<const Extension> CreateExtension(bool component,
                                               bool persistent) {
  ExtensionBuilder builder;
  auto manifest = base::DictValue()
                      .Set("name", "foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2);
  manifest.SetByDottedPath("background.page", "background.html");
  manifest.SetByDottedPath("background.persistent", persistent);
  builder.SetManifest(std::move(manifest));
  if (component) {
    builder.SetLocation(mojom::ManifestLocation::kComponent);
  }

  return builder.Build();
}

scoped_refptr<const Extension> CreateServiceWorkerExtension() {
  ExtensionBuilder builder;
  auto manifest = base::DictValue()
                      .Set("name", "foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2);
  manifest.SetByDottedPath("background.service_worker", "worker.js");
  builder.SetManifest(std::move(manifest));
  return builder.Build();
}

base::DictValue CreateHostSuffixFilter(const std::string& suffix) {
  base::DictValue filter_dict;
  filter_dict.Set("hostSuffix", Value(suffix));

  base::ListValue filter_list;
  filter_list.Append(std::move(filter_dict));

  base::DictValue filter;
  filter.Set("url", std::move(filter_list));
  return filter;
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const EventTarget& e) {
  return os << "EventTarget{" << e.extension_id << "," << e.render_process_id
            << "," << e.service_worker_version_id << "," << e.worker_thread_id
            << "}";
}

class EventRouterTest : public ExtensionsTest {
 public:
  EventRouterTest() = default;

  EventRouterTest(const EventRouterTest&) = delete;
  EventRouterTest& operator=(const EventRouterTest&) = delete;

  void SetUp() override {
    ExtensionsTest::SetUp();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

 protected:
  // Tests adding and removing observers from EventRouter.
  void RunEventRouterObserverTest(const EventListenerConstructor& constructor);

  // Tests that the correct counts are recorded for the Extensions.Events
  // histograms.
  void ExpectHistogramCounts(int dispatch_count,
                             int component_count,
                             int persistent_count,
                             int suspended_count,
                             int running_count,
                             int service_worker_count) {
    histogram_tester_.ExpectBucketCount("Extensions.Events.Dispatch",
                                        events::HistogramValue::FOR_TEST,
                                        dispatch_count);
    histogram_tester_.ExpectBucketCount("Extensions.Events.DispatchToComponent",
                                        events::HistogramValue::FOR_TEST,
                                        component_count);
    histogram_tester_.ExpectBucketCount(
        "Extensions.Events.DispatchWithPersistentBackgroundPage",
        events::HistogramValue::FOR_TEST, persistent_count);
    histogram_tester_.ExpectBucketCount(
        "Extensions.Events.DispatchWithSuspendedEventPage",
        events::HistogramValue::FOR_TEST, suspended_count);
    histogram_tester_.ExpectBucketCount(
        "Extensions.Events.DispatchWithRunningEventPage",
        events::HistogramValue::FOR_TEST, running_count);
    histogram_tester_.ExpectBucketCount(
        "Extensions.Events.DispatchWithServiceWorkerBackground",
        events::HistogramValue::FOR_TEST, service_worker_count);
  }

 private:
  base::HistogramTester histogram_tester_;

  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

class EventRouterFilterTest : public ExtensionsTest,
                              public testing::WithParamInterface<bool> {
 public:
  EventRouterFilterTest() = default;

  EventRouterFilterTest(const EventRouterFilterTest&) = delete;
  EventRouterFilterTest& operator=(const EventRouterFilterTest&) = delete;

  void SetUp() override {
    ExtensionsTest::SetUp();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

  EventRouter* event_router() { return EventRouter::Get(browser_context()); }

  const base::DictValue* GetFilteredEvents(const ExtensionId& extension_id) {
    return event_router()->GetFilteredEvents(
        extension_id, is_for_service_worker()
                          ? EventRouter::RegisteredEventType::kServiceWorker
                          : EventRouter::RegisteredEventType::kLazy);
  }

  bool ContainsFilter(const ExtensionId& extension_id,
                      const std::string& event_name,
                      const base::DictValue& to_check) {
    const base::ListValue* filter_list =
        GetFilterList(extension_id, event_name);
    if (!filter_list) {
      ADD_FAILURE();
      return false;
    }

    for (const base::Value& filter : *filter_list) {
      if (!filter.is_dict()) {
        ADD_FAILURE();
        return false;
      }
      if (filter.GetDict() == to_check) {
        return true;
      }
    }
    return false;
  }

  bool is_for_service_worker() const { return GetParam(); }

 private:
  const base::ListValue* GetFilterList(const ExtensionId& extension_id,
                                       const std::string& event_name) {
    const base::DictValue* filtered_events = GetFilteredEvents(extension_id);
    return filtered_events ? filtered_events->FindList(event_name) : nullptr;
  }

  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

TEST_F(EventRouterTest, GetBaseEventName) {
  // Normal event names are passed through unchanged.
  EXPECT_EQ("foo.onBar", EventRouter::GetBaseEventName("foo.onBar"));

  // Sub-events are converted to the part before the slash.
  EXPECT_EQ("foo.onBar", EventRouter::GetBaseEventName("foo.onBar/123"));
}

// Tests adding and removing observers from EventRouter.
void EventRouterTest::RunEventRouterObserverTest(
    const EventListenerConstructor& constructor) {
  EventRouter router(browser_context(), nullptr);
  std::unique_ptr<EventListener> listener =
      constructor.Run("event_name", render_process_host(), base::DictValue());

  // Add/remove works without any observers.
  router.OnListenerAdded(listener.get());
  router.OnListenerRemoved(listener.get());

  // Register observers that both match and don't match the event above.
  MockEventRouterObserver matching_observer;
  router.RegisterObserver(&matching_observer, "event_name");
  MockEventRouterObserver non_matching_observer;
  router.RegisterObserver(&non_matching_observer, "other");

  // Adding a listener notifies the appropriate observers.
  router.OnListenerAdded(listener.get());
  EXPECT_EQ(1, matching_observer.listener_added_count());
  EXPECT_EQ(0, non_matching_observer.listener_added_count());

  // Removing a listener notifies the appropriate observers.
  router.OnListenerRemoved(listener.get());
  EXPECT_EQ(1, matching_observer.listener_removed_count());
  EXPECT_EQ(0, non_matching_observer.listener_removed_count());

  // Adding the listener again notifies again.
  router.OnListenerAdded(listener.get());
  EXPECT_EQ(2, matching_observer.listener_added_count());
  EXPECT_EQ(0, non_matching_observer.listener_added_count());

  // Removing the listener again notifies again.
  router.OnListenerRemoved(listener.get());
  EXPECT_EQ(2, matching_observer.listener_removed_count());
  EXPECT_EQ(0, non_matching_observer.listener_removed_count());

  // Adding a listener with a sub-event notifies the main observer with
  // proper details.
  matching_observer.Reset();
  std::unique_ptr<EventListener> sub_event_listener =
      constructor.Run("event_name/1", render_process_host(), base::DictValue());
  router.OnListenerAdded(sub_event_listener.get());
  EXPECT_EQ(1, matching_observer.listener_added_count());
  EXPECT_EQ(0, matching_observer.listener_removed_count());
  EXPECT_EQ("event_name/1", matching_observer.last_event_name());

  // Ditto for removing the listener.
  matching_observer.Reset();
  router.OnListenerRemoved(sub_event_listener.get());
  EXPECT_EQ(0, matching_observer.listener_added_count());
  EXPECT_EQ(1, matching_observer.listener_removed_count());
  EXPECT_EQ("event_name/1", matching_observer.last_event_name());
}

TEST_F(EventRouterTest, EventRouterObserverForExtensions) {
  RunEventRouterObserverTest(
      base::BindRepeating(&CreateEventListenerForExtension, "extension_id"));
}

TEST_F(EventRouterTest, EventRouterObserverForURLs) {
  RunEventRouterObserverTest(base::BindRepeating(
      &CreateEventListenerForURL, GURL("http://google.com/path")));
}

TEST_F(EventRouterTest, EventRouterObserverForServiceWorkers) {
  RunEventRouterObserverTest(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, "extension_id",
      // Placeholder version_id and thread_id.
      99, 199));
}

namespace {

// Tracks event dispatches to a specific process.
class EventRouterObserver : public EventRouter::TestObserver {
 public:
  // Only counts events that match |process_id|.
  explicit EventRouterObserver(int process_id) : process_id_(process_id) {}

  void OnWillDispatchEvent(const Event& event) override {
    // Do nothing.
  }

  void OnDidDispatchEventToProcess(const Event& event,
                                   int process_id) override {
    if (process_id == process_id_) {
      ++dispatch_count;
    }
  }

  int dispatch_count = 0;
  const int process_id_;
};

// A fake that pretends that all contexts are WebUI.
class ProcessMapFake : public ProcessMap {
 public:
  explicit ProcessMapFake(content::BrowserContext* browser_context)
      : ProcessMap(browser_context) {}

  mojom::ContextType GetMostLikelyContextType(
      const Extension* extension,
      content::ChildProcessId process_id,
      const GURL* url) const override {
    return mojom::ContextType::kWebUi;
  }
};

std::unique_ptr<KeyedService> BuildProcessMap(
    content::BrowserContext* profile) {
  return std::make_unique<ProcessMapFake>(profile);
}

}  // namespace

TEST_F(EventRouterTest, WebUIEventsDoNotCrossIncognitoBoundaries) {
  // Override ProcessMap to allow routing to WebUI.
  ProcessMapFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildProcessMap));
  ProcessMapFactory::GetInstance()->SetTestingFactory(
      incognito_context(), base::BindRepeating(&BuildProcessMap));

  // Create a SimpleFeature to allow this API call to be routed to our test URL.
  static constexpr auto kMatches =
      std::to_array<std::string_view>({"chrome://settings/*"});
  static constexpr SimpleFeatureData kFeatureData = {
      .feature = {.name = kTestEventName},
      .config = {.match_patterns = StaticSpan(kMatches)},
  };
  SimpleFeature feature{StaticFeatureData(kFeatureData)};
  std::array<Feature*, 1> features = {&feature};
  FeatureProvider provider;
  provider.AddStaticFeatures(features);

  ExtensionAPI api;
  api.RegisterDependencyProvider("api", &provider);
  ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

  EventRouter router(browser_context(), nullptr);
  content::MockRenderProcessHost regular_rph(browser_context());
  content::MockRenderProcessHost otr_rph(incognito_context());

  // Add event listeners, as if we had created two real WebUIs, one in a regular
  // profile and one in an otr profile. Note that the string chrome://settings
  // is hardcoded into the api permissions of settingsPrivate.
  GURL placeholder_url("chrome://settings/test");
  router.AddEventListenerForURL(kTestEventName, &regular_rph, placeholder_url);
  router.AddEventListenerForURL(kTestEventName, &otr_rph, placeholder_url);

  // Hook up some test observers
  EventRouterObserver regular_counter(regular_rph.GetDeprecatedID());
  router.AddObserverForTesting(&regular_counter);
  EventRouterObserver otr_counter(otr_rph.GetDeprecatedID());
  router.AddObserverForTesting(&otr_counter);

  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(0, otr_counter.dispatch_count);

  // Sending an otr event should not trigger the regular observer.
  auto otr_event =
      std::make_unique<Event>(extensions::events::FOR_TEST, kTestEventName,
                              nullptr, incognito_context());
  router.BroadcastEvent(std::move(otr_event));
  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);

  // Setting a regular event should not trigger the otr observer.
  std::unique_ptr<Event> regular_event = std::make_unique<Event>(
      extensions::events::FOR_TEST, kTestEventName, nullptr, browser_context());
  router.BroadcastEvent(std::move(regular_event));
  EXPECT_EQ(1, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);
}

TEST_F(EventRouterTest, MultipleEventRouterObserver) {
  EventRouter router(browser_context(), nullptr);
  std::unique_ptr<EventListener> listener =
      EventListener::ForURL("event_name", GURL("http://google.com/path"),
                            render_process_host(), base::DictValue());

  // Add/remove works without any observers.
  router.OnListenerAdded(listener.get());
  router.OnListenerRemoved(listener.get());

  // Register two observers for same event name.
  MockEventRouterObserver matching_observer1;
  router.RegisterObserver(&matching_observer1, "event_name");
  MockEventRouterObserver matching_observer2;
  router.RegisterObserver(&matching_observer2, "event_name");

  // Adding a listener notifies the appropriate observers.
  router.OnListenerAdded(listener.get());
  EXPECT_EQ(1, matching_observer1.listener_added_count());
  EXPECT_EQ(1, matching_observer2.listener_added_count());

  // Removing a listener notifies the appropriate observers.
  router.OnListenerRemoved(listener.get());
  EXPECT_EQ(1, matching_observer1.listener_removed_count());
  EXPECT_EQ(1, matching_observer2.listener_removed_count());

  // Unregister the observer so that the current observer no longer receives
  // monitoring, but the other observer still continues to receive monitoring.
  router.UnregisterObserver(&matching_observer1);

  router.OnListenerAdded(listener.get());
  EXPECT_EQ(1, matching_observer1.listener_added_count());
  EXPECT_EQ(2, matching_observer2.listener_added_count());
}

TEST_F(EventRouterTest, TestReportEvent) {
  EventRouter router(browser_context(), nullptr);
  scoped_refptr<const Extension> normal = ExtensionBuilder("Test").Build();
  router.ReportEvent(events::HistogramValue::FOR_TEST, normal.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(1 /** Dispatch */, 0 /** DispatchToComponent */,
                        0 /** DispatchWithPersistentBackgroundPage */,
                        0 /** DispatchWithSuspendedEventPage */,
                        0 /** DispatchWithRunningEventPage */,
                        0 /** DispatchWithServiceWorkerBackground */);

  scoped_refptr<const Extension> component =
      CreateExtension(true /** component */, true /** persistent */);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(2, 1, 1, 0, 0, 0);

  scoped_refptr<const Extension> persistent = CreateExtension(false, true);
  router.ReportEvent(events::HistogramValue::FOR_TEST, persistent.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(3, 1, 2, 0, 0, 0);

  scoped_refptr<const Extension> event = CreateExtension(false, false);
  router.ReportEvent(events::HistogramValue::FOR_TEST, event.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(4, 1, 2, 0, 1, 0);
  router.ReportEvent(events::HistogramValue::FOR_TEST, event.get(),
                     true /** did_enqueue */);
  ExpectHistogramCounts(5, 1, 2, 1, 1, 0);

  scoped_refptr<const Extension> component_event = CreateExtension(true, false);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component_event.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(6, 2, 2, 1, 2, 0);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component_event.get(),
                     true /** did_enqueue */);
  ExpectHistogramCounts(7, 3, 2, 2, 2, 0);

  scoped_refptr<const Extension> service_worker_extension =
      CreateServiceWorkerExtension();
  router.ReportEvent(events::HistogramValue::FOR_TEST,
                     service_worker_extension.get(), true /** did_enqueue */);
  ExpectHistogramCounts(8, 3, 2, 2, 2, 1);
}

// Tests that when an event is dispatched with a null context,
// `cannot_dispatch_callback` is still run. Regression test for
// crbug.com/484218883.
TEST_F(EventRouterTest, DispatchPendingEvent_NullContext) {
  EventRouter* router = EventRouter::Get(browser_context());
  auto event = std::make_unique<Event>(
      extensions::events::FOR_TEST, "test.event", nullptr, browser_context());
  base::RunLoop run_loop;
  event->cannot_dispatch_callback = run_loop.QuitClosure();

  router->DispatchPendingEvent(std::move(event), nullptr);

  run_loop.Run();
}

TEST_F(EventRouterTest, AddLazyListenerForUnloadedExtension) {
  EventRouter* router = EventRouter::Get(browser_context());
  const std::string kEventName1 = "webNavigation.onBeforeNavigate";
  const std::string kEventName2 = "webNavigation.onBeforeNavigate";

  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  EXPECT_FALSE(router->IsExtensionEnabled(kExtensionId));

  // === Main Thread ===
  router->AddLazyListenerForMainThreadImpl(kExtensionId, kEventName1);
  // The listener should not be registered.
  EXPECT_FALSE(router->ExtensionHasEventListener(kExtensionId, kEventName1));
  // The listener should be persisted to prefs.
  auto registered_events = router->GetRegisteredEvents(
      kExtensionId, EventRouter::RegisteredEventType::kLazy);
  EXPECT_TRUE(registered_events.contains(kEventName1));

  // === Service Worker ===
  router->AddLazyListenerForServiceWorkerImpl(
      *browser_context(), kExtensionId,
      Extension::GetBaseURLFromExtensionId(kExtensionId), kEventName2);
  // The listener should not be registered. We don't want to add listeners to
  // contexts that are being shut down.
  EXPECT_FALSE(router->ExtensionHasEventListener(kExtensionId, kEventName2));
  // The listener should be persisted to prefs, because, even if the context was
  // shutting down, we still want a record of the events for which to wake up
  // the extension.
  auto registered_sw_events = router->GetRegisteredEvents(
      kExtensionId, EventRouter::RegisteredEventType::kServiceWorker);
  EXPECT_TRUE(registered_sw_events.count(kEventName2));
}

// TODO(crbug.com/474558883): Remove this in M157.
TEST_F(EventRouterTest, RemovesOrphanedWebRequestEvents) {
  EventRouter* router = EventRouter::Get(browser_context());
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  // Manually add orphaned events to prefs.
  router->AddLazyListenerForMainThreadImpl(extension->id(),
                                           "webRequest.onBeforeRequest/s1");
  router->AddLazyListenerForServiceWorkerImpl(
      *browser_context(), extension->id(),
      Extension::GetBaseURLFromExtensionId(extension->id()),
      "webRequest.onBeforeRequest/s2");

  router->AddLazyListenerForMainThreadImpl(extension->id(),
                                           "webViewInternal.onMessage/s1");
  router->AddLazyListenerForServiceWorkerImpl(
      *browser_context(), extension->id(),
      Extension::GetBaseURLFromExtensionId(extension->id()),
      "webViewInternal.onMessage/s2");

  // Add non-orphaned events to ensure they are kept.
  router->AddLazyListenerForMainThreadImpl(extension->id(), "tabs.onCreated");
  router->AddLazyListenerForServiceWorkerImpl(
      *browser_context(), extension->id(),
      Extension::GetBaseURLFromExtensionId(extension->id()), "tabs.onRemoved");

  router->AddLazyListenerForMainThreadImpl(extension->id(),
                                           "webRequest.onActionIgnored");
  router->AddLazyListenerForServiceWorkerImpl(
      *browser_context(), extension->id(),
      Extension::GetBaseURLFromExtensionId(extension->id()),
      "webRequest.onActionIgnored");

  // Trigger OnExtensionLoaded.
  router->OnExtensionLoaded(browser_context(), extension.get());

  // Verify the orphaned events were removed from prefs.
  auto lazy_events = router->GetRegisteredEvents(
      extension->id(), EventRouter::RegisteredEventType::kLazy);
  EXPECT_TRUE(lazy_events.contains("tabs.onCreated"));
  EXPECT_TRUE(lazy_events.contains("webRequest.onActionIgnored"));
  EXPECT_FALSE(lazy_events.contains("webRequest.onBeforeRequest/s1"));
  EXPECT_FALSE(lazy_events.contains("webViewInternal.onMessage/s1"));

  auto sw_events = router->GetRegisteredEvents(
      extension->id(), EventRouter::RegisteredEventType::kServiceWorker);
  EXPECT_TRUE(sw_events.contains("tabs.onRemoved"));
  EXPECT_TRUE(sw_events.contains("webRequest.onActionIgnored"));
  EXPECT_FALSE(sw_events.contains("webRequest.onBeforeRequest/s2"));
  EXPECT_FALSE(sw_events.contains("webViewInternal.onMessage/s2"));
}

// Tests adding and removing events with filters.
// TODO(crbug.com/40281129): test is flaky across platforms.
TEST_P(EventRouterFilterTest, DISABLED_Basic) {
  // For the purpose of this test, "." is important in |event_name| as it
  // exercises the code path that uses |event_name| as a key in
  // base::DictValue.
  const std::string kEventName = "webNavigation.onBeforeNavigate";

  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);
  const std::string kHostSuffixes[] = {"foo.com", "bar.com", "baz.com"};

  // The extension must be enabled so the lazy listeners actually land in the
  // in-memory map; otherwise removal never reaches the persisted filters.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetID(kExtensionId).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }
  std::vector<base::DictValue> filters;
  for (const auto& host_suffix : kHostSuffixes) {
    base::DictValue filter = CreateHostSuffixFilter(host_suffix);
    event_router()->AddFilteredEventListener(
        kEventName, render_process_host(), param.Clone(), worker_context.get(),
        filter, true);
    filters.push_back(std::move(filter));
  }

  const base::DictValue* filtered_events = GetFilteredEvents(kExtensionId);
  ASSERT_TRUE(filtered_events);
  ASSERT_EQ(1u, filtered_events->size());

  const auto iter = filtered_events->begin();
  ASSERT_EQ(kEventName, iter->first);
  ASSERT_TRUE(iter->second.is_list());
  ASSERT_EQ(3u, iter->second.GetList().size());

  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[0]));
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[1]));
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[2]));

  // Remove the second filter.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filters[1], true);
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[0]));
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[1]));
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[2]));

  // Remove the first filter.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filters[0], true);
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[0]));
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[1]));
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filters[2]));

  // Removing the third filter erases the empty event key.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filters[2], true);
  const base::DictValue* remaining_events = GetFilteredEvents(kExtensionId);
  ASSERT_TRUE(remaining_events);
  EXPECT_FALSE(remaining_events->contains(kEventName));
}

TEST_P(EventRouterFilterTest, AddFilteredLazyListenerForUnloadedExtension) {
  const std::string kEventName = "webRequest.onBeforeRequest";
  const base::DictValue filter = CreateHostSuffixFilter("example.com");

  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  EXPECT_FALSE(event_router()->IsExtensionEnabled(kExtensionId));

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }

  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(),
      mojom::EventListenerOwner::NewExtensionId(kExtensionId),
      worker_context.get(), filter, /*add_lazy_listener=*/true);

  // The listener should not be registered.
  EXPECT_FALSE(
      event_router()->ExtensionHasEventListener(kExtensionId, kEventName));
  // The listener should be persisted to prefs.
  EXPECT_TRUE(ContainsFilter(kExtensionId, kEventName, filter));
}

// Re-registering a sub-event-named listener with a different filter must
// replace the persisted filter rather than append, so that prefs do not
// accumulate stale filters across service-worker invocations.
// Regression test for crbug.com/502402731.
TEST_P(EventRouterFilterTest, SubEventNamedListenerReplacesPersistedFilter) {
  const std::string kEventName = "webRequest.onBeforeRequest/s0";
  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }

  // Register a listener for "foo.com".
  const base::DictValue filter_foo = CreateHostSuffixFilter("foo.com");
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_foo, /*add_lazy_listener=*/true);

  // Verify that the filter was stored correctly: there should be exactly one
  // filtered event entry containing the "foo.com" filter.
  {
    const base::DictValue* filtered_events = GetFilteredEvents(kExtensionId);
    ASSERT_TRUE(filtered_events);
    ASSERT_EQ(1u, filtered_events->size());
    const auto iter = filtered_events->begin();
    ASSERT_EQ(kEventName, iter->first);
    ASSERT_TRUE(iter->second.is_list());
    ASSERT_EQ(1u, iter->second.GetList().size());
    EXPECT_TRUE(ContainsFilter(kExtensionId, kEventName, filter_foo));
  }

  // Re-register the exact same event name but with "bar.com".
  // This simulates a Service Worker waking up and updating its listeners.
  const base::DictValue filter_bar = CreateHostSuffixFilter("bar.com");
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_bar, /*add_lazy_listener=*/true);

  // Retrieve the stored events again. We expect the EventRouter to have
  // swapped "foo.com" for "bar.com" rather than having a list of two filters.
  const base::DictValue* filtered_events = GetFilteredEvents(kExtensionId);
  ASSERT_TRUE(filtered_events);
  ASSERT_EQ(1u, filtered_events->size());
  const auto iter = filtered_events->begin();
  ASSERT_EQ(kEventName, iter->first);
  ASSERT_TRUE(iter->second.is_list());
  // "foo.com" should be gone, and "bar.com" should be present.
  ASSERT_EQ(1u, iter->second.GetList().size());
  EXPECT_FALSE(ContainsFilter(kExtensionId, kEventName, filter_foo));
  EXPECT_TRUE(ContainsFilter(kExtensionId, kEventName, filter_bar));
}

// Re-registering a sub-event-named listener with a different filter must
// update the in-memory lazy listener in place rather than accumulate a stale
// entry alongside the new one. Regression test for crbug.com/508672617.
TEST_P(EventRouterFilterTest,
       SubEventNamedListenerReplacesInMemoryLazyListener) {
  const std::string kEventName = "webRequest.onBeforeRequest/s0";
  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);

  // The extension must be enabled so the lazy listener actually lands in the
  // in-memory map.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetID(kExtensionId).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }

  // Register a listener for "foo.com".
  const base::DictValue filter_foo = CreateHostSuffixFilter("foo.com");
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_foo, /*add_lazy_listener=*/true);
  ASSERT_TRUE(event_router()->HasLazyEventListenerWithFilterForTesting(
      kEventName, filter_foo));

  MockEventRouterObserver observer;
  event_router()->RegisterObserver(&observer,
                                   EventRouter::GetBaseEventName(kEventName));

  // Re-register the same sub-event with a different filter.
  const base::DictValue filter_bar = CreateHostSuffixFilter("bar.com");
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_bar, /*add_lazy_listener=*/true);

  // The lazy listener's filter changed but the listener itself was neither
  // added nor removed: observers see exactly one `OnListenerUpdated()` and no
  // `OnListenerRemoved()`. The single `OnListenerAdded()` is for the (separate)
  // active listener's re-registration, not the lazy one.
  EXPECT_EQ(0, observer.listener_removed_count());
  EXPECT_EQ(1, observer.listener_updated_count());
  EXPECT_EQ(1, observer.listener_added_count());
  event_router()->UnregisterObserver(&observer);

  // The new lazy listener is present and the stale one is gone.
  EXPECT_TRUE(event_router()->HasLazyEventListenerWithFilterForTesting(
      kEventName, filter_bar));
  EXPECT_FALSE(event_router()->HasLazyEventListenerWithFilterForTesting(
      kEventName, filter_foo));

  // Prefs also reflect only the latest filter.
  EXPECT_TRUE(ContainsFilter(kExtensionId, kEventName, filter_bar));
  EXPECT_FALSE(ContainsFilter(kExtensionId, kEventName, filter_foo));
}

// Tests that removing the last filter of a sub-event listener deletes the empty
// list and its key from preferences. Regression test for crbug.com/526929792.
TEST_P(EventRouterFilterTest, RemoveLastFilterErasesSubEventKey) {
  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);

  // The extension must be enabled so lazy listeners are tracked in memory;
  // otherwise removal does not update persisted filters.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetID(kExtensionId).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }

  const base::DictValue filter = CreateHostSuffixFilter("foo.com");
  for (int i = 0; i < 3; ++i) {
    // Each addListener call in the renderer creates a unique sub-event name.
    const std::string event_name =
        "webRequest.onBeforeRequest/s" + base::NumberToString(i);
    event_router()->AddFilteredEventListener(
        event_name, render_process_host(), param.Clone(), worker_context.get(),
        filter, /*add_lazy_listener=*/true);
    ASSERT_TRUE(ContainsFilter(kExtensionId, event_name, filter));

    event_router()->RemoveFilteredEventListener(
        event_name, render_process_host(), param.Clone(), worker_context.get(),
        filter, /*remove_lazy_listener=*/true);
    const base::DictValue* filtered_events = GetFilteredEvents(kExtensionId);
    ASSERT_TRUE(filtered_events);
    EXPECT_FALSE(filtered_events->contains(event_name));
  }

  // No keys should remain after all filters are removed.
  EXPECT_TRUE(GetFilteredEvents(kExtensionId)->empty());
}

// Tests that removing the last filter from a standard (non-sub-event) filtered
// event deletes its key from preferences. Regression test for
// crbug.com/526929792.
TEST_P(EventRouterFilterTest, RemoveLastFilterErasesEventKey) {
  const std::string kEventName = "webNavigation.onBeforeNavigate";
  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);

  // The extension must be enabled so lazy listeners are tracked in memory;
  // otherwise removal does not update persisted filters.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetID(kExtensionId).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Placeholder version_id.
        199);  // Placeholder thread_id.
  }

  const base::DictValue filter_foo = CreateHostSuffixFilter("foo.com");
  const base::DictValue filter_bar = CreateHostSuffixFilter("bar.com");
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_foo, /*add_lazy_listener=*/true);
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_bar, /*add_lazy_listener=*/true);

  // Removing one filter leaves the other in place, so the key is preserved.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_foo, /*remove_lazy_listener=*/true);
  ASSERT_TRUE(ContainsFilter(kExtensionId, kEventName, filter_bar));

  // Removing the last filter erases the key.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filter_bar, /*remove_lazy_listener=*/true);
  const base::DictValue* filtered_events = GetFilteredEvents(kExtensionId);
  ASSERT_TRUE(filtered_events);
  EXPECT_FALSE(filtered_events->contains(kEventName));
  EXPECT_TRUE(filtered_events->empty());
}

// TODO(crbug.com/40281129): test is flaky across platforms.
TEST_P(EventRouterFilterTest, DISABLED_URLBasedFilteredEventListener) {
  const std::string kEventName = "windows.onRemoved";
  const GURL kUrl("chrome-untrusted://terminal");
  base::DictValue filter;
  bool lazy = false;
  EXPECT_FALSE(event_router()->HasEventListener(kEventName));
  event_router()->AddFilteredEventListener(
      kEventName, render_process_host(),
      mojom::EventListenerOwner::NewListenerUrl(kUrl), nullptr, filter, lazy);
  EXPECT_TRUE(event_router()->HasEventListener(kEventName));
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(),
      mojom::EventListenerOwner::NewListenerUrl(kUrl), nullptr, filter, lazy);
  EXPECT_FALSE(event_router()->HasEventListener(kEventName));
}

INSTANTIATE_TEST_SUITE_P(Lazy, EventRouterFilterTest, testing::Values(false));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         EventRouterFilterTest,
                         testing::Values(true));

// Tests lazy listener tracking across registration phase transitions for
// `background.async_listener_registration`, driving `EventRouter` directly.
class EventRouterAsyncListenerRegistrationTest : public EventRouterTest {
 public:
  void SetUp() override {
    EventRouterTest::SetUp();
    // `util::IsIncognitoEnabled()` reads `UserPrefs` on ChromeOS.
    user_prefs::UserPrefs::Set(browser_context(), pref_service());
    user_prefs::UserPrefs::Set(incognito_context(), pref_service());
    // Declare split mode up front; it has no effect unless a test also enables
    // the extension in incognito.
    extension_ = ExtensionBuilder("Test")
                     .SetBackgroundContext(
                         ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
                     .SetManifestKey("incognito", "split")
                     .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
  }

 protected:
  EventRouter* event_router() { return EventRouter::Get(browser_context()); }
  ListenerRegistrationPhaseMap& phases() {
    return event_router()->listener_registration_phases();
  }
  const ExtensionId& extension_id() const { return extension_->id(); }

  // Adds or removes a lazy listener for the worker in `browser_context`.
  void AddLazyListener(content::BrowserContext& browser_context,
                       const std::string& event_name) {
    event_router()->AddLazyListenerForServiceWorkerImpl(
        browser_context, extension_id(), extension_->url(), event_name);
  }
  void RemoveLazyListener(content::BrowserContext& browser_context,
                          const std::string& event_name) {
    event_router()->RemoveLazyListenerForServiceWorkerImpl(
        browser_context, extension_id(), extension_->url(), event_name);
  }

  // Adds or removes a filtered listener (active and lazy) for the regular
  // worker.
  void AddFilteredLazyListener(const std::string& event_name,
                               const base::DictValue& filter) {
    mojom::ServiceWorkerContext worker_context = GetWorkerContext();
    event_router()->AddFilteredEventListener(
        event_name, render_process_host(),
        mojom::EventListenerOwner::NewExtensionId(extension_id()),
        &worker_context, filter, /*add_lazy_listener=*/true);
  }
  void RemoveFilteredLazyListener(const std::string& event_name,
                                  const base::DictValue& filter) {
    mojom::ServiceWorkerContext worker_context = GetWorkerContext();
    event_router()->RemoveFilteredEventListener(
        event_name, render_process_host(),
        mojom::EventListenerOwner::NewExtensionId(extension_id()),
        &worker_context, filter, /*remove_lazy_listener=*/true);
  }

  bool HasLazyListener(const std::string& event_name) {
    return event_router()->HasLazyEventListenerForTesting(event_name);
  }
  bool HasFilteredLazyListener(const std::string& event_name,
                               const base::DictValue& filter) {
    return event_router()->HasLazyEventListenerWithFilterForTesting(event_name,
                                                                    filter);
  }
  std::set<std::string> GetPersistedEvents() {
    return event_router()->GetRegisteredEvents(
        extension_id(), EventRouter::RegisteredEventType::kServiceWorker);
  }
  base::ListValue GetPersistedFilters(const std::string& event_name) {
    const base::DictValue* filtered_events = event_router()->GetFilteredEvents(
        extension_id(), EventRouter::RegisteredEventType::kServiceWorker);
    const base::ListValue* filters =
        filtered_events ? filtered_events->FindList(event_name) : nullptr;
    return filters ? filters->Clone() : base::ListValue();
  }

 private:
  // Placeholder version and thread IDs. They only need to stay the same across
  // calls, so that removals match the active listener added earlier.
  mojom::ServiceWorkerContext GetWorkerContext() const {
    return mojom::ServiceWorkerContext(extension_->url(), /*version_id=*/99,
                                       /*thread_id=*/199);
  }

  scoped_refptr<const Extension> extension_;
};

// Tests that listener changes during a registration phase only take effect in
// memory until `Commit()` replaces the persisted set, and that post-commit
// changes are persisted immediately. Also verifies `EventRouter::Observer`s
// aren't notified for listeners that stay registered across the commit.
TEST_F(EventRouterAsyncListenerRegistrationTest,
       CommitReplacesPersistedListeners) {
  // Seed two persisted listeners from a previous run.
  AddLazyListener(*browser_context(), "test.onKept");
  AddLazyListener(*browser_context(), "test.onDropped");

  MockEventRouterObserver kept_observer;
  MockEventRouterObserver dropped_observer;
  MockEventRouterObserver added_observer;
  event_router()->RegisterObserver(&kept_observer, "test.onKept");
  event_router()->RegisterObserver(&dropped_observer, "test.onDropped");
  event_router()->RegisterObserver(&added_observer, "test.onAdded");

  // Start a new phase that re-registers `onKept` and adds `onAdded`, omitting
  // `onDropped`. All three remain in memory during the phase, while prefs still
  // hold the previous run's set.
  const blink::ServiceWorkerToken kWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  AddLazyListener(*browser_context(), "test.onKept");
  AddLazyListener(*browser_context(), "test.onAdded");
  EXPECT_TRUE(HasLazyListener("test.onKept"));
  EXPECT_TRUE(HasLazyListener("test.onDropped"));
  EXPECT_TRUE(HasLazyListener("test.onAdded"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onDropped", "test.onKept"}));
  // `onKept` was already in memory, so only `onAdded` triggers
  // `OnListenerAdded()`.
  EXPECT_EQ(0, kept_observer.listener_added_count());
  EXPECT_EQ(1, added_observer.listener_added_count());

  // Committing writes `{onKept, onAdded}` to prefs and drops `onDropped` from
  // memory. Kept listeners stay in place without firing remove/add
  // notifications.
  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kWorkerToken));
  EXPECT_FALSE(HasLazyListener("test.onDropped"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onAdded", "test.onKept"}));
  EXPECT_EQ(1, dropped_observer.listener_removed_count());
  EXPECT_EQ(0, dropped_observer.listener_added_count());
  EXPECT_EQ(0, kept_observer.listener_removed_count());
  EXPECT_EQ(0, kept_observer.listener_added_count());
  EXPECT_EQ(0, added_observer.listener_removed_count());
  EXPECT_EQ(1, added_observer.listener_added_count());
  event_router()->UnregisterObserver(&kept_observer);
  event_router()->UnregisterObserver(&dropped_observer);
  event_router()->UnregisterObserver(&added_observer);

  // Post-commit updates are persisted immediately.
  AddLazyListener(*browser_context(), "test.onAddedLater");
  RemoveLazyListener(*browser_context(), "test.onKept");
  EXPECT_TRUE(HasLazyListener("test.onAddedLater"));
  EXPECT_FALSE(HasLazyListener("test.onKept"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onAdded", "test.onAddedLater"}));
}

// Tests that lazy listeners added after a registration phase aborts are
// ignored, while previously persisted listeners remain registered.
TEST_F(EventRouterAsyncListenerRegistrationTest,
       IgnoresListenersAddedAfterAbort) {
  // Seed a listener persisted by a previous run.
  AddLazyListener(*browser_context(), "test.onPersisted");

  const blink::ServiceWorkerToken kWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  phases().AbortForInstance(extension_id(), *browser_context(), kWorkerToken);

  AddLazyListener(*browser_context(), "test.onLate");
  AddLazyListener(*browser_context(), "test.onPersisted");
  EXPECT_FALSE(HasLazyListener("test.onLate"));
  EXPECT_TRUE(HasLazyListener("test.onPersisted"));
  EXPECT_EQ(GetPersistedEvents(), (std::set<std::string>{"test.onPersisted"}));
}

// Tests that a filtered listener removed during a registration phase stays in
// memory while it's still persisted from a previous run, and that one removed
// after commit is dropped from memory and prefs right away.
TEST_F(EventRouterAsyncListenerRegistrationTest,
       FilteredListenerRemovalBeforeAndAfterCommit) {
  static constexpr char kEventName[] = "test.onFiltered";
  const base::DictValue foo_filter = CreateHostSuffixFilter("foo.com");
  const base::DictValue bar_filter = CreateHostSuffixFilter("bar.com");
  const base::ListValue expected_filters =
      base::ListValue().Append(foo_filter.Clone());

  // Seed `foo_filter` as persisted by a previous run.
  AddFilteredLazyListener(kEventName, foo_filter);
  ASSERT_EQ(GetPersistedFilters(kEventName), expected_filters);

  // The next worker adds and removes both filters. `foo_filter` stays in memory
  // until commit because it's still persisted, while `bar_filter` is dropped.
  const blink::ServiceWorkerToken kWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  AddFilteredLazyListener(kEventName, foo_filter);
  RemoveFilteredLazyListener(kEventName, foo_filter);
  AddFilteredLazyListener(kEventName, bar_filter);
  RemoveFilteredLazyListener(kEventName, bar_filter);
  EXPECT_TRUE(HasFilteredLazyListener(kEventName, foo_filter));
  EXPECT_FALSE(HasFilteredLazyListener(kEventName, bar_filter));
  EXPECT_EQ(GetPersistedFilters(kEventName), expected_filters);

  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kWorkerToken));
  EXPECT_FALSE(HasFilteredLazyListener(kEventName, foo_filter));
  EXPECT_TRUE(GetPersistedFilters(kEventName).empty());

  // After commit, adding and removing the listener update prefs and memory
  // right away.
  AddFilteredLazyListener(kEventName, foo_filter);
  EXPECT_TRUE(HasFilteredLazyListener(kEventName, foo_filter));
  EXPECT_EQ(GetPersistedFilters(kEventName), expected_filters);
  RemoveFilteredLazyListener(kEventName, foo_filter);
  EXPECT_FALSE(HasFilteredLazyListener(kEventName, foo_filter));
  EXPECT_TRUE(GetPersistedFilters(kEventName).empty());
}

// Tests that for a split-mode extension with incognito enabled, prefs store
// the union of the regular and incognito workers' listeners (including
// previously persisted listeners until the other worker has committed).
TEST_F(EventRouterAsyncListenerRegistrationTest,
       SplitModeWorkersPersistUnionOfTheirListeners) {
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(extension_id(), true);

  // Simulate a listener persisted by an earlier incognito worker run.
  AddLazyListener(*incognito_context(), "test.onPersisted");

  // Both workers register listeners at the same time.
  const blink::ServiceWorkerToken kWorkerToken;
  const blink::ServiceWorkerToken kIncognitoWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  phases().Start(extension_id(), *incognito_context(), kIncognitoWorkerToken);
  AddLazyListener(*browser_context(), "test.onRegular");
  AddLazyListener(*browser_context(), "test.onShared");
  AddLazyListener(*incognito_context(), "test.onShared");
  AddLazyListener(*incognito_context(), "test.onIncognito");

  // `onTemporary` stays registered until both in-progress phases remove it.
  AddLazyListener(*browser_context(), "test.onTemporary");
  AddLazyListener(*incognito_context(), "test.onTemporary");
  RemoveLazyListener(*browser_context(), "test.onTemporary");
  EXPECT_TRUE(HasLazyListener("test.onTemporary"));
  RemoveLazyListener(*incognito_context(), "test.onTemporary");
  EXPECT_FALSE(HasLazyListener("test.onTemporary"));

  // Committing the regular worker keeps `onPersisted` because the incognito
  // worker hasn't committed yet, so its listener set isn't known. The incognito
  // worker's listeners stay in memory, but aren't persisted yet.
  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kWorkerToken));
  EXPECT_TRUE(HasLazyListener("test.onPersisted"));
  EXPECT_TRUE(HasLazyListener("test.onIncognito"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onPersisted", "test.onRegular",
                                   "test.onShared"}));

  // When the incognito worker commits, prefs hold the union of both sets and
  // `onPersisted` is dropped.
  ASSERT_TRUE(phases().Commit(extension_id(), *incognito_context(),
                              kIncognitoWorkerToken));
  EXPECT_FALSE(HasLazyListener("test.onPersisted"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onIncognito", "test.onRegular",
                                   "test.onShared"}));

  // `onShared` stays registered until both workers remove it.
  RemoveLazyListener(*browser_context(), "test.onShared");
  EXPECT_TRUE(HasLazyListener("test.onShared"));
  EXPECT_TRUE(GetPersistedEvents().contains("test.onShared"));
  RemoveLazyListener(*incognito_context(), "test.onShared");
  EXPECT_FALSE(HasLazyListener("test.onShared"));
  EXPECT_FALSE(GetPersistedEvents().contains("test.onShared"));

  // If the regular worker restarts and commits an empty set, `onRegular` is
  // dropped while the incognito worker's `onIncognito` is preserved.
  const blink::ServiceWorkerToken kNextWorkerToken;
  phases().Start(extension_id(), *browser_context(), kNextWorkerToken);
  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kNextWorkerToken));
  EXPECT_FALSE(HasLazyListener("test.onRegular"));
  EXPECT_TRUE(HasLazyListener("test.onIncognito"));
  EXPECT_EQ(GetPersistedEvents(), (std::set<std::string>{"test.onIncognito"}));
}

// Tests that in split mode, post-commit listener removals are deferred in
// prefs and memory until the other worker has also committed, since the
// listener might still belong to that worker.
TEST_F(EventRouterAsyncListenerRegistrationTest,
       SplitModeRemovalWaitsForOtherWorkerCommit) {
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(extension_id(), true);

  const blink::ServiceWorkerToken kWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  AddLazyListener(*browser_context(), "test.onFirst");
  AddLazyListener(*browser_context(), "test.onSecond");
  AddLazyListener(*browser_context(), "test.onRegular");
  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kWorkerToken));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onFirst", "test.onRegular",
                                   "test.onSecond"}));

  // The incognito context exists (`incognito_context()` was initialized in
  // `SetUp()`), but its worker hasn't started or committed yet. Removing
  // `onFirst` on the regular worker must keep it in prefs and memory for now.
  RemoveLazyListener(*browser_context(), "test.onFirst");
  EXPECT_TRUE(HasLazyListener("test.onFirst"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onFirst", "test.onRegular",
                                   "test.onSecond"}));

  // Same while the incognito worker's registration phase is in progress.
  const blink::ServiceWorkerToken kIncognitoWorkerToken;
  phases().Start(extension_id(), *incognito_context(), kIncognitoWorkerToken);
  RemoveLazyListener(*browser_context(), "test.onSecond");
  EXPECT_TRUE(HasLazyListener("test.onSecond"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onFirst", "test.onRegular",
                                   "test.onSecond"}));

  // The incognito worker registers `onSecond` (but not `onFirst`) and commits.
  // `onFirst` is now dropped, while `onSecond` stays.
  AddLazyListener(*incognito_context(), "test.onSecond");
  ASSERT_TRUE(phases().Commit(extension_id(), *incognito_context(),
                              kIncognitoWorkerToken));
  EXPECT_FALSE(HasLazyListener("test.onFirst"));
  EXPECT_TRUE(HasLazyListener("test.onSecond"));
  EXPECT_EQ(GetPersistedEvents(),
            (std::set<std::string>{"test.onRegular", "test.onSecond"}));

  // With both workers committed, removals take effect immediately.
  RemoveLazyListener(*incognito_context(), "test.onSecond");
  EXPECT_FALSE(HasLazyListener("test.onSecond"));
  EXPECT_EQ(GetPersistedEvents(), (std::set<std::string>{"test.onRegular"}));
}

// Tests that `DispatchEventWithLazyListener()` doesn't persist its temporary
// listener or record it in the registration phase. In split mode with an
// uncommitted incognito worker, a persisted listener would stay in prefs and
// memory after the dispatch.
TEST_F(EventRouterAsyncListenerRegistrationTest,
       DispatchEventWithLazyListenerDoesNotPersistListener) {
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(extension_id(), true);
  const blink::ServiceWorkerToken kWorkerToken;
  phases().Start(extension_id(), *browser_context(), kWorkerToken);
  ASSERT_TRUE(
      phases().Commit(extension_id(), *browser_context(), kWorkerToken));
  const auto* phase_listeners =
      phases().GetListeners(extension_id(), *browser_context());
  ASSERT_TRUE(phase_listeners);

  // While the event is dispatched, the temporary listener should only be in
  // memory. Cancel the event before it's queued for the worker (the extension
  // isn't activated in `ServiceWorkerTaskQueue` here).
  bool routed_to_extension = false;
  auto event = std::make_unique<Event>(events::FOR_TEST, "test.onEvent",
                                       base::MakeRefCounted<EventArgs>());
  event->will_dispatch_callback = base::BindLambdaForTesting(
      [&](content::BrowserContext*, mojom::ContextType, const Extension*,
          const base::DictValue*, std::optional<base::ListValue>&,
          mojom::EventFilteringInfoPtr&, bool*) {
        routed_to_extension = true;
        EXPECT_TRUE(HasLazyListener("test.onEvent"));
        EXPECT_FALSE(GetPersistedEvents().contains("test.onEvent"));
        EXPECT_FALSE(phase_listeners->Has("test.onEvent", /*filter=*/nullptr));
        return false;
      });
  event_router()->DispatchEventWithLazyListener(extension_id(),
                                                std::move(event));

  EXPECT_TRUE(routed_to_extension);
  EXPECT_FALSE(HasLazyListener("test.onEvent"));
  EXPECT_FALSE(GetPersistedEvents().contains("test.onEvent"));
}

class EventRouterDispatchTest : public ExtensionsTest {
 public:
  EventRouterDispatchTest() = default;
  EventRouterDispatchTest(const EventRouterDispatchTest&) = delete;
  EventRouterDispatchTest& operator=(const EventRouterDispatchTest&) = delete;

  void SetUp() override {
    ExtensionsTest::SetUp();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderProcessHost* process() const {
    return render_process_host_.get();
  }
  EventRouter* event_router() { return EventRouter::Get(browser_context()); }

 protected:
  void RegisterTestApiFeature(StaticFeatureData<SimpleFeatureData> data) {
    test_api_feature_.emplace(data);
    std::array<Feature*, 1> features = {&*test_api_feature_};
    provider_.AddStaticFeatures(features);
    api_.RegisterDependencyProvider("api", &provider_);
    api_scope_ =
        std::make_unique<ExtensionAPI::OverrideSharedInstanceForTest>(&api_);
  }

 private:
  std::optional<SimpleFeature> test_api_feature_;
  FeatureProvider provider_;
  ExtensionAPI api_;
  std::unique_ptr<ExtensionAPI::OverrideSharedInstanceForTest> api_scope_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

TEST_F(EventRouterDispatchTest, TestDispatch) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  GURL webui1("chrome-untrusted://one");
  GURL webui2("chrome-untrusted://two");
  static constexpr auto kMatches = std::to_array<std::string_view>(
      {"chrome-untrusted://one/", "chrome-untrusted://two/"});
  static constexpr SimpleFeatureData kFeatureData = {
      .feature = {.name = kTestEventName},
      .config = {.match_patterns = StaticSpan(kMatches)},
  };
  RegisterTestApiFeature(StaticFeatureData(kFeatureData));

  TestEventRouterObserver observer(event_router());
  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetID(id)
            .SetManifest(base::DictValue()
                             .Set("name", "Test app")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2))
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  };
  add_extension(ext1);
  add_extension(ext2);
  auto event = [](std::string name) {
    return std::make_unique<extensions::Event>(extensions::events::FOR_TEST,
                                               name, nullptr);
  };

  // Register both extensions and both URLs for event.
  event_router()->AddEventListener(kTestEventName, process(), ext1);
  event_router()->AddEventListener(kTestEventName, process(), ext2);
  event_router()->AddEventListenerForURL(kTestEventName, process(), webui1);
  event_router()->AddEventListenerForURL(kTestEventName, process(), webui2);

  // Should only dispatch to the single specified extension or url.
  event_router()->DispatchEventToExtension(ext1, event(kTestEventName));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToExtension(ext2, event(kTestEventName));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToURL(webui1, event(kTestEventName));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToURL(webui2, event(kTestEventName));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();

  // No listeners registered for 'api.other' event.
  event_router()->DispatchEventToExtension(ext1, event("api.other"));
  EXPECT_EQ(0u, observer.dispatched_events().size());
  event_router()->DispatchEventToURL(webui1, event("api.other"));
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

// Tests that a dispatch restricted to one of an extension's service workers
// reaches exactly the identified worker.
TEST_F(EventRouterDispatchTest, ActiveDispatchTargetRestrictsToWorker) {
  std::string ext1 = "ext1";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").SetID(ext1).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  // Register two service worker listener contexts of the same extension for
  // the same event, each with its own event dispatcher.
  const int64_t sw_version_id1 = 10;
  const int sw_thread_id1 = 100;
  const int64_t sw_version_id2 = 11;
  const int sw_thread_id2 = 101;
  MockEventDispatcher sw_event_dispatcher1;
  MockEventDispatcher sw_event_dispatcher2;
  auto sw_context1 =
      mojom::ServiceWorkerContext::New(GURL(), sw_version_id1, sw_thread_id1);
  auto sw_context2 =
      mojom::ServiceWorkerContext::New(GURL(), sw_version_id2, sw_thread_id2);
  event_router()->AddServiceWorkerEventListener(ext1, kTestEventName,
                                                *sw_context1, process());
  event_router()->AddServiceWorkerEventListener(ext1, kTestEventName,
                                                *sw_context2, process());
  event_router()->BindServiceWorkerEventDispatcher(
      process()->GetDeprecatedID(), sw_thread_id1,
      sw_event_dispatcher1.BindAndPassRemote());
  event_router()->BindServiceWorkerEventDispatcher(
      process()->GetDeprecatedID(), sw_thread_id2,
      sw_event_dispatcher2.BindAndPassRemote());

  // Creates an event whose dispatch is restricted to the given service worker
  // and that records every target it is delivered to in `dispatched`.
  std::vector<extensions::EventTarget> dispatched;
  auto create_restricted_event = [&](int64_t sw_version_id, int sw_thread_id) {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::FOR_TEST, kTestEventName, nullptr);
    event->restrict_to_dispatch_target =
        Event::DispatchTarget{.render_process_id = process()->GetID(),
                              .worker_thread_id = sw_thread_id,
                              .service_worker_version_id = sw_version_id};
    event->did_dispatch_callback =
        base::BindLambdaForTesting([&](const extensions::EventTarget& target) {
          dispatched.push_back(target);
        });
    return event;
  };

  // Although both workers have a listener registered for the event, an event
  // restricted to the first worker must be delivered to it alone.
  event_router()->DispatchEventToExtension(
      ext1, create_restricted_event(sw_version_id1, sw_thread_id1));
  ASSERT_EQ(1u, dispatched.size());
  EXPECT_EQ((EventTarget{ext1, process()->GetDeprecatedID(), sw_version_id1,
                         sw_thread_id1}),
            dispatched[0]);
  dispatched.clear();

  // Likewise for the second worker.
  event_router()->DispatchEventToExtension(
      ext1, create_restricted_event(sw_version_id2, sw_thread_id2));
  ASSERT_EQ(1u, dispatched.size());
  EXPECT_EQ((EventTarget{ext1, process()->GetDeprecatedID(), sw_version_id2,
                         sw_thread_id2}),
            dispatched[0]);
}

// Tests that a dispatch restricted to an active target with no matching
// listener registration is not delivered anywhere and fires the
// cannot-dispatch callback.
TEST_F(EventRouterDispatchTest,
       ActiveDispatchTargetMissingFiresCannotDispatch) {
  std::string ext1 = "ext1";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  TestEventRouterObserver observer(event_router());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").SetID(ext1).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  // The extension's only listener registration lives in `process()`.
  event_router()->AddEventListener(kTestEventName, process(), ext1);

  // Restrict the dispatch to `other_process`, where no listener is
  // registered.
  auto other_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto event = std::make_unique<extensions::Event>(extensions::events::FOR_TEST,
                                                   kTestEventName, nullptr);
  event->restrict_to_dispatch_target =
      Event::DispatchTarget{.render_process_id = other_process->GetID()};

  // The event must not be handed to the listener in `process()`; instead,
  // `cannot_dispatch_callback` must fire.
  base::RunLoop run_loop;
  event->cannot_dispatch_callback = run_loop.QuitClosure();
  event_router()->DispatchEventToExtension(ext1, std::move(event));
  run_loop.Run();
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

TEST_F(EventRouterDispatchTest, TestDispatchCallback) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  std::string ext3 = "ext3";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test extension")
            .SetID(id)
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  };
  add_extension(ext1);
  add_extension(ext2);
  add_extension(ext3);

  std::vector<extensions::EventTarget> dispatched;
  auto create_event = [&](const std::string& name) {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::FOR_TEST, name, nullptr);
    return event;
  };

  auto create_event_with_callback = [&](const std::string& name) {
    auto e = create_event(name);
    e->did_dispatch_callback =
        base::BindLambdaForTesting([&](const extensions::EventTarget& target) {
          dispatched.push_back(target);
        });
    // To ensure did_dispatch_callback is copied properly.
    return e->Clone();
  };

  auto process1 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto process2 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto process3 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto process4 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  // Register all extensions for the event:
  // 1) single listener for ext1
  event_router()->AddEventListener(kTestEventName, process1.get(), ext1);
  // 2) two listeners for two processes for ext2
  event_router()->AddEventListener(kTestEventName, process2.get(), ext2);
  event_router()->AddEventListener(kTestEventName, process3.get(), ext2);
  // 3) service worker listeners for ext3
  const int sw_version_id = 10;
  const int sw_thread_id = 100;
  MockEventDispatcher sw_event_dispatcher;
  auto sw_context =
      mojom::ServiceWorkerContext::New(GURL(), sw_version_id, sw_thread_id);
  event_router()->AddServiceWorkerEventListener(ext3, kTestEventName,
                                                *sw_context, process4.get());
  event_router()->BindServiceWorkerEventDispatcher(
      process4->GetDeprecatedID(), sw_thread_id,
      sw_event_dispatcher.BindAndPassRemote());

  // Dispatch without callback set.
  event_router()->DispatchEventToExtension(ext1, create_event(kTestEventName));
  event_router()->DispatchEventToExtension(ext2, create_event(kTestEventName));
  event_router()->DispatchEventToExtension(ext3, create_event(kTestEventName));

  EXPECT_EQ(0u, dispatched.size());
  dispatched.clear();

  // Dispatch with a post-dispatch callback set.
  event_router()->DispatchEventToExtension(
      ext1, create_event_with_callback(kTestEventName));
  event_router()->DispatchEventToExtension(
      ext2, create_event_with_callback(kTestEventName));
  event_router()->DispatchEventToExtension(
      ext3, create_event_with_callback(kTestEventName));

  const int sw_invalid_version_id =
      blink::mojom::kInvalidServiceWorkerVersionId;
  std::vector<EventTarget> expected{
      {ext1, process1->GetDeprecatedID(), sw_invalid_version_id, kMainThreadId},
      {ext2, process2->GetDeprecatedID(), sw_invalid_version_id, kMainThreadId},
      {ext2, process3->GetDeprecatedID(), sw_invalid_version_id, kMainThreadId},
      {ext3, process4->GetDeprecatedID(), sw_version_id, sw_thread_id},
  };
  std::sort(std::begin(dispatched), std::end(dispatched));
  EXPECT_EQ(dispatched, expected);
  dispatched.clear();

  // Repeat the same event, but with broadcast: should have the same dispatch
  // targets.
  event_router()->BroadcastEvent(create_event_with_callback(kTestEventName));

  std::sort(std::begin(dispatched), std::end(dispatched));
  EXPECT_EQ(dispatched, expected);
  dispatched.clear();

  // No listeners registered for 'api.other' event.
  event_router()->DispatchEventToExtension(
      ext1, create_event_with_callback("api.other"));
  event_router()->DispatchEventToExtension(
      ext2, create_event_with_callback("api.other"));
  event_router()->DispatchEventToExtension(
      ext3, create_event_with_callback("api.other"));
  EXPECT_EQ(0u, dispatched.size());
}

TEST_F(EventRouterDispatchTest, TestDispatchCallback_NoListeners) {
  std::string ext1 = "ext1";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").SetID(ext1).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  TestEventRouterObserver observer(event_router());

  // A dispatch restricted to `ext1` should still trigger the callback when
  // EventRouter has no listener to receive the event.
  auto event = std::make_unique<extensions::Event>(extensions::events::FOR_TEST,
                                                   kTestEventName, nullptr);
  base::RunLoop run_loop;
  bool callback_ran = false;
  event->cannot_dispatch_callback = base::BindLambdaForTesting([&]() {
    callback_ran = true;
    run_loop.Quit();
  });

  event_router()->DispatchEventToExtension(ext1, std::move(event));
  run_loop.Run();

  EXPECT_TRUE(callback_ran);
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

TEST_F(EventRouterDispatchTest, TestDispatchCallback_OtherExtensionListener) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test extension").SetID(id).Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  };
  add_extension(ext1);
  add_extension(ext2);

  TestEventRouterObserver observer(event_router());
  // A listener for the same event name owned by `ext2` should not suppress the
  // callback for a dispatch restricted to `ext1`.
  event_router()->AddFilteredEventListener(
      kTestEventName, process(),
      mojom::EventListenerOwner::NewExtensionId(ext2),
      /*service_worker_context=*/nullptr, base::DictValue(),
      /*add_lazy_listener=*/false);

  auto event = std::make_unique<extensions::Event>(extensions::events::FOR_TEST,
                                                   kTestEventName, nullptr);
  base::RunLoop run_loop;
  bool callback_ran = false;
  event->cannot_dispatch_callback = base::BindLambdaForTesting([&]() {
    callback_ran = true;
    run_loop.Quit();
  });

  event_router()->DispatchEventToExtension(ext1, std::move(event));
  run_loop.Run();

  EXPECT_TRUE(callback_ran);
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

TEST_F(EventRouterDispatchTest, CopySelectivelyAndClone_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kShareEventArgsOnDispatch);

  auto args = base::MakeRefCounted<EventArgs>();
  args->data.Append("test_arg");

  auto info = mojom::EventFilteringInfo::New();
  info->instance_id = 42;

  auto event = std::make_unique<Event>(
      events::FOR_TEST, "test.event", std::move(args), browser_context(),
      std::nullopt, GURL(), EventRouter::UserGestureState::kUnknown,
      std::move(info));
  ASSERT_TRUE(event->args_ptr());
  ASSERT_TRUE(event->filter_info);

  // Verify Clone shares ref-counted args_ptr, while filter_info is
  // value-cloned (different pointer addresses, equal values).
  auto cloned = event->Clone();
  EXPECT_EQ(event->args_ptr(), cloned->args_ptr());
  EXPECT_EQ(event->args(), cloned->args());
  ASSERT_TRUE(cloned->filter_info);
  EXPECT_NE(event->filter_info.get(), cloned->filter_info.get());
  EXPECT_EQ(*event->filter_info, *cloned->filter_info);

  // Verify CopySelectively with default arguments (no modifications) behaves
  // like Clone: shares ref-counted args_ptr, clones filter_info.
  auto default_copied = event->CopySelectively();
  EXPECT_EQ(event->args_ptr(), default_copied->args_ptr());
  EXPECT_EQ(event->args(), default_copied->args());
  ASSERT_TRUE(default_copied->filter_info);
  EXPECT_NE(event->filter_info.get(), default_copied->filter_info.get());
  EXPECT_EQ(*event->filter_info, *default_copied->filter_info);

  // Verify CopySelectively with modified_args replaces the arguments reference
  // while cloning filter_info.
  base::ListValue modified_args;
  modified_args.Append("modified_arg");
  auto args_copied = event->CopySelectively(std::move(modified_args));
  EXPECT_NE(event->args_ptr(), args_copied->args_ptr());
  ASSERT_EQ(1u, args_copied->args().size());
  EXPECT_EQ("modified_arg", args_copied->args()[0].GetString());
  ASSERT_TRUE(args_copied->filter_info);
  EXPECT_NE(event->filter_info.get(), args_copied->filter_info.get());
  EXPECT_EQ(*event->filter_info, *args_copied->filter_info);

  // Verify CopySelectively with modified_filter_info replaces filter_info
  // while sharing the arguments reference.
  auto modified_info = mojom::EventFilteringInfo::New();
  modified_info->instance_id = 99;
  auto filter_copied = event->CopySelectively(
      /*modified_event_args=*/std::nullopt, std::move(modified_info));
  EXPECT_EQ(event->args_ptr(), filter_copied->args_ptr());
  EXPECT_EQ(event->args(), filter_copied->args());
  ASSERT_TRUE(filter_copied->filter_info);
  EXPECT_EQ(99, filter_copied->filter_info->instance_id);

  auto event_args = base::MakeRefCounted<EventArgs>();
  // Verify empty event args and default empty filter_info.
  auto empty_event1 = std::make_unique<Event>(events::FOR_TEST, "test.event",
                                              std::move(event_args));
  auto empty_event2 = std::make_unique<Event>(events::FOR_TEST, "test.event",
                                              scoped_refptr<const EventArgs>());
  EXPECT_TRUE(empty_event1->args().empty());
  ASSERT_TRUE(empty_event1->filter_info);
  EXPECT_EQ(*mojom::EventFilteringInfo::New(), *empty_event1->filter_info);
  EXPECT_TRUE(empty_event2->args().empty());
  ASSERT_TRUE(empty_event2->filter_info);
  EXPECT_EQ(*mojom::EventFilteringInfo::New(), *empty_event2->filter_info);
}

TEST_F(EventRouterDispatchTest, CopySelectivelyAndClone_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kShareEventArgsOnDispatch);

  auto args = base::MakeRefCounted<EventArgs>();
  args->data.Append("test_arg");

  auto info = mojom::EventFilteringInfo::New();
  info->instance_id = 42;

  auto event = std::make_unique<Event>(
      events::FOR_TEST, "test.event", std::move(args), browser_context(),
      std::nullopt, GURL(), EventRouter::UserGestureState::kUnknown,
      std::move(info));
  ASSERT_TRUE(event->args_ptr());
  ASSERT_TRUE(event->filter_info);

  // Verify Clone creates deep copies of args (distinct pointers, equal
  // values) and filter_info is value-cloned (distinct pointer, equal values).
  auto cloned = event->Clone();
  EXPECT_NE(event->args_ptr(), cloned->args_ptr());
  EXPECT_EQ(event->args(), cloned->args());
  ASSERT_TRUE(cloned->filter_info);
  EXPECT_NE(event->filter_info.get(), cloned->filter_info.get());
  EXPECT_EQ(*event->filter_info, *cloned->filter_info);

  // Verify CopySelectively with default arguments (no modifications) creates
  // deep copies when sharing is disabled.
  auto default_copied = event->CopySelectively();
  EXPECT_NE(event->args_ptr(), default_copied->args_ptr());
  EXPECT_EQ(event->args(), default_copied->args());
  ASSERT_TRUE(default_copied->filter_info);
  EXPECT_NE(event->filter_info.get(), default_copied->filter_info.get());
  EXPECT_EQ(*event->filter_info, *default_copied->filter_info);

  // Verify CopySelectively with modified_args replaces the arguments.
  base::ListValue modified_args;
  modified_args.Append("modified_arg");
  auto args_copied = event->CopySelectively(std::move(modified_args));
  EXPECT_NE(event->args_ptr(), args_copied->args_ptr());
  ASSERT_EQ(1u, args_copied->args().size());
  EXPECT_EQ("modified_arg", args_copied->args()[0].GetString());
  ASSERT_TRUE(args_copied->filter_info);
  EXPECT_NE(event->filter_info.get(), args_copied->filter_info.get());
  EXPECT_EQ(*event->filter_info, *args_copied->filter_info);

  // Verify CopySelectively with modified_filter_info replaces filter_info
  // and deep-copies args.
  auto modified_info = mojom::EventFilteringInfo::New();
  modified_info->instance_id = 99;
  auto filter_copied = event->CopySelectively(
      /*modified_event_args=*/std::nullopt, std::move(modified_info));
  EXPECT_NE(event->args_ptr(), filter_copied->args_ptr());
  EXPECT_EQ(event->args(), filter_copied->args());
  ASSERT_TRUE(filter_copied->filter_info);
  EXPECT_EQ(99, filter_copied->filter_info->instance_id);
}

TEST_F(EventRouterDispatchTest,
       BroadcastEventSharesArgumentsAcrossDispatchedEvents) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test extension").SetID(id).Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  };
  add_extension(ext1);
  add_extension(ext2);

  auto process1 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto process2 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  event_router()->AddFilteredEventListener(
      kTestEventName, process1.get(),
      mojom::EventListenerOwner::NewExtensionId(ext1),
      /*service_worker_context=*/nullptr, base::DictValue(),
      /*add_lazy_listener=*/false);
  event_router()->AddFilteredEventListener(
      kTestEventName, process2.get(),
      mojom::EventListenerOwner::NewExtensionId(ext2),
      /*service_worker_context=*/nullptr, base::DictValue(),
      /*add_lazy_listener=*/false);

  // 1. When kShareEventArgsOnDispatch is ENABLED: pointer equality.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        extensions_features::kShareEventArgsOnDispatch);

    TestEventRouterObserver observer(event_router());

    auto broadcast_args = base::MakeRefCounted<EventArgs>();
    broadcast_args->data.Append("broadcast_payload");
    auto broadcast_event = std::make_unique<Event>(
        events::FOR_TEST, kTestEventName, std::move(broadcast_args));
    scoped_refptr<const EventArgs> original_args_ptr =
        broadcast_event->args_ptr();

    event_router()->BroadcastEvent(std::move(broadcast_event));

    ASSERT_EQ(2u, observer.all_dispatched_events().size());
    EXPECT_EQ(original_args_ptr,
              observer.all_dispatched_events()[0]->args_ptr());
    EXPECT_EQ(original_args_ptr,
              observer.all_dispatched_events()[1]->args_ptr());
  }

  // 2. When kShareEventArgsOnDispatch is DISABLED: distinct pointers, equal
  // values.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        extensions_features::kShareEventArgsOnDispatch);

    TestEventRouterObserver observer(event_router());

    auto broadcast_args = base::MakeRefCounted<EventArgs>();
    broadcast_args->data.Append("broadcast_payload");
    auto broadcast_event = std::make_unique<Event>(
        events::FOR_TEST, kTestEventName, std::move(broadcast_args));
    scoped_refptr<const EventArgs> original_args_ptr =
        broadcast_event->args_ptr();
    base::ListValue original_args_copy = original_args_ptr->data.Clone();

    event_router()->BroadcastEvent(std::move(broadcast_event));

    ASSERT_EQ(2u, observer.all_dispatched_events().size());
    EXPECT_NE(original_args_ptr,
              observer.all_dispatched_events()[0]->args_ptr());
    EXPECT_NE(original_args_ptr,
              observer.all_dispatched_events()[1]->args_ptr());
    EXPECT_NE(observer.all_dispatched_events()[0]->args_ptr(),
              observer.all_dispatched_events()[1]->args_ptr());
    EXPECT_EQ(original_args_copy, observer.all_dispatched_events()[0]->args());
    EXPECT_EQ(original_args_copy, observer.all_dispatched_events()[1]->args());
  }
}

TEST_F(EventRouterDispatchTest,
       WillDispatchCallbackModifiesArgsReferenceReplacement) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test extension").SetID(id).Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  };
  add_extension(ext1);
  add_extension(ext2);

  auto process1 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  auto process2 =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  event_router()->AddFilteredEventListener(
      kTestEventName, process1.get(),
      mojom::EventListenerOwner::NewExtensionId(ext1),
      /*service_worker_context=*/nullptr, base::DictValue(),
      /*add_lazy_listener=*/false);
  event_router()->AddFilteredEventListener(
      kTestEventName, process2.get(),
      mojom::EventListenerOwner::NewExtensionId(ext2),
      /*service_worker_context=*/nullptr, base::DictValue(),
      /*add_lazy_listener=*/false);

  // 1. When kShareEventArgsOnDispatch is ENABLED:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        extensions_features::kShareEventArgsOnDispatch);

    TestEventRouterObserver observer(event_router());

    auto event_args = base::MakeRefCounted<EventArgs>();
    event_args->data.Append("original_val");
    auto broadcast_event = std::make_unique<Event>(
        events::FOR_TEST, kTestEventName, std::move(event_args));
    scoped_refptr<const EventArgs> original_args_ptr =
        broadcast_event->args_ptr();

    // Set will_dispatch_callback to modify arguments for ext1 only.
    broadcast_event->will_dispatch_callback = base::BindRepeating(
        [](content::BrowserContext* context,
           mojom::ContextType target_context_type, const Extension* extension,
           const base::DictValue* listener_filter,
           std::optional<base::ListValue>& event_args_out,
           mojom::EventFilteringInfoPtr& event_filtering_info_out,
           bool* dispatch_separate_event_out) {
          if (extension && extension->id() == "ext1") {
            base::ListValue modified;
            modified.Append("modified_val_for_ext1");
            event_args_out = std::move(modified);
          }
          return true;
        });

    event_router()->BroadcastEvent(std::move(broadcast_event));

    ASSERT_EQ(2u, observer.all_dispatched_events().size());

    const Event* dispatched_ext1 = nullptr;
    const Event* dispatched_ext2 = nullptr;

    for (const auto& dispatched : observer.all_dispatched_events()) {
      if (!dispatched->args().empty() && dispatched->args()[0].is_string() &&
          dispatched->args()[0].GetString() == "modified_val_for_ext1") {
        dispatched_ext1 = dispatched.get();
      } else {
        dispatched_ext2 = dispatched.get();
      }
    }

    ASSERT_TRUE(dispatched_ext1);
    ASSERT_TRUE(dispatched_ext2);

    // The modified listener received a new reference.
    EXPECT_NE(original_args_ptr, dispatched_ext1->args_ptr());
    EXPECT_EQ("modified_val_for_ext1", dispatched_ext1->args()[0].GetString());

    // The unmodified listener retained the original reference.
    EXPECT_EQ(original_args_ptr, dispatched_ext2->args_ptr());
    EXPECT_EQ("original_val", dispatched_ext2->args()[0].GetString());
  }

  // 2. When kShareEventArgsOnDispatch is DISABLED:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        extensions_features::kShareEventArgsOnDispatch);

    TestEventRouterObserver observer(event_router());

    auto event_args = base::MakeRefCounted<EventArgs>();
    event_args->data.Append("original_val");
    auto broadcast_event = std::make_unique<Event>(
        events::FOR_TEST, kTestEventName, std::move(event_args));
    scoped_refptr<const EventArgs> original_args_ptr =
        broadcast_event->args_ptr();

    // Set will_dispatch_callback to modify arguments for ext1 only.
    broadcast_event->will_dispatch_callback = base::BindRepeating(
        [](content::BrowserContext* context,
           mojom::ContextType target_context_type, const Extension* extension,
           const base::DictValue* listener_filter,
           std::optional<base::ListValue>& event_args_out,
           mojom::EventFilteringInfoPtr& event_filtering_info_out,
           bool* dispatch_separate_event_out) {
          if (extension && extension->id() == "ext1") {
            base::ListValue modified;
            modified.Append("modified_val_for_ext1");
            event_args_out = std::move(modified);
          }
          return true;
        });

    event_router()->BroadcastEvent(std::move(broadcast_event));

    ASSERT_EQ(2u, observer.all_dispatched_events().size());

    const Event* dispatched_ext1 = nullptr;
    const Event* dispatched_ext2 = nullptr;

    for (const auto& dispatched : observer.all_dispatched_events()) {
      if (!dispatched->args().empty() && dispatched->args()[0].is_string() &&
          dispatched->args()[0].GetString() == "modified_val_for_ext1") {
        dispatched_ext1 = dispatched.get();
      } else {
        dispatched_ext2 = dispatched.get();
      }
    }

    ASSERT_TRUE(dispatched_ext1);
    ASSERT_TRUE(dispatched_ext2);

    // The modified listener received a new reference with modified value.
    EXPECT_NE(original_args_ptr, dispatched_ext1->args_ptr());
    EXPECT_EQ("modified_val_for_ext1", dispatched_ext1->args()[0].GetString());

    // The unmodified listener received a deep copy (distinct reference, same
    // value).
    EXPECT_NE(original_args_ptr, dispatched_ext2->args_ptr());
    EXPECT_EQ("original_val", dispatched_ext2->args()[0].GetString());
  }
}

TEST_F(EventRouterDispatchTest, DispatchedEventPreservesFilterInfo) {
  std::string ext1 = "ext1";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").SetID(ext1).Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  TestEventRouterObserver observer(event_router());
  event_router()->AddEventListenerForTesting(kTestEventName, process(), ext1);

  auto info = mojom::EventFilteringInfo::New();
  info->url = GURL("https://example.com/test");
  info->service_type = "test_service";
  info->instance_id = 42;

  auto event = std::make_unique<extensions::Event>(
      events::FOR_TEST, kTestEventName, nullptr, browser_context(),
      std::nullopt, GURL(), EventRouter::UserGestureState::kEnabled,
      std::move(info));

  event_router()->DispatchEventToExtension(ext1, std::move(event));

  ASSERT_EQ(1u, observer.dispatched_events().size());
  ASSERT_TRUE(observer.dispatched_events().contains(kTestEventName));
  const Event* dispatched_event =
      observer.dispatched_events().at(kTestEventName).get();
  ASSERT_TRUE(dispatched_event);
  ASSERT_TRUE(dispatched_event->filter_info);
  EXPECT_EQ(GURL("https://example.com/test"),
            dispatched_event->filter_info->url);
  EXPECT_EQ("test_service", dispatched_event->filter_info->service_type);
  EXPECT_EQ(42, dispatched_event->filter_info->instance_id);
  EXPECT_EQ(EventRouter::UserGestureState::kEnabled,
            dispatched_event->user_gesture);
}

// Tests that an event scoped to a URL isn't dispatched for hosts blocked by
// enterprise policy, even if the extension has host permissions for them.
// Regression test for https://crbug.com/517094892.
TEST_F(EventRouterDispatchTest, EventURLRespectsPolicyBlockedHosts) {
  const ExtensionId ext1 = "ext1";
  RegisterTestApiFeature(StaticFeatureData(kTestEventFeatureData));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension")
          .SetID(ext1)
          .AddHostPermission("<all_urls>")
          .Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  URLPatternSet policy_blocked_hosts;
  policy_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "*://blocked.example/*"));
  extension->permissions_data()->SetPolicyHostRestrictions(
      policy_blocked_hosts, /*policy_allowed_hosts=*/URLPatternSet());

  TestEventRouterObserver observer(event_router());
  event_router()->AddEventListenerForTesting(kTestEventName, process(), ext1);

  auto create_event = [&](const char* url) {
    auto event = std::make_unique<Event>(events::FOR_TEST, kTestEventName,
                                         nullptr, browser_context());
    event->event_url = GURL(url);
    return event;
  };

  event_router()->BroadcastEvent(create_event("http://blocked.example/"));
  EXPECT_EQ(0u, observer.dispatched_events().size());

  event_router()->BroadcastEvent(create_event("http://allowed.example/"));
  EXPECT_EQ(1u, observer.dispatched_events().size());
}

}  // namespace extensions
