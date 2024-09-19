// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-blink-forward.h"

using base::Value;

namespace extensions {

namespace {

// A simple mock to keep track of listener additions and removals.
class MockEventRouterObserver : public EventRouter::Observer {
 public:
  MockEventRouterObserver()
      : listener_added_count_(0),
        listener_removed_count_(0) {}

  MockEventRouterObserver(const MockEventRouterObserver&) = delete;
  MockEventRouterObserver& operator=(const MockEventRouterObserver&) = delete;

  ~MockEventRouterObserver() override {}

  int listener_added_count() const { return listener_added_count_; }
  int listener_removed_count() const { return listener_removed_count_; }
  const std::string& last_event_name() const { return last_event_name_; }

  void Reset() {
    listener_added_count_ = 0;
    listener_removed_count_ = 0;
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

 private:
  int listener_added_count_;
  int listener_removed_count_;
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
                     base::Value::List event_args,
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
        base::Value::Dict /* filter */)>;

std::unique_ptr<EventListener> CreateEventListenerForExtension(
    const ExtensionId& extension_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::Value::Dict filter) {
  return EventListener::ForExtension(event_name, extension_id, process,
                                     std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForURL(
    const GURL& listener_url,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::Value::Dict filter) {
  return EventListener::ForURL(event_name, listener_url, process,
                               std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForExtensionServiceWorker(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::Value::Dict filter) {
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
  auto manifest = base::Value::Dict()
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
  auto manifest = base::Value::Dict()
                      .Set("name", "foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2);
  manifest.SetByDottedPath("background.service_worker", "worker.js");
  builder.SetManifest(std::move(manifest));
  return builder.Build();
}

base::Value::Dict CreateHostSuffixFilter(const std::string& suffix) {
  base::Value::Dict filter_dict;
  filter_dict.Set("hostSuffix", Value(suffix));

  base::Value::List filter_list;
  filter_list.Append(std::move(filter_dict));

  base::Value::Dict filter;
  filter.Set("url", std::move(filter_list));
  return filter;
}

}  // namespace

bool operator<(const EventTarget& e1, const EventTarget& e2) {
  return std::tie(e1.extension_id, e1.render_process_id,
                  e1.service_worker_version_id, e1.worker_thread_id) <
         std::tie(e2.extension_id, e2.render_process_id,
                  e2.service_worker_version_id, e2.worker_thread_id);
}

bool operator==(const EventTarget& e1, const EventTarget& e2) {
  return std::tie(e1.extension_id, e1.render_process_id,
                  e1.service_worker_version_id, e1.worker_thread_id) ==
         std::tie(e2.extension_id, e2.render_process_id,
                  e2.service_worker_version_id, e2.worker_thread_id);
}

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
    ASSERT_TRUE(event_router());  // constructs EventRouter
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

  EventRouter* event_router() { return EventRouter::Get(browser_context()); }

  const base::Value::Dict* GetFilteredEvents(const ExtensionId& extension_id) {
    return event_router()->GetFilteredEvents(
        extension_id, is_for_service_worker()
                          ? EventRouter::RegisteredEventType::kServiceWorker
                          : EventRouter::RegisteredEventType::kLazy);
  }

  bool ContainsFilter(const ExtensionId& extension_id,
                      const std::string& event_name,
                      const base::Value::Dict& to_check) {
    const base::Value::List* filter_list =
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
  const base::Value::List* GetFilterList(const ExtensionId& extension_id,
                                         const std::string& event_name) {
    const base::Value::Dict* filtered_events = GetFilteredEvents(extension_id);
    const auto iter = filtered_events->begin();
    if (iter->first != event_name) {
      return nullptr;
    }

    return iter->second.is_list() ? &iter->second.GetList() : nullptr;
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
  EventRouter router(nullptr, nullptr);
  std::unique_ptr<EventListener> listener =
      constructor.Run("event_name", render_process_host(), base::Value::Dict());

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
  std::unique_ptr<EventListener> sub_event_listener = constructor.Run(
      "event_name/1", render_process_host(), base::Value::Dict());
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
      // Dummy version_id and thread_id.
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

  mojom::ContextType GetMostLikelyContextType(const Extension* extension,
                                              int process_id,
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
  std::string event_name = "testapi.onEvent";
  FeatureProvider provider;
  auto feature = std::make_unique<SimpleFeature>();
  feature->set_name("test feature");
  feature->set_matches({"chrome://settings/*"});
  provider.AddFeature(event_name, std::move(feature));

  ExtensionAPI api;
  api.RegisterDependencyProvider("api", &provider);
  ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

  EventRouter router(browser_context(), nullptr);
  content::MockRenderProcessHost regular_rph(browser_context());
  content::MockRenderProcessHost otr_rph(incognito_context());

  // Add event listeners, as if we had created two real WebUIs, one in a regular
  // profile and one in an otr profile. Note that the string chrome://settings
  // is hardcoded into the api permissions of settingsPrivate.
  GURL dummy_url("chrome://settings/test");
  router.AddEventListenerForURL(event_name, &regular_rph, dummy_url);
  router.AddEventListenerForURL(event_name, &otr_rph, dummy_url);

  // Hook up some test observers
  EventRouterObserver regular_counter(regular_rph.GetID());
  router.AddObserverForTesting(&regular_counter);
  EventRouterObserver otr_counter(otr_rph.GetID());
  router.AddObserverForTesting(&otr_counter);

  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(0, otr_counter.dispatch_count);

  // Sending an otr event should not trigger the regular observer.
  auto otr_event =
      std::make_unique<Event>(extensions::events::FOR_TEST, event_name,
                              base::Value::List(), incognito_context());
  router.BroadcastEvent(std::move(otr_event));
  EXPECT_EQ(0, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);

  // Setting a regular event should not trigger the otr observer.
  std::unique_ptr<Event> regular_event =
      std::make_unique<Event>(extensions::events::FOR_TEST, event_name,
                              base::Value::List(), browser_context());
  router.BroadcastEvent(std::move(regular_event));
  EXPECT_EQ(1, regular_counter.dispatch_count);
  EXPECT_EQ(1, otr_counter.dispatch_count);
}

TEST_F(EventRouterTest, MultipleEventRouterObserver) {
  EventRouter router(nullptr, nullptr);
  std::unique_ptr<EventListener> listener =
      EventListener::ForURL("event_name", GURL("http://google.com/path"),
                            render_process_host(), base::Value::Dict());

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

// Tests adding and removing events with filters.
// TODO(crbug.com/40281129): test is flaky across platforms.
TEST_P(EventRouterFilterTest, DISABLED_Basic) {
  // For the purpose of this test, "." is important in |event_name| as it
  // exercises the code path that uses |event_name| as a key in
  // base::Value::Dict.
  const std::string kEventName = "webNavigation.onBeforeNavigate";

  const std::string kExtensionId = "mbflcebpggnecokmikipoihdbecnjfoj";
  auto param = mojom::EventListenerOwner::NewExtensionId(kExtensionId);
  const std::string kHostSuffixes[] = {"foo.com", "bar.com", "baz.com"};

  std::unique_ptr<mojom::ServiceWorkerContext> worker_context;
  if (is_for_service_worker()) {
    worker_context = std::make_unique<mojom::ServiceWorkerContext>(
        Extension::GetBaseURLFromExtensionId(kExtensionId),
        99,    // Dummy version_id.
        199);  // Dummy thread_id.
  }
  std::vector<base::Value::Dict> filters;
  for (const auto& host_suffix : kHostSuffixes) {
    base::Value::Dict filter = CreateHostSuffixFilter(host_suffix);
    event_router()->AddFilteredEventListener(
        kEventName, render_process_host(), param.Clone(), worker_context.get(),
        filter, true);
    filters.push_back(std::move(filter));
  }

  const base::Value::Dict* filtered_events = GetFilteredEvents(kExtensionId);
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

  // Remove the third filter.
  event_router()->RemoveFilteredEventListener(
      kEventName, render_process_host(), param.Clone(), worker_context.get(),
      filters[2], true);
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[0]));
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[1]));
  ASSERT_FALSE(ContainsFilter(kExtensionId, kEventName, filters[2]));
}

// TODO(crbug.com/40281129): test is flaky across platforms.
TEST_P(EventRouterFilterTest, DISABLED_URLBasedFilteredEventListener) {
  const std::string kEventName = "windows.onRemoved";
  const GURL kUrl("chrome-untrusted://terminal");
  base::Value::Dict filter;
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

class EventRouterDispatchTest : public ExtensionsTest {
 public:
  EventRouterDispatchTest() = default;
  EventRouterDispatchTest(const EventRouterDispatchTest&) = delete;
  EventRouterDispatchTest& operator=(const EventRouterDispatchTest&) = delete;

  void SetUp() override {
    ExtensionsTest::SetUp();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    ASSERT_TRUE(event_router());  // constructs EventRouter
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderProcessHost* process() const {
    return render_process_host_.get();
  }
  EventRouter* event_router() { return EventRouter::Get(browser_context()); }

 private:
  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

TEST_F(EventRouterDispatchTest, TestDispatch) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  GURL webui1("chrome-untrusted://one");
  GURL webui2("chrome-untrusted://two");
  std::string event_name = "testapi.onEvent";
  FeatureProvider provider;
  auto feature = std::make_unique<SimpleFeature>();
  feature->set_name("test feature");
  feature->set_matches({webui1.spec().c_str(), webui2.spec().c_str()});
  provider.AddFeature(event_name, std::move(feature));

  ExtensionAPI api;
  api.RegisterDependencyProvider("api", &provider);
  ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

  TestEventRouterObserver observer(event_router());
  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetID(id)
            .SetManifest(base::Value::Dict()
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
                                               name, base::Value::List());
  };

  // Register both extensions and both URLs for event.
  event_router()->AddEventListener(event_name, process(), ext1);
  event_router()->AddEventListener(event_name, process(), ext2);
  event_router()->AddEventListenerForURL(event_name, process(), webui1);
  event_router()->AddEventListenerForURL(event_name, process(), webui2);

  // Should only dispatch to the single specified extension or url.
  event_router()->DispatchEventToExtension(ext1, event(event_name));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToExtension(ext2, event(event_name));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToURL(webui1, event(event_name));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();
  event_router()->DispatchEventToURL(webui2, event(event_name));
  EXPECT_EQ(1u, observer.dispatched_events().size());
  observer.ClearEvents();

  // No listeners registered for 'api.other' event.
  event_router()->DispatchEventToExtension(ext1, event("api.other"));
  EXPECT_EQ(0u, observer.dispatched_events().size());
  event_router()->DispatchEventToURL(webui1, event("api.other"));
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

// TODO(crbug.com/40281129): test is flaky across platforms.
TEST_F(EventRouterDispatchTest, DISABLED_TestDispatchCallback) {
  std::string ext1 = "ext1";
  std::string ext2 = "ext2";
  std::string ext3 = "ext3";
  std::string event_name = "testapi.onEvent";
  FeatureProvider provider;
  auto feature = std::make_unique<SimpleFeature>();
  feature->set_name("test feature");
  provider.AddFeature(event_name, std::move(feature));

  ExtensionAPI api;
  api.RegisterDependencyProvider("api", &provider);
  ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

  auto add_extension = [&](const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test extension")
            .SetManifestVersion(3)
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
        extensions::events::FOR_TEST, name, base::Value::List());
    return event;
  };

  auto create_event_with_callback = [&](const std::string& name) {
    auto e = create_event(name);
    e->did_dispatch_callback =
        base::BindLambdaForTesting([&](const extensions::EventTarget& target) {
          dispatched.push_back(target);
        });
    // To ensure did_dispatch_callback is copied properly.
    return e->DeepCopy();
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
  event_router()->AddEventListener(event_name, process1.get(), ext1);
  // 2) two listeners for two processes for ext2
  event_router()->AddEventListener(event_name, process2.get(), ext2);
  event_router()->AddEventListener(event_name, process3.get(), ext2);
  // 3) service worker listeners for ext3
  const int sw_version_id = 10;
  const int sw_thread_id = 100;
  MockEventDispatcher sw_event_dispatcher;
  event_router()->AddServiceWorkerEventListener(
      mojom::EventListener::New(
          mojom::EventListenerOwner::NewExtensionId(ext3), event_name,
          mojom::ServiceWorkerContext::New(GURL(), sw_version_id, sw_thread_id),
          /*event_filter=*/std::nullopt),
      process4.get());
  event_router()->BindServiceWorkerEventDispatcher(
      process4->GetID(), sw_thread_id, sw_event_dispatcher.BindAndPassRemote());

  // Dispatch without callback set.
  event_router()->DispatchEventToExtension(ext1, create_event(event_name));
  event_router()->DispatchEventToExtension(ext2, create_event(event_name));
  event_router()->DispatchEventToExtension(ext3, create_event(event_name));

  EXPECT_EQ(0u, dispatched.size());
  dispatched.clear();

  // Dispatch with a post-dispatch callback set.
  event_router()->DispatchEventToExtension(
      ext1, create_event_with_callback(event_name));
  event_router()->DispatchEventToExtension(
      ext2, create_event_with_callback(event_name));
  event_router()->DispatchEventToExtension(
      ext3, create_event_with_callback(event_name));

  const int sw_invalid_version_id =
      blink::mojom::kInvalidServiceWorkerVersionId;
  std::vector<EventTarget> expected{
      {ext1, process1->GetID(), sw_invalid_version_id, kMainThreadId},
      {ext2, process2->GetID(), sw_invalid_version_id, kMainThreadId},
      {ext2, process3->GetID(), sw_invalid_version_id, kMainThreadId},
      {ext3, process4->GetID(), sw_version_id, sw_thread_id},
  };
  std::sort(std::begin(dispatched), std::end(dispatched));
  EXPECT_EQ(dispatched, expected);
  dispatched.clear();

  // Repeat the same event, but with broadcast: should have the same dispatch
  // targets.
  event_router()->BroadcastEvent(create_event_with_callback(event_name));

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

}  // namespace extensions
