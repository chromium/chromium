// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_process_observer_hub.h"

#include "base/process/process.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/test/browser_task_environment.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

ServiceProcessInfo MakeFakeInfo(uint64_t id) {
  return ServiceProcessInfo(audio::mojom::AudioService::Name_,
                            /*site=*/std::nullopt,
                            ServiceProcessId::FromUnsafeValue(id),
                            base::Process::Current().Duplicate());
}

class TestObserver
    : public ServiceProcessObserverHub<audio::mojom::AudioService>::Observer {
 public:
  void OnServiceLaunched(const ServiceProcessInfo& info) override {
    launch_count_++;
  }
  void OnServiceTerminatedNormally(const ServiceProcessInfo& info) override {
    terminate_count_++;
  }
  void OnServiceCrashed(const ServiceProcessInfo& info) override {
    crash_count_++;
  }

  int launch_count_ = 0;
  int terminate_count_ = 0;
  int crash_count_ = 0;
};

class ServiceProcessObserverHubTest : public testing::Test {
 public:
  ServiceProcessObserverHubTest() = default;

 protected:
  // Calls through the ServiceProcessHost::Observer base interface, matching
  // how the service host invokes these methods in production.
  static ServiceProcessHost::Observer& AsHostObserver(
      ServiceProcessObserverHub<audio::mojom::AudioService>& hub) {
    return static_cast<ServiceProcessHost::Observer&>(hub);
  }

 private:
  BrowserTaskEnvironment task_environment_;
};

// Verifies that a duplicate termination notification (arriving after the hub
// already processed the first one) is ignored.
TEST_F(ServiceProcessObserverHubTest,
       DuplicateTerminationNotificationIsIgnored) {
  ServiceProcessObserverHub<audio::mojom::AudioService> hub;
  auto& host_observer = AsHostObserver(hub);
  TestObserver observer;
  hub.AddObserver(&observer);

  // Launch and terminate normally.
  ServiceProcessInfo info = MakeFakeInfo(1);
  host_observer.OnServiceProcessLaunched(info.Duplicate());
  EXPECT_EQ(1, observer.launch_count_);

  host_observer.OnServiceProcessTerminatedNormally(info.Duplicate());
  EXPECT_EQ(1, observer.terminate_count_);

  // A duplicate/stale termination arrives — should be ignored.
  host_observer.OnServiceProcessTerminatedNormally(info.Duplicate());
  EXPECT_EQ(1, observer.terminate_count_);

  hub.RemoveObserver(&observer);
}

// Same as above but for crash notifications.
TEST_F(ServiceProcessObserverHubTest, DuplicateCrashNotificationIsIgnored) {
  ServiceProcessObserverHub<audio::mojom::AudioService> hub;
  auto& host_observer = AsHostObserver(hub);
  TestObserver observer;
  hub.AddObserver(&observer);

  ServiceProcessInfo info = MakeFakeInfo(1);
  host_observer.OnServiceProcessLaunched(info.Duplicate());
  EXPECT_EQ(1, observer.launch_count_);

  host_observer.OnServiceProcessCrashed(info.Duplicate());
  EXPECT_EQ(1, observer.crash_count_);

  // A duplicate/stale crash arrives — should be ignored.
  host_observer.OnServiceProcessCrashed(info.Duplicate());
  EXPECT_EQ(1, observer.crash_count_);

  hub.RemoveObserver(&observer);
}

// Verifies that termination/crash with no tracked process is ignored.
TEST_F(ServiceProcessObserverHubTest, TerminationWithNoActiveProcessIsIgnored) {
  ServiceProcessObserverHub<audio::mojom::AudioService> hub;
  auto& host_observer = AsHostObserver(hub);
  TestObserver observer;
  hub.AddObserver(&observer);

  // No launch has occurred — terminate and crash should be no-ops.
  ServiceProcessInfo info = MakeFakeInfo(1);
  host_observer.OnServiceProcessTerminatedNormally(info.Duplicate());
  EXPECT_EQ(0, observer.terminate_count_);

  host_observer.OnServiceProcessCrashed(info.Duplicate());
  EXPECT_EQ(0, observer.crash_count_);

  hub.RemoveObserver(&observer);
}

// Simulates the race where the service pipe disconnect triggers a relaunch
// before the child process death notification arrives for the old instance.
// The stale crash notification for the old instance should be dropped.
TEST_F(ServiceProcessObserverHubTest, StaleCrashAfterRelaunchIsIgnored) {
  ServiceProcessObserverHub<audio::mojom::AudioService> hub;
  auto& host_observer = AsHostObserver(hub);
  TestObserver observer;
  hub.AddObserver(&observer);

  // First instance launches.
  ServiceProcessInfo first = MakeFakeInfo(1);
  host_observer.OnServiceProcessLaunched(first.Duplicate());
  EXPECT_EQ(1, observer.launch_count_);

  // Service pipe disconnects, caller relaunches before crash notification.
  // The hub receives a second OnServiceProcessLaunched (new instance).
  ServiceProcessInfo second = MakeFakeInfo(2);
  host_observer.OnServiceProcessLaunched(second.Duplicate());
  EXPECT_EQ(2, observer.launch_count_);

  // Stale crash notification for the first instance arrives — dropped.
  host_observer.OnServiceProcessCrashed(first.Duplicate());
  EXPECT_EQ(0, observer.crash_count_);

  // The second instance terminates normally — delivered.
  host_observer.OnServiceProcessTerminatedNormally(second.Duplicate());
  EXPECT_EQ(1, observer.terminate_count_);

  hub.RemoveObserver(&observer);
}

}  // namespace
}  // namespace content
