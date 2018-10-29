// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/storage_monitor.h"
#include "storage/browser/quota/storage_observer.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/mock_storage_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;
using storage::HostStorageObservers;
using storage::QuotaClient;
using storage::QuotaManager;
using storage::SpecialStoragePolicy;
using storage::StorageMonitor;
using storage::StorageObserver;
using storage::StorageObserverList;
using storage::StorageTypeObservers;

namespace content {

namespace {

// TODO(crbug.com/889590): Use helper for url::Origin creation from string.
const url::Origin kDefaultOrigin =
    url::Origin::Create(GURL("http://www.foo.com/"));
const url::Origin kAlternativeOrigin =
    url::Origin::Create(GURL("http://www.bar.com/"));

class MockObserver : public StorageObserver {
 public:
  const StorageObserver::Event& LastEvent() const {
    CHECK(!events_.empty());
    return events_.back();
  }

  int EventCount() const {
    return events_.size();
  }

  // StorageObserver implementation:
  void OnStorageEvent(const StorageObserver::Event& event) override {
    events_.push_back(event);
  }

 private:
  std::vector<StorageObserver::Event> events_;
};

// A mock quota manager for overriding GetUsageAndQuotaForWebApps().
class UsageMockQuotaManager : public QuotaManager {
 public:
  UsageMockQuotaManager(SpecialStoragePolicy* special_storage_policy)
      : QuotaManager(false,
                     base::FilePath(),
                     base::ThreadTaskRunnerHandle::Get().get(),
                     special_storage_policy,
                     storage::GetQuotaSettingsFunc()),
        callback_usage_(0),
        callback_quota_(0),
        callback_status_(QuotaStatusCode::kOk),
        initialized_(false) {}

  void SetCallbackParams(int64_t usage, int64_t quota, QuotaStatusCode status) {
    initialized_ = true;
    callback_quota_ = quota;
    callback_usage_ = usage;
    callback_status_ = status;
  }

  void InvokeCallback() {
    std::move(delayed_callback_)
        .Run(callback_status_, callback_usage_, callback_quota_);
  }

  void GetUsageAndQuotaForWebApps(const url::Origin& origin,
                                  StorageType type,
                                  UsageAndQuotaCallback callback) override {
    if (initialized_)
      std::move(callback).Run(callback_status_, callback_usage_,
                              callback_quota_);
    else
      delayed_callback_ = std::move(callback);
  }

 protected:
  ~UsageMockQuotaManager() override = default;

 private:
  int64_t callback_usage_;
  int64_t callback_quota_;
  QuotaStatusCode callback_status_;
  bool initialized_;
  UsageAndQuotaCallback delayed_callback_;
};

}  // namespace

class StorageMonitorTestBase : public testing::Test {
 protected:
  void DispatchPendingEvents(StorageObserverList& observer_list) {
    observer_list.DispatchPendingEvent();
  }

  const StorageObserver::Event* GetPendingEvent(
      const StorageObserverList& observer_list) {
    return observer_list.notification_timer_.IsRunning()
               ? &observer_list.pending_event_
               : nullptr;
  }

  const StorageObserver::Event* GetPendingEvent(
      const HostStorageObservers& host_observers) {
    return GetPendingEvent(host_observers.observers_);
  }

  int GetRequiredUpdatesCount(const StorageObserverList& observer_list) {
    int count = 0;
    for (const auto& observer_state_pair : observer_list.observer_state_map_) {
      if (observer_state_pair.second.requires_update)
        ++count;
    }

    return count;
  }

  int GetRequiredUpdatesCount(const HostStorageObservers& host_observers) {
    return GetRequiredUpdatesCount(host_observers.observers_);
  }

  void SetLastNotificationTime(StorageObserverList& observer_list,
                               StorageObserver* observer) {
    ASSERT_TRUE(base::ContainsKey(observer_list.observer_state_map_, observer));
    StorageObserverList::ObserverState& state =
        observer_list.observer_state_map_[observer];
    state.last_notification_time = base::TimeTicks::Now() - state.rate;
  }

  void SetLastNotificationTime(HostStorageObservers& host_observers,
                               StorageObserver* observer) {
    SetLastNotificationTime(host_observers.observers_, observer);
  }

