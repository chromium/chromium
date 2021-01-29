// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_info/system_info_api.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/api/system_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kFakeExtensionId[] = "extension_id";
const char kFakeExtensionId2[] = "extension_id_2";

// Extends TestExtensionsBrowserClient to support a secondary context, and also
// tracks events broadcast to renderers.
class FakeExtensionsBrowserClient : public TestExtensionsBrowserClient {
 public:
  struct Broadcast {
    Broadcast(events::HistogramValue histogram_value,
              std::unique_ptr<base::ListValue> args,
              bool dispatch_to_off_the_record_profiles)
        : histogram_value(histogram_value),
          args(std::move(args)),
          dispatch_to_off_the_record_profiles(
              dispatch_to_off_the_record_profiles) {}
    Broadcast(Broadcast&&) = default;
    ~Broadcast() = default;

    events::HistogramValue histogram_value;
    std::unique_ptr<base::ListValue> args;
    bool dispatch_to_off_the_record_profiles;
  };

  // TestExtensionsBrowserClient:
  bool IsValidContext(content::BrowserContext* context) override {
    return TestExtensionsBrowserClient::IsValidContext(context) ||
           context == second_context_;
  }

  // TestExtensionsBrowserClient:
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override {
    if (context == second_context_)
      return second_context_;

    return TestExtensionsBrowserClient::GetOriginalContext(context);
  }

  // TestExtensionsBrowserClient:
  void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args,
      bool dispatch_to_off_the_record_profiles) override {
    event_name_to_broadcasts_map_[event_name].emplace_back(
        histogram_value, std::move(args), dispatch_to_off_the_record_profiles);
  }

  void SetSecondContext(content::BrowserContext* second_context) {
    DCHECK(!second_context_);
    DCHECK(second_context);
    DCHECK(!second_context->IsOffTheRecord());
    second_context_ = second_context;
  }

  const std::vector<Broadcast>& GetBroadcastsForEvent(
      const std::string& event_name) {
    return event_name_to_broadcasts_map_[event_name];
  }

 private:
  content::BrowserContext* second_context_ = nullptr;
  base::flat_map<std::string, std::vector<Broadcast>>
      event_name_to_broadcasts_map_;
};

class FakeDisplayInfoProvider : public DisplayInfoProvider {
 public:
  FakeDisplayInfoProvider() = default;
  ~FakeDisplayInfoProvider() override = default;

  // DisplayInfoProvider:
  void StartObserving() override { is_observing_ = true; }
  void StopObserving() override { is_observing_ = false; }

  bool is_observing() const { return is_observing_; }

 private:
  bool is_observing_ = false;
};

EventRouter* CreateAndUsePreflessEventRouter(content::BrowserContext* context) {
  return static_cast<EventRouter*>(
      EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating([](content::BrowserContext* context) {
            return static_cast<std::unique_ptr<KeyedService>>(
                std::make_unique<EventRouter>(context,
                                              nullptr /* extensions_prefs */));
          })));
}

const std::string& GetFakeStorageDeviceId() {
  static const base::NoDestructor<std::string> id([] {
    return storage_monitor::StorageInfo::MakeDeviceId(
        storage_monitor::StorageInfo::Type::REMOVABLE_MASS_STORAGE_WITH_DCIM,
        "storage_device_id");
  }());
  return *id;
}

const storage_monitor::StorageInfo& GetFakeStorageInfo() {
  static const base::NoDestructor<storage_monitor::StorageInfo> info([] {
    return storage_monitor::StorageInfo(
        GetFakeStorageDeviceId(),
        base::FilePath::StringType() /* device_location */,
        base::string16() /* label */, base::string16() /* vendor */,
        base::string16() /* model */, 0 /* size_in_bytes */);
  }());
  return *info;
}

base::ListValue GetStorageAttachedArgs() {
  // Because of the use of GetTransientIdForDeviceId() in
  // BuildStorageUnitInfo(), we cannot use a static variable and cache the
  // returned ListValue.
  api::system_storage::StorageUnitInfo unit;
  systeminfo::BuildStorageUnitInfo(GetFakeStorageInfo(), &unit);
  base::ListValue args;
  args.Append(unit.ToValue());
  return args;
}

base::ListValue GetStorageDetachedArgs() {
  // Because of the use of GetTransientIdForDeviceId(), we cannot use a static
  // variable and cache the returned ListValue.
  base::ListValue args;
  args.AppendString(
      storage_monitor::StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          GetFakeStorageDeviceId()));
  return args;
}

}  // namespace

class SystemInfoAPITest : public testing::Test {
 protected:
  enum class EventType { kDisplay, kStorageAttached, kStorageDetached };

