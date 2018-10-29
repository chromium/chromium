// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/utility_process_host_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

namespace {

void VerifyPowerStateInChildProcess(mojom::PowerMonitorTest* power_monitor_test,
                                    bool expected_state) {
  base::RunLoop run_loop;
  power_monitor_test->QueryNextState(base::BindOnce(
      [](const base::Closure& quit, bool expected_state,
         bool on_battery_power) {
        EXPECT_EQ(expected_state, on_battery_power);
        quit.Run();
      },
      run_loop.QuitClosure(), expected_state));
  run_loop.Run();
}

void StartUtilityProcessOnIOThread(mojom::PowerMonitorTestRequest request) {
  UtilityProcessHost* host =
      new UtilityProcessHost(/*client=*/nullptr,
                             /*client_task_runner=*/nullptr);
  host->SetMetricsName("test_process");
  host->SetName(base::ASCIIToUTF16("TestProcess"));
  EXPECT_TRUE(host->Start());

  BindInterface(host, std::move(request));
}

void BindInterfaceForGpuOnIOThread(mojom::PowerMonitorTestRequest request) {
  BindInterfaceInGpuProcess(std::move(request));
}

class MockPowerMonitorMessageBroadcaster : public device::mojom::PowerMonitor {
 public:
  MockPowerMonitorMessageBroadcaster() = default;
  ~MockPowerMonitorMessageBroadcaster() override = default;

  void Bind(device::mojom::PowerMonitorRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // device::mojom::PowerMonitor:
  void AddClient(
      device::mojom::PowerMonitorClientPtr power_monitor_client) override {
    power_monitor_client->PowerStateChange(on_battery_power_);
    clients_.AddPtr(std::move(power_monitor_client));
  }

  void OnPowerStateChange(bool on_battery_power) {
    on_battery_power_ = on_battery_power;
    clients_.ForAllPtrs(
        [&on_battery_power](device::mojom::PowerMonitorClient* client) {
          client->PowerStateChange(on_battery_power);
        });
  }

 private:
  bool on_battery_power_ = false;

  mojo::BindingSet<device::mojom::PowerMonitor> bindings_;
  mojo::InterfacePtrSet<device::mojom::PowerMonitorClient> clients_;

  DISALLOW_COPY_AND_ASSIGN(MockPowerMonitorMessageBroadcaster);
};

class PowerMonitorTest : public ContentBrowserTest {
 public:
  PowerMonitorTest() {
    // Because Device Service also runs in this process(browser process), we can
    // set our binder to intercept requests for PowerMonitor interface to it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::PowerMonitor::Name_,
        base::Bind(&PowerMonitorTest::BindPowerMonitor,
                   base::Unretained(this)));
  }

  ~PowerMonitorTest() override {
    service_manager::ServiceContext::ClearGlobalBindersForTesting(
        device::mojom::kServiceName);
  }

  void BindPowerMonitor(const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle handle,
                        const service_manager::BindSourceInfo& source_info) {
    if (source_info.identity.name() == mojom::kRendererServiceName) {
      // We can receive binding requests for the spare RenderProcessHost - this
      // might happen before the test has provided the
      // |renderer_bound_closure_|.
      if (renderer_bound_closure_) {
        ++request_count_from_renderer_;
        std::move(renderer_bound_closure_).Run();
      } else {
        DCHECK(RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
      }
    } else if (source_info.identity.name() == mojom::kUtilityServiceName) {
      // If the network service is enabled, it will create utility processes
      // without a utility closure.
      if (utility_bound_closure_) {
        ++request_count_from_utility_;
        std::move(utility_bound_closure_).Run();
      }
    } else if (source_info.identity.name() == mojom::kGpuServiceName) {
      ++request_count_from_gpu_;

      // We ignore null gpu_bound_closure_ here for two possible scenarios:
      //  - TestRendererProcess and TestUtilityProcess also result in spinning
      //    up GPU processes as a side effect, but they do not set valid
      //    gpu_bound_closure_.
      //  - As GPU process is started during setup of browser test suite, so
      //    it's possible that TestGpuProcess execution may have not started
      //    yet when the PowerMonitor bind request comes here, in such case
      //    gpu_bound_closure_ will also be null.
      if (gpu_bound_closure_)
        std::move(gpu_bound_closure_).Run();
    }

    power_monitor_message_broadcaster_.Bind(
        device::mojom::PowerMonitorRequest(std::move(handle)));
  }

 protected:
  void StartUtilityProcess(mojom::PowerMonitorTestPtr* power_monitor_test,
                           base::Closure utility_bound_closure) {
    utility_bound_closure_ = std::move(utility_bound_closure);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&StartUtilityProcessOnIOThread,
                       mojo::MakeRequest(power_monitor_test)));
  }