  int GetObserverCount(const HostStorageObservers& host_observers) {
    return host_observers.observers_.ObserverCount();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class StorageTestWithManagerBase : public StorageMonitorTestBase {
 public:
  void SetUp() override {
    storage_policy_ = new MockSpecialStoragePolicy();
    quota_manager_ = new UsageMockQuotaManager(storage_policy_.get());
  }

  void TearDown() override {
    // This ensures the quota manager is destroyed correctly.
    quota_manager_ = nullptr;
    scoped_task_environment_.RunUntilIdle();
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<UsageMockQuotaManager> quota_manager_;
};

// Tests for StorageObserverList:

using StorageObserverListTest = StorageMonitorTestBase;

// Test dispatching events to one observer.
TEST_F(StorageObserverListTest, DispatchEventToSingleObserver) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer;
  StorageObserverList observer_list;
  observer_list.AddObserver(&mock_observer, params);

  StorageObserver::Event event;
  event.filter = params.filter;

  // Verify that the first event is dispatched immediately.
  event.quota = 1;
  event.usage = 1;
  observer_list.OnStorageChange(event);
  EXPECT_EQ(1, mock_observer.EventCount());
  EXPECT_EQ(event, mock_observer.LastEvent());
  EXPECT_EQ(nullptr, GetPendingEvent(observer_list));
  EXPECT_EQ(0, GetRequiredUpdatesCount(observer_list));

  // Verify that the next event is pending.
  event.quota = 2;
  event.usage = 2;
  observer_list.OnStorageChange(event);
  EXPECT_EQ(1, mock_observer.EventCount());
  ASSERT_TRUE(GetPendingEvent(observer_list));
  EXPECT_EQ(event, *GetPendingEvent(observer_list));
  EXPECT_EQ(1, GetRequiredUpdatesCount(observer_list));

  // Fake the last notification time so that an event will be dispatched.
  SetLastNotificationTime(observer_list, &mock_observer);
  event.quota = 3;
  event.usage = 3;
  observer_list.OnStorageChange(event);
  EXPECT_EQ(2, mock_observer.EventCount());
  EXPECT_EQ(event, mock_observer.LastEvent());
  EXPECT_EQ(nullptr, GetPendingEvent(observer_list));
  EXPECT_EQ(0, GetRequiredUpdatesCount(observer_list));

  // Remove the observer.
  event.quota = 4;
  event.usage = 4;
  observer_list.RemoveObserver(&mock_observer);
  observer_list.OnStorageChange(event);
  EXPECT_EQ(2, mock_observer.EventCount());
  EXPECT_EQ(nullptr, GetPendingEvent(observer_list));
}

// Test dispatching events to multiple observers.
TEST_F(StorageObserverListTest, DispatchEventToMultipleObservers) {
  MockObserver mock_observer1;
  MockObserver mock_observer2;
  StorageObserverList observer_list;
  StorageObserver::Filter filter(StorageType::kPersistent, kDefaultOrigin);
  observer_list.AddObserver(
      &mock_observer1,
      StorageObserver::MonitorParams(
          filter, base::TimeDelta::FromHours(1), false));
  observer_list.AddObserver(
      &mock_observer2,
      StorageObserver::MonitorParams(
          filter, base::TimeDelta::FromHours(2), false));

  StorageObserver::Event event;
  event.filter = filter;

  // Verify that the first event is dispatched immediately.
  event.quota = 1;
  event.usage = 1;
  observer_list.OnStorageChange(event);
  EXPECT_EQ(1, mock_observer1.EventCount());
  EXPECT_EQ(1, mock_observer2.EventCount());
  EXPECT_EQ(event, mock_observer1.LastEvent());
  EXPECT_EQ(event, mock_observer2.LastEvent());
  EXPECT_EQ(nullptr, GetPendingEvent(observer_list));
  EXPECT_EQ(0, GetRequiredUpdatesCount(observer_list));

  // Fake the last notification time so that observer1 will receive the next
  // event, but it will be pending for observer2.
  SetLastNotificationTime(observer_list, &mock_observer1);
  event.quota = 2;
  event.usage = 2;
  observer_list.OnStorageChange(event);
  EXPECT_EQ(2, mock_observer1.EventCount());
  EXPECT_EQ(1, mock_observer2.EventCount());
  EXPECT_EQ(event, mock_observer1.LastEvent());
  ASSERT_TRUE(GetPendingEvent(observer_list));
  EXPECT_EQ(event, *GetPendingEvent(observer_list));
  EXPECT_EQ(1, GetRequiredUpdatesCount(observer_list));

  // Now dispatch the pending event to observer2.
  SetLastNotificationTime(observer_list, &mock_observer2);
  DispatchPendingEvents(observer_list);
  EXPECT_EQ(2, mock_observer1.EventCount());
  EXPECT_EQ(2, mock_observer2.EventCount());
  EXPECT_EQ(event, mock_observer1.LastEvent());
  EXPECT_EQ(event, mock_observer2.LastEvent());
  EXPECT_EQ(nullptr, GetPendingEvent(observer_list));
  EXPECT_EQ(0, GetRequiredUpdatesCount(observer_list));
}

// Ensure that the |origin| field in events match the origin specified by the
// observer on registration.
TEST_F(StorageObserverListTest, ReplaceEventOrigin) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer;
  StorageObserverList observer_list;
  observer_list.AddObserver(&mock_observer, params);

  StorageObserver::Event dispatched_event;
  dispatched_event.filter = params.filter;
  dispatched_event.filter.origin =
      url::Origin::Create(GURL("https://www.foo.com/bar"));
  observer_list.OnStorageChange(dispatched_event);

  EXPECT_EQ(params.filter.origin, mock_observer.LastEvent().filter.origin);
}

// Tests for HostStorageObservers:

using HostStorageObserversTest = StorageTestWithManagerBase;

// Verify that HostStorageObservers is initialized after the first usage change.
TEST_F(HostStorageObserversTest, InitializeOnUsageChange) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  const int64_t kUsage = 324554;
  const int64_t kQuota = 234354354;
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);