  SystemInfoAPITest() = default;
  ~SystemInfoAPITest() override = default;

  void SetUp() override {
    client_.SetMainContext(&context1_);
    client_.SetSecondContext(&context2_);
    ExtensionsBrowserClient::Set(&client_);

    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServicesForTest(&context1_);
    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServicesForTest(&context2_);

    router1_ = CreateAndUsePreflessEventRouter(&context1_);
    router2_ = CreateAndUsePreflessEventRouter(&context2_);

    FakeDisplayInfoProvider::InitializeForTesting(&display_info_provider_);

    storage_monitor_ = storage_monitor::TestStorageMonitor::CreateAndInstall();
  }

  void TearDown() override {
    storage_monitor_ = nullptr;
    storage_monitor::StorageMonitor::Destroy();

    DisplayInfoProvider::ResetForTesting();

    router2_ = nullptr;
    router1_ = nullptr;

    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&context2_);
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&context1_);

    ExtensionsBrowserClient::Set(nullptr);
  }

  std::string EventTypeToName(EventType type) {
    switch (type) {
      case EventType::kDisplay:
        return api::system_display::OnDisplayChanged::kEventName;
      case EventType::kStorageAttached:
        return api::system_storage::OnAttached::kEventName;
      case EventType::kStorageDetached:
        return api::system_storage::OnDetached::kEventName;
    }
  }

  void AddEventListener(EventRouter* router,
                        EventType type,
                        const std::string& extension_id = kFakeExtensionId) {
    router->AddEventListener(EventTypeToName(type), nullptr /* process */,
                             extension_id);
  }

  void RemoveEventListener(EventRouter* router,
                           EventType type,
                           const std::string& extension_id = kFakeExtensionId) {
    router->RemoveEventListener(EventTypeToName(type), nullptr /* process */,
                                extension_id);
  }

  // Returns true if the DisplayInfoProvider is observing. When
  // DisplayInfoProvider is observing, it is notified of display changes, and is
  // responsible for dispatching events.
  bool IsDispatchingDisplayEvents() {
    return display_info_provider_.is_observing();
  }

  // Returns true if the extension is observing storage-attach changes from the
  // StorageMonitor and subsequently dispatching the expected storage-attached
  // events.
  bool IsDispatchingStorageAttachedEvents() {
    size_t num_events_before =
        client_
            .GetBroadcastsForEvent(api::system_storage::OnAttached::kEventName)
            .size();

    // Because the StorageMonitor uses thread-safe observers, we need the
    // RunUntilIdle() statement.
    storage_monitor_->receiver()->ProcessAttach(GetFakeStorageInfo());
    base::RunLoop().RunUntilIdle();

    const std::vector<FakeExtensionsBrowserClient::Broadcast>& broadcasts =
        client_.GetBroadcastsForEvent(
            api::system_storage::OnAttached::kEventName);

    size_t num_events_after = broadcasts.size();
    if (num_events_after != num_events_before + 1)
      return false;

    return broadcasts.back().histogram_value ==
               events::SYSTEM_STORAGE_ON_ATTACHED &&
           broadcasts.back().args &&
           *broadcasts.back().args == GetStorageAttachedArgs() &&
           !broadcasts.back().dispatch_to_off_the_record_profiles;
  }

  // Returns true if the extension is observing storage-detach changes from the
  // StorageMonitor and subsequently dispatching the expected storage-detached
  // events.
  bool IsDispatchingStorageDetachedEvents() {
    size_t num_events_before =
        client_
            .GetBroadcastsForEvent(api::system_storage::OnDetached::kEventName)
            .size();

    // Because the StorageMonitor uses thread-safe observers, we need the
    // RunUntilIdle() statement.
    storage_monitor_->receiver()->ProcessDetach(GetFakeStorageDeviceId());
    base::RunLoop().RunUntilIdle();

    const std::vector<FakeExtensionsBrowserClient::Broadcast>& broadcasts =
        client_.GetBroadcastsForEvent(
            api::system_storage::OnDetached::kEventName);

    size_t num_events_after = broadcasts.size();
    if (num_events_after != num_events_before + 1)
      return false;

    return broadcasts.back().histogram_value ==
               events::SYSTEM_STORAGE_ON_DETACHED &&
           broadcasts.back().args &&
           *broadcasts.back().args == GetStorageDetachedArgs() &&
           !broadcasts.back().dispatch_to_off_the_record_profiles;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context1_;
  content::TestBrowserContext context2_;
  FakeExtensionsBrowserClient client_;
  EventRouter* router1_ = nullptr;
  EventRouter* router2_ = nullptr;
  FakeDisplayInfoProvider display_info_provider_;
  storage_monitor::TestStorageMonitor* storage_monitor_;
};

