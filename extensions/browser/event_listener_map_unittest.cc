// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kExt1Id[] = "extension_1";
const char kExt2Id[] = "extension_2";
const char kEvent1Name[] = "event1";
const char kEvent2Name[] = "event2";
const char kURL[] = "https://google.com/some/url";

// Returns appropriate worker version id for tests.
int64_t GetWorkerVersionId(bool lazy) {
  static constexpr int64_t kDummyServiceWorkerVersionId = 199;
  return lazy ? blink::mojom::kInvalidServiceWorkerVersionId
              : kDummyServiceWorkerVersionId;
}

// Returns appropriate worker thread id for tests.
int GetWorkerThreadId(bool lazy) {
  static constexpr int kDummyServiceWorkerThreadId = 99;
  return lazy ? kMainThreadId : kDummyServiceWorkerThreadId;
}

using EventListenerConstructor =
    base::RepeatingCallback<std::unique_ptr<EventListener>(
        const std::string& /* event_name */,
        content::RenderProcessHost* /* process */,
        std::optional<base::Value::Dict> /* filter */)>;

class EmptyDelegate : public EventListenerMap::Delegate {
  void OnListenerAdded(const EventListener* listener) override {}
  void OnListenerRemoved(const EventListener* listener) override {}
};

class EventListenerMapTest : public ExtensionsTest {
 public:
  EventListenerMapTest()
      : delegate_(std::make_unique<EmptyDelegate>()),
        listeners_(std::make_unique<EventListenerMap>(delegate_.get())) {}

  // testing::Test overrides:
  void SetUp() override {
    ExtensionsTest::SetUp();

    process_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
  }

  void TearDown() override {
    process_->Cleanup();
    process_ = nullptr;

    ExtensionsTest::TearDown();
  }

  base::Value::Dict CreateHostSuffixFilter(const std::string& suffix) {
    base::Value::Dict filter_dict;
    filter_dict.Set("hostSuffix", suffix);

    base::Value::List filter_list;
    filter_list.Append(std::move(filter_dict));

    base::Value::Dict filter;
    filter.Set("url", base::Value(std::move(filter_list)));
    return filter;
  }

  std::unique_ptr<Event> CreateNamedEvent(const std::string& event_name) {
    return CreateEvent(event_name, GURL());
  }

  std::unique_ptr<Event> CreateEvent(const std::string& event_name,
                                     const GURL& url) {
    mojom::EventFilteringInfoPtr info = mojom::EventFilteringInfo::New();
    info->url = url;
    return std::make_unique<Event>(
        events::FOR_TEST, event_name, base::Value::List(), nullptr,
        /*restrict_to_context_type=*/std::nullopt, GURL(),
        EventRouter::USER_GESTURE_UNKNOWN, std::move(info));
  }

  std::unique_ptr<EventListener> CreateLazyListener(
      const std::string& event_name,
      const ExtensionId& extension_id,
      std::optional<base::Value::Dict> filter,
      bool is_for_service_worker) {
    return EventListener::CreateLazyListener(
        event_name, extension_id, browser_context(), is_for_service_worker,
        is_for_service_worker
            ? Extension::GetBaseURLFromExtensionId(extension_id)
            : GURL(),
        std::move(filter));
  }

 protected:
  void TestUnfilteredEventsGoToAllListeners(
      const EventListenerConstructor& constructor);
  void TestRemovingByProcess(const EventListenerConstructor& constructor);
  void TestRemovingByListener(const EventListenerConstructor& constructor);
  void TestAddExistingUnfilteredListener(
      const EventListenerConstructor& constructor);
  void TestHasListenerForEvent(const EventListenerConstructor& constructor);

  std::unique_ptr<EventListenerMap::Delegate> delegate_;
  std::unique_ptr<EventListenerMap> listeners_;
  std::unique_ptr<content::RenderProcessHost> process_;
};

// Parameterized version of test to run with |is_for_service_worker|.
class EventListenerMapWithContextTest
    : public EventListenerMapTest,
      public testing::WithParamInterface<bool /* is_for_service_worker */> {};