  MockObserver mock_observer;
  HostStorageObservers host_observers(quota_manager_.get());
  host_observers.AddObserver(&mock_observer, params);

  // Verify that HostStorageObservers dispatches the first event correctly.
  StorageObserver::Event expected_event(params.filter, kUsage, kQuota);
  host_observers.NotifyUsageChange(params.filter, 87324);
  EXPECT_EQ(1, mock_observer.EventCount());
  EXPECT_EQ(expected_event, mock_observer.LastEvent());
  EXPECT_TRUE(host_observers.is_initialized());

  // Verify that HostStorageObservers handles subsequent usage changes
  // correctly.
  const int64_t kDelta = 2345;
  expected_event.usage += kDelta;
  SetLastNotificationTime(host_observers, &mock_observer);
  host_observers.NotifyUsageChange(params.filter, kDelta);
  EXPECT_EQ(2, mock_observer.EventCount());
  EXPECT_EQ(expected_event, mock_observer.LastEvent());
}

// Verify that HostStorageObservers is initialized after the adding the first
// observer that elected to receive the initial state.
TEST_F(HostStorageObserversTest, InitializeOnObserver) {
  const int64_t kUsage = 74387;
  const int64_t kQuota = 92834743;
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);
  HostStorageObservers host_observers(quota_manager_.get());

  // |host_observers| should not be initialized after the first observer is
  // added because it did not elect to receive the initial state.
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer1;
  host_observers.AddObserver(&mock_observer1, params);
  EXPECT_FALSE(host_observers.is_initialized());
  EXPECT_EQ(0, mock_observer1.EventCount());

  // |host_observers| should be initialized after the second observer is
  // added.
  MockObserver mock_observer2;
  params.dispatch_initial_state = true;
  host_observers.AddObserver(&mock_observer2, params);
  StorageObserver::Event expected_event(params.filter, kUsage, kQuota);
  EXPECT_EQ(0, mock_observer1.EventCount());
  EXPECT_EQ(1, mock_observer2.EventCount());
  EXPECT_EQ(expected_event, mock_observer2.LastEvent());
  EXPECT_TRUE(host_observers.is_initialized());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));

  // Verify that both observers will receive events after a usage change.
  const int64_t kDelta = 2345;
  expected_event.usage += kDelta;
  SetLastNotificationTime(host_observers, &mock_observer2);
  host_observers.NotifyUsageChange(params.filter, kDelta);
  EXPECT_EQ(1, mock_observer1.EventCount());
  EXPECT_EQ(2, mock_observer2.EventCount());
  EXPECT_EQ(expected_event, mock_observer1.LastEvent());
  EXPECT_EQ(expected_event, mock_observer2.LastEvent());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));

  // Verify that the addition of a third observer only causes an event to be
  // dispatched to the new observer.
  MockObserver mock_observer3;
  params.dispatch_initial_state = true;
  host_observers.AddObserver(&mock_observer3, params);
  EXPECT_EQ(1, mock_observer1.EventCount());
  EXPECT_EQ(2, mock_observer2.EventCount());
  EXPECT_EQ(1, mock_observer3.EventCount());
  EXPECT_EQ(expected_event, mock_observer3.LastEvent());
}