  void set_renderer_bound_closure(base::Closure closure) {
    renderer_bound_closure_ = std::move(closure);
  }

  void set_gpu_bound_closure(base::Closure closure) {
    gpu_bound_closure_ = std::move(closure);
  }

  int request_count_from_renderer() { return request_count_from_renderer_; }
  int request_count_from_utility() { return request_count_from_utility_; }
  int request_count_from_gpu() { return request_count_from_gpu_; }

  void SimulatePowerStateChange(bool on_battery_power) {
    power_monitor_message_broadcaster_.OnPowerStateChange(on_battery_power);
  }

 private:
  int request_count_from_renderer_ = 0;
  int request_count_from_utility_ = 0;
  int request_count_from_gpu_ = 0;
  base::OnceClosure renderer_bound_closure_;
  base::OnceClosure gpu_bound_closure_;
  base::OnceClosure utility_bound_closure_;

  MockPowerMonitorMessageBroadcaster power_monitor_message_broadcaster_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorTest);
};

IN_PROC_BROWSER_TEST_F(PowerMonitorTest, TestRendererProcess) {
  ASSERT_EQ(0, request_count_from_renderer());
  base::RunLoop run_loop;
  set_renderer_bound_closure(run_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  run_loop.Run();
  EXPECT_EQ(1, request_count_from_renderer());

  mojom::PowerMonitorTestPtr power_monitor_renderer;
  RenderProcessHost* rph =
      shell()->web_contents()->GetMainFrame()->GetProcess();
  BindInterface(rph, &power_monitor_renderer);

  SimulatePowerStateChange(true);
  // Verify renderer process on_battery_power changed to true.
  VerifyPowerStateInChildProcess(power_monitor_renderer.get(), true);

  SimulatePowerStateChange(false);
  // Verify renderer process on_battery_power changed to false.
  VerifyPowerStateInChildProcess(power_monitor_renderer.get(), false);
}

IN_PROC_BROWSER_TEST_F(PowerMonitorTest, TestUtilityProcess) {
  mojom::PowerMonitorTestPtr power_monitor_utility;

  ASSERT_EQ(0, request_count_from_utility());
  base::RunLoop run_loop;
  StartUtilityProcess(&power_monitor_utility, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1, request_count_from_utility());

  SimulatePowerStateChange(true);
  // Verify utility process on_battery_power changed to true.
  VerifyPowerStateInChildProcess(power_monitor_utility.get(), true);

  SimulatePowerStateChange(false);
  // Verify utility process on_battery_power changed to false.
  VerifyPowerStateInChildProcess(power_monitor_utility.get(), false);
}

IN_PROC_BROWSER_TEST_F(PowerMonitorTest, TestGpuProcess) {
  // As gpu process is started automatically during the setup period of browser
  // test suite, it may have already started and bound PowerMonitor interface to
  // Device Service before execution of this TestGpuProcess test. So here we
  // do not wait for the connection if we found it has already been established.
  if (request_count_from_gpu() != 1) {
    ASSERT_EQ(0, request_count_from_gpu());
    base::RunLoop run_loop;
    set_gpu_bound_closure(run_loop.QuitClosure());
    // Wait for the connection from gpu process.
    run_loop.Run();
  }
  EXPECT_EQ(1, request_count_from_gpu());

  mojom::PowerMonitorTestPtr power_monitor_gpu;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BindInterfaceForGpuOnIOThread,
                     mojo::MakeRequest(&power_monitor_gpu)));

  SimulatePowerStateChange(true);
  // Verify gpu process on_battery_power changed to true.
  VerifyPowerStateInChildProcess(power_monitor_gpu.get(), true);

  SimulatePowerStateChange(false);
  // Verify gpu process on_battery_power changed to false.
  VerifyPowerStateInChildProcess(power_monitor_gpu.get(), false);
}

}  //  namespace

}  //  namespace content