/******************************************************************************/
// Display event tests
/******************************************************************************/

TEST_F(SystemInfoAPITest, DisplayListener_AddRemove) {
  EXPECT_FALSE(IsDispatchingDisplayEvents());

  // Say a display event listener exists before SystemInfoAPI is created.
  AddEventListener(router1_, EventType::kDisplay);
  SystemInfoAPI api1(&context1_);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  RemoveEventListener(router1_, EventType::kDisplay);
  EXPECT_FALSE(IsDispatchingDisplayEvents());

  AddEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  api1.Shutdown();
  EXPECT_FALSE(IsDispatchingDisplayEvents());
}

TEST_F(SystemInfoAPITest, DisplayListener_MultipleContexts) {
  SystemInfoAPI api1(&context1_);
  SystemInfoAPI api2(&context2_);

  EXPECT_FALSE(IsDispatchingDisplayEvents());
  AddEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());
  AddEventListener(router2_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  // DisplayInfoProvider should be observing (in other words, display events
  // should be dispatched) if and only if at least one browser context has a
  // display listener.
  RemoveEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());
  RemoveEventListener(router2_, EventType::kDisplay);
  EXPECT_FALSE(IsDispatchingDisplayEvents());

  AddEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  api2.Shutdown();
  EXPECT_TRUE(IsDispatchingDisplayEvents());
  api1.Shutdown();
  EXPECT_FALSE(IsDispatchingDisplayEvents());
}

TEST_F(SystemInfoAPITest, DisplayListener_MultipleListeners) {
  SystemInfoAPI api1(&context1_);
  EXPECT_FALSE(IsDispatchingDisplayEvents());

  AddEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  // Add another listener for the same browser context and use a different
  // extension ID so that it is distinct from the other listener.
  AddEventListener(router1_, EventType::kDisplay, kFakeExtensionId2);
  EXPECT_TRUE(IsDispatchingDisplayEvents());

  // Dispatch events until all listeners are removed.
  RemoveEventListener(router1_, EventType::kDisplay);
  EXPECT_TRUE(IsDispatchingDisplayEvents());
  RemoveEventListener(router1_, EventType::kDisplay, kFakeExtensionId2);
  EXPECT_FALSE(IsDispatchingDisplayEvents());

  api1.Shutdown();
}

/******************************************************************************/
// Storage event tests
/******************************************************************************/

TEST_F(SystemInfoAPITest, StorageListener_AddRemove) {
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());

  // Say a storage-attached listener exists before SystemInfoAPI is created.
  AddEventListener(router1_, EventType::kStorageAttached);
  SystemInfoAPI api1(&context1_);

  // If a storage-attached *or* storage-detached listener exists, then both
  // events are dispatched.
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  AddEventListener(router1_, EventType::kStorageDetached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  RemoveEventListener(router1_, EventType::kStorageDetached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  RemoveEventListener(router1_, EventType::kStorageAttached);
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());

  AddEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  api1.Shutdown();
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());
}

TEST_F(SystemInfoAPITest, StorageListener_MultipleContexts) {
  SystemInfoAPI api1(&context1_);
  SystemInfoAPI api2(&context2_);

  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());
  AddEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());
  AddEventListener(router2_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  // Storage events should be dispatched if and only if at least one browser
  // context has a storage listener.
  RemoveEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());
  RemoveEventListener(router2_, EventType::kStorageAttached);
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());

  AddEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  api2.Shutdown();
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());
  api1.Shutdown();
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());
}

TEST_F(SystemInfoAPITest, StorageListener_MultipleListeners) {
  SystemInfoAPI api1(&context1_);
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());

  AddEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  // Add another listener for the same browser context and use a different
  // extension ID so that it is distinct from the other listener.
  AddEventListener(router1_, EventType::kStorageAttached, kFakeExtensionId2);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());

  // Dispatch events until all listeners are removed.
  RemoveEventListener(router1_, EventType::kStorageAttached);
  EXPECT_TRUE(IsDispatchingStorageAttachedEvents());
  EXPECT_TRUE(IsDispatchingStorageDetachedEvents());
  RemoveEventListener(router1_, EventType::kStorageAttached, kFakeExtensionId2);
  EXPECT_FALSE(IsDispatchingStorageAttachedEvents());
  EXPECT_FALSE(IsDispatchingStorageDetachedEvents());

  api1.Shutdown();
}

}  // namespace extensions