// Verify that negative usage and quota is changed to zero.
TEST_F(HostStorageObserversTest, NegativeUsageAndQuota) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  const int64_t kUsage = -324554;
  const int64_t kQuota = -234354354;
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);

  MockObserver mock_observer;
  HostStorageObservers host_observers(quota_manager_.get());
  host_observers.AddObserver(&mock_observer, params);

  StorageObserver::Event expected_event(params.filter, 0, 0);
  host_observers.NotifyUsageChange(params.filter, -87324);
  EXPECT_EQ(expected_event, mock_observer.LastEvent());
}

// Verify that HostStorageObservers can recover from a bad initialization.
TEST_F(HostStorageObserversTest, RecoverFromBadUsageInit) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer;
  HostStorageObservers host_observers(quota_manager_.get());
  host_observers.AddObserver(&mock_observer, params);

  // Set up the quota manager to return an error status.
  const int64_t kUsage = 6656;
  const int64_t kQuota = 99585556;
  quota_manager_->SetCallbackParams(kUsage, kQuota,
                                    QuotaStatusCode::kErrorNotSupported);

  // Verify that |host_observers| is not initialized and an event has not been
  // dispatched.
  host_observers.NotifyUsageChange(params.filter, 9438);
  EXPECT_EQ(0, mock_observer.EventCount());
  EXPECT_FALSE(host_observers.is_initialized());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));

  // Now ensure that quota manager returns a good status.
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);
  host_observers.NotifyUsageChange(params.filter, 9048543);
  StorageObserver::Event expected_event(params.filter, kUsage, kQuota);
  EXPECT_EQ(1, mock_observer.EventCount());
  EXPECT_EQ(expected_event, mock_observer.LastEvent());
  EXPECT_TRUE(host_observers.is_initialized());
}

// Verify that HostStorageObservers handle initialization of the cached usage
// and quota correctly.
TEST_F(HostStorageObserversTest, AsyncInitialization) {
  StorageObserver::MonitorParams params(StorageType::kPersistent,
                                        kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer;
  HostStorageObservers host_observers(quota_manager_.get());
  host_observers.AddObserver(&mock_observer, params);

  // Trigger initialization. Leave the mock quota manager uninitialized so that
  // the callback is not invoked.
  host_observers.NotifyUsageChange(params.filter, 7645);
  EXPECT_EQ(0, mock_observer.EventCount());
  EXPECT_FALSE(host_observers.is_initialized());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));

  // Simulate notifying |host_observers| of a usage change before initialization
  // is complete.
  const int64_t kUsage = 6656;
  const int64_t kQuota = 99585556;
  const int64_t kDelta = 327643;
  host_observers.NotifyUsageChange(params.filter, kDelta);
  EXPECT_EQ(0, mock_observer.EventCount());
  EXPECT_FALSE(host_observers.is_initialized());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));

  // Simulate an asynchronous callback from QuotaManager.
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);
  quota_manager_->InvokeCallback();
  StorageObserver::Event expected_event(params.filter, kUsage + kDelta, kQuota);
  EXPECT_EQ(1, mock_observer.EventCount());
  EXPECT_EQ(expected_event, mock_observer.LastEvent());
  EXPECT_TRUE(host_observers.is_initialized());
  EXPECT_EQ(nullptr, GetPendingEvent(host_observers));
  EXPECT_EQ(0, GetRequiredUpdatesCount(host_observers));
}