std::unique_ptr<EventListener> CreateEventListenerForExtension(
    const ExtensionId& extension_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    std::optional<base::Value::Dict> filter) {
  return EventListener::ForExtension(event_name, extension_id, process,
                                     std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForURL(
    const GURL& listener_url,
    const std::string& event_name,
    content::RenderProcessHost* process,
    std::optional<base::Value::Dict> filter) {
  return EventListener::ForURL(event_name, listener_url, process,
                               std::move(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForExtensionServiceWorker(
    const ExtensionId& extension_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    std::optional<base::Value::Dict> filter) {
  content::BrowserContext* browser_context =
      process ? process->GetBrowserContext() : nullptr;
  return EventListener::ForExtensionServiceWorker(
      event_name, extension_id, process, browser_context,
      Extension::GetBaseURLFromExtensionId(extension_id),
      GetWorkerVersionId(false), GetWorkerThreadId(false), std::move(filter));
}

void EventListenerMapTest::TestUnfilteredEventsGoToAllListeners(
    const EventListenerConstructor& constructor) {
  listeners_->AddListener(
      constructor.Run(kEvent1Name, process_.get(), base::Value::Dict()));
  std::unique_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  ASSERT_EQ(1u, listeners_->GetEventListeners(*event).size());
}

TEST_F(EventListenerMapTest, UnfilteredEventsGoToAllListenersForExtensions) {
  TestUnfilteredEventsGoToAllListeners(
      base::BindRepeating(&CreateEventListenerForExtension, kExt1Id));
}

TEST_F(EventListenerMapTest, UnfilteredEventsGoToAllListenersForURLs) {
  TestUnfilteredEventsGoToAllListeners(
      base::BindRepeating(&CreateEventListenerForURL, GURL(kURL)));
}

TEST_F(EventListenerMapTest,
       UnfilteredEventsGoToAllListenersForExtensionServiceWorkers) {
  TestUnfilteredEventsGoToAllListeners(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, kExt1Id));
}

TEST_F(EventListenerMapTest, FilteredEventsGoToAllMatchingListeners) {
  auto create_filter = [&](const std::string& filter_str) {
    return CreateHostSuffixFilter(filter_str);
  };
  auto create_empty_filter = []() { return base::Value::Dict(); };

  for (bool is_for_service_worker : {false, true}) {
    listeners_->AddListener(CreateLazyListener(kEvent1Name, kExt1Id,
                                               create_filter("google.com"),
                                               is_for_service_worker));
    listeners_->AddListener(CreateLazyListener(
        kEvent1Name, kExt1Id, create_empty_filter(), is_for_service_worker));
  }

  std::unique_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info->url = GURL("http://www.google.com");
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(4u, targets.size());
}

TEST_P(EventListenerMapWithContextTest,
       FilteredEventsOnlyGoToMatchingListeners) {
  const bool is_for_service_worker = GetParam();
  struct TestCase {
    const std::string filter_host_suffix;
    const std::string url_of_event;
  } kTestCases[] = {
      {"google.com", "http://www.google.com"},
      {"yahoo.com", "http://www.yahoo.com"},
  };
  for (const TestCase& test_case : kTestCases) {
    listeners_->AddListener(
        CreateLazyListener(kEvent1Name, kExt1Id,
                           CreateHostSuffixFilter(test_case.filter_host_suffix),
                           is_for_service_worker));
  }

  // Matching filters.
  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(base::StringPrintf("host_suffix = %s, url = %s",
                                    test_case.filter_host_suffix.c_str(),
                                    test_case.url_of_event.c_str()));
    std::unique_ptr<Event> event(
        CreateEvent(kEvent1Name, GURL(test_case.url_of_event)));
    std::set<const EventListener*> targets(
        listeners_->GetEventListeners(*event));
    ASSERT_EQ(1u, targets.size());
    EXPECT_TRUE(
        CreateLazyListener(kEvent1Name, kExt1Id,
                           CreateHostSuffixFilter(test_case.filter_host_suffix),
                           is_for_service_worker)
            ->Equals(*targets.begin()));
  }

  // Non-matching filter.
  {
    std::unique_ptr<Event> event(
        CreateEvent(kEvent1Name, GURL("http://does_not_match.com")));
    std::set<const EventListener*> targets(
        listeners_->GetEventListeners(*event));
    EXPECT_TRUE(targets.empty());
  }
}

TEST_F(EventListenerMapTest, LazyAndUnlazyListenersGetReturned) {
  listeners_->AddListener(EventListener::CreateLazyListener(
      kEvent1Name, kExt1Id, process_->GetBrowserContext(), false, GURL(),
      CreateHostSuffixFilter("google.com")));

  listeners_->AddListener(
      EventListener::ForExtension(kEvent1Name, kExt1Id, process_.get(),
                                  CreateHostSuffixFilter("google.com")));

  std::unique_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info->url = GURL("http://www.google.com");
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(2u, targets.size());
}

void EventListenerMapTest::TestRemovingByProcess(
    const EventListenerConstructor& constructor) {
  listeners_->AddListener(constructor.Run(
      kEvent1Name, process_.get(), CreateHostSuffixFilter("google.com")));

  listeners_->AddListener(constructor.Run(
      kEvent2Name, process_.get(), CreateHostSuffixFilter("google.com")));

  listeners_->RemoveListenersForProcess(process_.get());

  std::unique_ptr<Event> event1(CreateNamedEvent(kEvent1Name));
  event1->filter_info->url = GURL("http://www.google.com");
  ASSERT_EQ(0u, listeners_->GetEventListeners(*event1).size());

  std::unique_ptr<Event> event2(CreateNamedEvent(kEvent2Name));
  event2->filter_info->url = GURL("http://www.google.com");
  ASSERT_EQ(0u, listeners_->GetEventListeners(*event2).size());
}

TEST_F(EventListenerMapTest, TestRemovingByProcessForExtension) {
  TestRemovingByProcess(
      base::BindRepeating(&CreateEventListenerForExtension, kExt1Id));
}

TEST_F(EventListenerMapTest, TestRemovingByProcessForURL) {
  TestRemovingByProcess(
      base::BindRepeating(&CreateEventListenerForURL, GURL(kURL)));
}

TEST_F(EventListenerMapTest, TestRemovingByProcessForExtensionServiceWorker) {
  TestRemovingByProcess(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, kExt1Id));
}

void EventListenerMapTest::TestRemovingByListener(
    const EventListenerConstructor& constructor) {
  listeners_->AddListener(constructor.Run(
      kEvent1Name, process_.get(), CreateHostSuffixFilter("google.com")));

  listeners_->AddListener(constructor.Run(
      kEvent2Name, process_.get(), CreateHostSuffixFilter("google.com")));

  std::unique_ptr<EventListener> listener(constructor.Run(
      kEvent1Name, process_.get(), CreateHostSuffixFilter("google.com")));
  listeners_->RemoveListener(listener.get());

  std::unique_ptr<Event> event1(CreateNamedEvent(kEvent1Name));
  event1->filter_info->url = GURL("http://www.google.com");
  ASSERT_EQ(0u, listeners_->GetEventListeners(*event1).size());

  std::unique_ptr<Event> event2(CreateNamedEvent(kEvent2Name));
  event2->filter_info->url = GURL("http://www.google.com");
  ASSERT_EQ(1u, listeners_->GetEventListeners(*event2).size());
}

TEST_F(EventListenerMapTest, TestRemovingByListenerForExtension) {
  TestRemovingByListener(
      base::BindRepeating(&CreateEventListenerForExtension, kExt1Id));
}

TEST_F(EventListenerMapTest, TestRemovingByListenerForURL) {
  TestRemovingByListener(
      base::BindRepeating(&CreateEventListenerForURL, GURL(kURL)));
}

TEST_F(EventListenerMapTest, TestRemovingByListenerForExtensionServiceWorker) {
  TestRemovingByListener(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, kExt1Id));
}

TEST_P(EventListenerMapWithContextTest, TestLazyDoubleAddIsUndoneByRemove) {
  const bool is_for_service_worker = GetParam();
  listeners_->AddListener(CreateLazyListener(
      kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
      is_for_service_worker));
  listeners_->AddListener(CreateLazyListener(
      kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
      is_for_service_worker));

  std::unique_ptr<EventListener> listener = CreateLazyListener(
      kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
      is_for_service_worker);
  listeners_->RemoveListener(listener.get());

  std::unique_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info->url = GURL("http://www.google.com");
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  EXPECT_EQ(0u, targets.size());
}

TEST_F(EventListenerMapTest, HostSuffixFilterEquality) {
  base::Value::Dict filter1 = CreateHostSuffixFilter("google.com");
  base::Value::Dict filter2 = CreateHostSuffixFilter("google.com");
  EXPECT_EQ(filter1, filter2);
}

TEST_F(EventListenerMapTest, RemoveListenersForExtension) {
  for (bool is_for_service_worker : {false, true}) {
    listeners_->AddListener(CreateLazyListener(
        kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
        is_for_service_worker));
    listeners_->AddListener(CreateLazyListener(
        kEvent2Name, kExt1Id, CreateHostSuffixFilter("google.com"),
        is_for_service_worker));
  }

  listeners_->RemoveListenersForExtension(kExt1Id);

  std::unique_ptr<Event> event1(CreateNamedEvent(kEvent1Name));
  event1->filter_info->url = GURL("http://www.google.com");
  std::set<const EventListener*> targets(
      listeners_->GetEventListeners(*event1));
  ASSERT_EQ(0u, targets.size());

  std::unique_ptr<Event> event2(CreateNamedEvent(kEvent2Name));
  event2->filter_info->url = GURL("http://www.google.com");
  targets = listeners_->GetEventListeners(*event2);
  ASSERT_EQ(0u, targets.size());
}

TEST_P(EventListenerMapWithContextTest, AddExistingFilteredListener) {
  const bool is_for_service_worker = GetParam();

  bool first_new = listeners_->AddListener(CreateLazyListener(
      kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
      is_for_service_worker));
  bool second_new = listeners_->AddListener(CreateLazyListener(
      kEvent1Name, kExt1Id, CreateHostSuffixFilter("google.com"),
      is_for_service_worker));

  ASSERT_TRUE(first_new);
  ASSERT_FALSE(second_new);
}

void EventListenerMapTest::TestAddExistingUnfilteredListener(
    const EventListenerConstructor& constructor) {
  bool first_add = listeners_->AddListener(
      constructor.Run(kEvent1Name, process_.get(), base::Value::Dict()));
  bool second_add = listeners_->AddListener(
      constructor.Run(kEvent1Name, process_.get(), base::Value::Dict()));

  std::unique_ptr<EventListener> listener(
      constructor.Run(kEvent1Name, process_.get(), base::Value::Dict()));
  bool first_remove = listeners_->RemoveListener(listener.get());
  bool second_remove = listeners_->RemoveListener(listener.get());

  ASSERT_TRUE(first_add);
  ASSERT_FALSE(second_add);
  ASSERT_TRUE(first_remove);
  ASSERT_FALSE(second_remove);
}

TEST_F(EventListenerMapTest, AddExistingUnfilteredListenerForExtensions) {
  TestAddExistingUnfilteredListener(
      base::BindRepeating(&CreateEventListenerForExtension, kExt1Id));
}

TEST_F(EventListenerMapTest, AddExistingUnfilteredListenerForURLs) {
  TestAddExistingUnfilteredListener(
      base::BindRepeating(&CreateEventListenerForURL, GURL(kURL)));
}

TEST_F(EventListenerMapTest,
       AddExistingUnfilteredListenerForExtensionServiceWorker) {
  TestAddExistingUnfilteredListener(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, kExt1Id));
}

