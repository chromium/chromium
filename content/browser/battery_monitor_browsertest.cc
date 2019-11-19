// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/battery_status.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace content {

namespace {

class MockBatteryMonitor : public device::mojom::BatteryMonitor {
 public:
  MockBatteryMonitor() = default;
  ~MockBatteryMonitor() override = default;

  void Bind(mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

  void DidChange(const device::mojom::BatteryStatus& battery_status) {
    status_ = battery_status;
    status_to_report_ = true;

    if (!callback_.is_null())
      ReportStatus();
  }

 private:
  // mojom::BatteryMonitor methods:
  void QueryNextStatus(QueryNextStatusCallback callback) override {
    if (!callback_.is_null()) {
      DVLOG(1) << "Overlapped call to QueryNextStatus!";
      receiver_.reset();
      return;
    }
    callback_ = std::move(callback);

    if (status_to_report_)
      ReportStatus();
  }

  void ReportStatus() {
    std::move(callback_).Run(status_.Clone());
    status_to_report_ = false;
  }

  QueryNextStatusCallback callback_;
  device::mojom::BatteryStatus status_;
  bool status_to_report_ = false;
  mojo::Receiver<device::mojom::BatteryMonitor> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockBatteryMonitor);
};

class BatteryMonitorTest : public ContentBrowserTest {
 public:
  BatteryMonitorTest() {
    mock_battery_monitor_ = std::make_unique<MockBatteryMonitor>();
    // Because Device Service also runs in this process(browser process), here
    // we can directly set our binder to intercept interface requests against
    // it.
    service_manager::ServiceBinding::OverrideInterfaceBinderForTesting(
        device::mojom::kServiceName,
        base::BindRepeating(&MockBatteryMonitor::Bind,
                            base::Unretained(mock_battery_monitor_.get())));
  }

  ~BatteryMonitorTest() override {
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::BatteryMonitor>(device::mojom::kServiceName);
  }

 protected:
  MockBatteryMonitor* mock_battery_monitor() {
    return mock_battery_monitor_.get();
  }

 private:
  std::unique_ptr<MockBatteryMonitor> mock_battery_monitor_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorTest);
};

IN_PROC_BROWSER_TEST_F(BatteryMonitorTest, NavigatorGetBatteryInfo) {
  // From JavaScript request a promise for the battery status information and
  // once it resolves check the values and navigate to #pass.
  device::mojom::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  mock_battery_monitor()->DidChange(status);

  GURL test_url = GetTestUrl("battery_monitor",
                             "battery_status_promise_resolution_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(BatteryMonitorTest, NavigatorGetBatteryListenChange) {
  // From JavaScript request a promise for the battery status information.
  // Once it resolves add an event listener for battery level change. Set
  // battery level to 0.6 and invoke update. Check that the event listener
  // is invoked with the correct value for level and navigate to #pass.
  device::mojom::BatteryStatus status;
  mock_battery_monitor()->DidChange(status);

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);
  GURL test_url =
      GetTestUrl("battery_monitor", "battery_status_event_listener_test.html");
  shell()->LoadURL(test_url);
  same_tab_observer.Wait();
  EXPECT_EQ("resolved", shell()->web_contents()->GetLastCommittedURL().ref());

  TestNavigationObserver same_tab_observer2(shell()->web_contents(), 1);
  status.level = 0.6;
  mock_battery_monitor()->DidChange(status);
  same_tab_observer2.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