// Tests for StorageTypeObservers:

using StorageTypeObserversTest = StorageTestWithManagerBase;

// Test adding and removing observers.
TEST_F(StorageTypeObserversTest, AddRemoveObservers) {
  StorageTypeObservers type_observers(quota_manager_.get());

  StorageObserver::MonitorParams params1(StorageType::kPersistent,
                                         kDefaultOrigin,
                                         base::TimeDelta::FromHours(1), false);
  StorageObserver::MonitorParams params2(StorageType::kPersistent,
                                         kAlternativeOrigin,
                                         base::TimeDelta::FromHours(1), false);
  std::string host1 = net::GetHostOrSpecFromURL(params1.filter.origin.GetURL());
  std::string host2 = net::GetHostOrSpecFromURL(params2.filter.origin.GetURL());

  MockObserver mock_observer1;
  MockObserver mock_observer2;
  MockObserver mock_observer3;
  type_observers.AddObserver(&mock_observer1, params1);
  type_observers.AddObserver(&mock_observer2, params1);

  type_observers.AddObserver(&mock_observer1, params2);
  type_observers.AddObserver(&mock_observer2, params2);
  type_observers.AddObserver(&mock_observer3, params2);

  // Verify that the observers have been removed correctly.
  ASSERT_TRUE(type_observers.GetHostObservers(host1));
  ASSERT_TRUE(type_observers.GetHostObservers(host2));
  EXPECT_EQ(2, GetObserverCount(*type_observers.GetHostObservers(host1)));
  EXPECT_EQ(3, GetObserverCount(*type_observers.GetHostObservers(host2)));

  // Remove all instances of observer1.
  type_observers.RemoveObserver(&mock_observer1);
  ASSERT_TRUE(type_observers.GetHostObservers(host1));
  ASSERT_TRUE(type_observers.GetHostObservers(host2));
  EXPECT_EQ(1, GetObserverCount(*type_observers.GetHostObservers(host1)));
  EXPECT_EQ(2, GetObserverCount(*type_observers.GetHostObservers(host2)));

  // Remove all instances of observer2.
  type_observers.RemoveObserver(&mock_observer2);
  ASSERT_TRUE(type_observers.GetHostObservers(host2));
  EXPECT_EQ(1, GetObserverCount(*type_observers.GetHostObservers(host2)));
  // Observers of host1 has been deleted as it is empty.
  EXPECT_FALSE(type_observers.GetHostObservers(host1));
}

// Tests for StorageMonitor:

class StorageMonitorTest : public StorageTestWithManagerBase {
 public:
  StorageMonitorTest()
      : storage_monitor_(nullptr),
        params1_(StorageType::kTemporary,
                 kDefaultOrigin,
                 base::TimeDelta::FromHours(1),
                 false),
        params2_(StorageType::kPersistent,
                 kDefaultOrigin,
                 base::TimeDelta::FromHours(1),
                 false) {}

 protected:
  void SetUp() override {
    StorageTestWithManagerBase::SetUp();

    storage_monitor_ = quota_manager_->storage_monitor_.get();
    host_ = net::GetHostOrSpecFromURL(params1_.filter.origin.GetURL());

    storage_monitor_->AddObserver(&mock_observer1_, params1_);
    storage_monitor_->AddObserver(&mock_observer2_, params1_);

    storage_monitor_->AddObserver(&mock_observer1_, params2_);
    storage_monitor_->AddObserver(&mock_observer2_, params2_);
    storage_monitor_->AddObserver(&mock_observer3_, params2_);
  }

  int GetObserverCount(StorageType storage_type) {
    const StorageTypeObservers* type_observers =
        storage_monitor_->GetStorageTypeObservers(storage_type);
    return StorageMonitorTestBase::GetObserverCount(
                *type_observers->GetHostObservers(host_));
  }