TEST_F(EventListenerMapTest, RemovingRouters) {
  listeners_->AddListener(EventListener::ForExtension(
      kEvent1Name, kExt1Id, process_.get(), base::Value::Dict()));
  listeners_->AddListener(EventListener::ForURL(
      kEvent1Name, GURL(kURL), process_.get(), base::Value::Dict()));
  listeners_->AddListener(CreateEventListenerForExtensionServiceWorker(
      kExt1Id, kEvent1Name, process_.get(), std::nullopt));
  listeners_->RemoveListenersForProcess(process_.get());
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));
}

void EventListenerMapTest::TestHasListenerForEvent(
    const EventListenerConstructor& constructor) {
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));

  listeners_->AddListener(
      constructor.Run(kEvent1Name, process_.get(), base::Value::Dict()));

  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent2Name));
  ASSERT_TRUE(listeners_->HasListenerForEvent(kEvent1Name));
  listeners_->RemoveListenersForProcess(process_.get());
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));
}

TEST_F(EventListenerMapTest, HasListenerForEventForExtension) {
  TestHasListenerForEvent(
      base::BindRepeating(&CreateEventListenerForExtension, kExt1Id));
}

TEST_F(EventListenerMapTest, HasListenerForEventForURL) {
  TestHasListenerForEvent(
      base::BindRepeating(&CreateEventListenerForURL, GURL(kURL)));
}

TEST_F(EventListenerMapTest, HasListenerForEventForExtensionServiceWorker) {
  TestHasListenerForEvent(base::BindRepeating(
      &CreateEventListenerForExtensionServiceWorker, kExt1Id));
}

TEST_F(EventListenerMapTest, HasListenerForExtension) {
  ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));

  auto create_event_listener = [&](bool is_for_service_worker, bool lazy) {
    if (lazy) {
      return EventListener::CreateLazyListener(
          kEvent1Name, kExt1Id, process_->GetBrowserContext(),
          is_for_service_worker, Extension::GetBaseURLFromExtensionId(kExt1Id),
          std::nullopt);
    }
    if (is_for_service_worker) {
      return EventListener::ForExtensionServiceWorker(
          kEvent1Name, kExt1Id, process_.get(), process_->GetBrowserContext(),
          Extension::GetBaseURLFromExtensionId(kExt1Id),
          GetWorkerVersionId(lazy), GetWorkerThreadId(lazy), std::nullopt);
    }
    return EventListener::ForExtension(kEvent1Name, kExt1Id, process_.get(),
                                       std::nullopt);
  };

  for (bool is_for_service_worker : {false, true}) {
    // Non-lazy listener.
    listeners_->AddListener(
        create_event_listener(is_for_service_worker, false));
    // Lazy listener.
    listeners_->AddListener(create_event_listener(is_for_service_worker, true));

    ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent2Name));
    ASSERT_TRUE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
    ASSERT_FALSE(listeners_->HasListenerForExtension(kExt2Id, kEvent1Name));
    listeners_->RemoveListenersForProcess(process_.get());
    ASSERT_TRUE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
    listeners_->RemoveListenersForExtension(kExt1Id);
    ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
  }
}