  void CheckObserverCount(int expected_temporary, int expected_persistent) {
    ASSERT_TRUE(
        storage_monitor_->GetStorageTypeObservers(StorageType::kTemporary));
    ASSERT_TRUE(
        storage_monitor_->GetStorageTypeObservers(StorageType::kTemporary)
            ->GetHostObservers(host_));
    EXPECT_EQ(expected_temporary, GetObserverCount(StorageType::kTemporary));

    ASSERT_TRUE(
        storage_monitor_->GetStorageTypeObservers(StorageType::kPersistent));
    ASSERT_TRUE(
        storage_monitor_->GetStorageTypeObservers(StorageType::kPersistent)
            ->GetHostObservers(host_));
    EXPECT_EQ(expected_persistent, GetObserverCount(StorageType::kPersistent));
  }

  StorageMonitor* storage_monitor_;
  StorageObserver::MonitorParams params1_;
  StorageObserver::MonitorParams params2_;
  MockObserver mock_observer1_;
  MockObserver mock_observer2_;
  MockObserver mock_observer3_;
  std::string host_;
};

// Test adding storage observers.
TEST_F(StorageMonitorTest, AddObservers) {
  // Verify that the observers are added correctly.
  CheckObserverCount(2, 3);
}

// Test dispatching events to storage observers.
TEST_F(StorageMonitorTest, EventDispatch) {
  // Verify dispatch of events.
  const int64_t kUsage = 5325;
  const int64_t kQuota = 903845;
  quota_manager_->SetCallbackParams(kUsage, kQuota, QuotaStatusCode::kOk);
  storage_monitor_->NotifyUsageChange(params1_.filter, 9048543);

  StorageObserver::Event expected_event(params1_.filter, kUsage, kQuota);
  EXPECT_EQ(1, mock_observer1_.EventCount());
  EXPECT_EQ(1, mock_observer2_.EventCount());
  EXPECT_EQ(0, mock_observer3_.EventCount());
  EXPECT_EQ(expected_event, mock_observer1_.LastEvent());
  EXPECT_EQ(expected_event, mock_observer2_.LastEvent());
}

// Test removing all instances of an observer.
TEST_F(StorageMonitorTest, RemoveObserver) {
  storage_monitor_->RemoveObserver(&mock_observer1_);
  CheckObserverCount(1, 2);
}

// Integration test for QuotaManager and StorageMonitor:

class StorageMonitorIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    storage_policy_ = new MockSpecialStoragePolicy();
    quota_manager_ = new QuotaManager(
        false, data_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get().get(),
        storage_policy_.get(), storage::GetQuotaSettingsFunc());

    client_ = new MockStorageClient(quota_manager_->proxy(), nullptr,
                                    QuotaClient::kFileSystem, 0);

    quota_manager_->proxy()->RegisterClient(client_);
  }

  void TearDown() override {
    // This ensures the quota manager is destroyed correctly.
    quota_manager_ = nullptr;
    scoped_task_environment_.RunUntilIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<QuotaManager> quota_manager_;
  MockStorageClient* client_;
};

// This test simulates a usage change in a quota client and verifies that a
// storage observer will receive a storage event.
TEST_F(StorageMonitorIntegrationTest, NotifyUsageEvent) {
  const StorageType kTestStorageType = StorageType::kPersistent;
  const int64_t kTestUsage = 234743;

  // Register the observer.
  StorageObserver::MonitorParams params(kTestStorageType, kDefaultOrigin,
                                        base::TimeDelta::FromHours(1), false);
  MockObserver mock_observer;
  quota_manager_->AddStorageObserver(&mock_observer, params);

  // Fire a usage change.
  client_->AddOriginAndNotify(kDefaultOrigin, kTestStorageType, kTestUsage);
  scoped_task_environment_.RunUntilIdle();

  // Verify that the observer receives it.
  ASSERT_EQ(1, mock_observer.EventCount());
  const StorageObserver::Event& event = mock_observer.LastEvent();
  EXPECT_EQ(params.filter, event.filter);
  EXPECT_EQ(kTestUsage, event.usage);
}

}  // namespace content