TEST_P(EventListenerMapWithContextTest, AddLazyListenersFromPreferences) {
  const bool is_for_service_worker = GetParam();
  struct TestCase {
    const std::string filter_host_suffix;
    const std::string url_of_event;
  } kTestCases[] = {
      {"google.com", "http://www.google.com"},
      {"yahoo.com", "http://www.yahoo.com"},
  };
  base::Value::List filter_list;
  for (const TestCase& test_case : kTestCases)
    filter_list.Append(CreateHostSuffixFilter(test_case.filter_host_suffix));

  base::Value::Dict filtered_listeners;
  filtered_listeners.Set(kEvent1Name, std::move(filter_list));
  listeners_->LoadFilteredLazyListeners(
      browser_context(), kExt1Id, is_for_service_worker, filtered_listeners);

  // Matching filters.
  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(base::StringPrintf("host_suffix = %s, url = %s",
                                    test_case.filter_host_suffix.c_str(),
                                    test_case.url_of_event.c_str()));
    std::unique_ptr<Event> event(
        CreateEvent(kEvent1Name, GURL(test_case.url_of_event)));
    std::set<const EventListener*> targets(
        listeners_->GetEventListeners(*event));
    ASSERT_EQ(1u, targets.size());
    EXPECT_TRUE(
        CreateLazyListener(kEvent1Name, kExt1Id,
                           CreateHostSuffixFilter(test_case.filter_host_suffix),
                           is_for_service_worker)
            ->Equals(*targets.begin()));
  }

  // Non-matching filter.
  {
    std::unique_ptr<Event> event(
        CreateEvent(kEvent1Name, GURL("http://does_not_match.com")));
    std::set<const EventListener*> targets(
        listeners_->GetEventListeners(*event));
    EXPECT_EQ(0u, targets.size());
  }
}

TEST_F(EventListenerMapTest, CorruptedExtensionPrefsShouldntCrash) {
  base::Value::Dict filtered_listeners;
  // kEvent1Name should be associated with a list, not a dictionary.
  filtered_listeners.Set(kEvent1Name, CreateHostSuffixFilter("google.com"));

  listeners_->LoadFilteredLazyListeners(browser_context(), kExt1Id, false,
                                        filtered_listeners);
  listeners_->LoadFilteredLazyListeners(browser_context(), kExt1Id, true,
                                        filtered_listeners);

  std::unique_ptr<Event> event(
      CreateEvent(kEvent1Name, GURL("http://www.google.com")));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(0u, targets.size());
}

INSTANTIATE_TEST_SUITE_P(NonServiceWorker,
                         EventListenerMapWithContextTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         EventListenerMapWithContextTest,
                         testing::Values(true));

}  // namespace

}  // namespace extensions
