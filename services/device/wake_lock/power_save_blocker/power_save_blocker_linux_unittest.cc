// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/switches.h"

namespace device {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kInhibitInterface[] = "org.freedesktop.portal.Inhibit";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kConnectionName[] = ":1.2";
constexpr char kDBusServiceName[] = "org.freedesktop.DBus";
constexpr char kDBusObjectPath[] = "/org/freedesktop/DBus";

// Inhibit flags shared by org.freedesktop.portal.Inhibit and
// org.gnome.SessionManager.
constexpr uint32_t kInhibitSuspend = 4;
constexpr uint32_t kInhibitIdle = 8;

// A TestScreen that tracks the number of outstanding screensaver suspensions.
class ScreenSaverTrackingScreen : public display::test::TestScreen {
 public:
  ScreenSaverTrackingScreen()
      : display::test::TestScreen(/*create_display=*/true,
                                  /*register_screen=*/true) {}
  ~ScreenSaverTrackingScreen() override = default;

  int suspend_count() const { return suspend_count_; }

  // display::Screen:
  std::unique_ptr<ScreenSaverSuspender> SuspendScreenSaver() override {
    return std::make_unique<Suspender>(suspend_count_);
  }

 private:
  class Suspender : public ScreenSaverSuspender {
   public:
    explicit Suspender(int& count) : count_(count) { ++*count_; }
    ~Suspender() override { --*count_; }

   private:
    const raw_ref<int> count_;
  };

  int suspend_count_ = 0;
};

class PowerSaveBlockerLinuxTest : public testing::Test {
 protected:
  PowerSaveBlockerLinuxTest() {
    // Skip the D-Bus inhibit calls, which aren't available in tests. The
    // screensaver suspension is still exercised.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kHeadless);
  }

  std::unique_ptr<PowerSaveBlocker> CreateBlocker(mojom::WakeLockType type) {
    return std::make_unique<PowerSaveBlocker>(
        type, mojom::WakeLockReason::kOther, "test",
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  // PowerSaveBlocker posts its work to the current sequence, so a task posted
  // afterwards runs once that work has completed.
  void FlushTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedCommandLine scoped_command_line_;
  ScreenSaverTrackingScreen screen_;
};

// dbus::MockBus doesn't mock RemoveObjectProxy().
class TestBus : public dbus::MockBus {
 public:
  using dbus::MockBus::MockBus;
  MOCK_METHOD(bool,
              RemoveObjectProxy,
              (std::string_view, const dbus::ObjectPath&, base::OnceClosure),
              (override));

 private:
  ~TestBus() override = default;
};

// Exercises the D-Bus paths with a mock session bus.
class PowerSaveBlockerLinuxDBusTest : public testing::Test {
 protected:
  // Arguments of a portal Inhibit() call.
  struct PortalInhibitCall {
    std::string window;
    uint32_t flags = 0;
    std::string reason;
  };

  PowerSaveBlockerLinuxDBusTest()
      : mock_bus_(base::MakeRefCounted<TestBus>(dbus::Bus::Options())),
        portal_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath(kPortalObjectPath))),
        request_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1_2/t"))),
        dbus_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(),
            kDBusServiceName,
            dbus::ObjectPath(kDBusObjectPath))) {
    PowerSaveBlocker::SetBusForTesting(mock_bus_);
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);

    ON_CALL(*mock_bus_, GetConnectionName())
        .WillByDefault(Return(kConnectionName));
    ON_CALL(*mock_bus_, RemoveObjectProxy(kPortalServiceName, _, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath& path,
                              base::OnceClosure) {
          removed_request_paths_.push_back(path);
          return true;
        });
    // Request objects. The more specific expectation below takes precedence.
    ON_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath& path) {
          requested_request_paths_.push_back(path);
          return request_proxy_.get();
        });
    ON_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName,
                                       dbus::ObjectPath(kPortalObjectPath)))
        .WillByDefault(Return(portal_proxy_.get()));
    ON_CALL(*mock_bus_,
            GetObjectProxy(kDBusServiceName, dbus::ObjectPath(kDBusObjectPath)))
        .WillByDefault(Return(dbus_proxy_.get()));

    ON_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault([this](dbus::MethodCall* call, int,
                              dbus::ObjectProxy::ResponseOrErrorCallback
                                  callback) {
          EXPECT_EQ(call->GetInterface(), kInhibitInterface);
          EXPECT_EQ(call->GetMember(), "Inhibit");
          dbus::MessageReader reader(call);
          PortalInhibitCall inhibit_call;
          EXPECT_TRUE(reader.PopString(&inhibit_call.window));
          EXPECT_TRUE(reader.PopUint32(&inhibit_call.flags));
          auto options =
              dbus_utils::ReadValue<std::map<std::string, dbus_utils::Variant>>(
                  reader);
          EXPECT_TRUE(options.has_value());
          if (options) {
            auto it = options->find("reason");
            if (it != options->end()) {
              inhibit_call.reason =
                  std::move(it->second).Take<std::string>().value_or("");
            }
            it = options->find("handle_token");
            if (it != options->end()) {
              request_path_ =
                  dbus::ObjectPath(base::nix::XdgDesktopPortalRequestPath(
                      kConnectionName,
                      std::move(it->second).Take<std::string>().value_or("")));
            }
          }
          EXPECT_TRUE(request_path_.IsValid());
          portal_inhibit_call_ = std::move(inhibit_call);
          portal_inhibit_callback_ = std::move(callback);
        });
    ON_CALL(*request_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* call, int,
                              dbus::ObjectProxy::ResponseCallback) {
          EXPECT_EQ(call->GetInterface(), kRequestInterface);
          EXPECT_EQ(call->GetMember(), "Close");
          ++close_count_;
        });
    ON_CALL(*request_proxy_,
            ConnectToSignal(kRequestInterface, "Response", _, _))
        .WillByDefault(
            [this](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              response_signal_callback_ = std::move(signal_callback);
              ++response_signal_connect_count_;
              std::move(connected_callback)
                  .Run(interface_name, signal_name, true);
            });
    ON_CALL(*dbus_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* call, int,
                              dbus::ObjectProxy::ResponseCallback) {
          EXPECT_EQ(call->GetMember(), "NameHasOwner");
          dbus::MessageReader reader(call);
          std::string name;
          EXPECT_TRUE(reader.PopString(&name));
          name_has_owner_queries_.push_back(name);
        });
  }

  // Blockers destroyed at the end of a test clean up asynchronously, and the
  // cleanup releases the screensaver suspension owned by `screen_`.
  void TearDown() override {
    FlushTasks();
    // Only the Request object named by the handle_token, or the one returned
    // by an old portal, is used.
    for (const auto& path : requested_request_paths_) {
      EXPECT_TRUE(path == request_path_ ||
                  (reply_request_path_ && path == *reply_request_path_))
          << path.value();
    }
  }

  ~PowerSaveBlockerLinuxDBusTest() override {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
    PowerSaveBlocker::SetBusForTesting(nullptr);
  }

  std::unique_ptr<PowerSaveBlocker> CreateBlocker(mojom::WakeLockType type) {
    return std::make_unique<PowerSaveBlocker>(
        type, mojom::WakeLockReason::kOther, "test",
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  // PowerSaveBlocker posts its work to the current sequence, and the mock bus
  // runs D-Bus calls synchronously, so a task posted afterwards runs once that
  // work has completed.
  void FlushTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Replies to the pending portal Inhibit() call with the Request handle (or
  // `reply_request_path_` if set), or with an error if `success` is false.
  void RespondToPortalInhibit(bool success = true) {
    ASSERT_FALSE(portal_inhibit_callback_.is_null());
    if (!success) {
      std::move(portal_inhibit_callback_).Run(nullptr, nullptr);
    } else {
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendObjectPath(reply_request_path_.value_or(request_path_));
      std::move(portal_inhibit_callback_).Run(response.get(), nullptr);
    }
    FlushTasks();
  }

  // Emits the Request's Response signal with `response_code`.
  void EmitPortalResponse(uint32_t response_code) {
    ASSERT_FALSE(response_signal_callback_.is_null());
    dbus::Signal signal(kRequestInterface, "Response");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(response_code);
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("{sv}", &array_writer);
    writer.CloseContainer(&array_writer);
    response_signal_callback_.Run(&signal);
    FlushTasks();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  ScreenSaverTrackingScreen screen_;
  scoped_refptr<TestBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> request_proxy_;
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy_;

  std::optional<PortalInhibitCall> portal_inhibit_call_;
  dbus::ObjectProxy::ResponseOrErrorCallback portal_inhibit_callback_;
  dbus::ObjectProxy::SignalCallback response_signal_callback_;
  dbus::ObjectPath request_path_;
  // Simulates a portal older than 0.9, which ignores handle_token.
  std::optional<dbus::ObjectPath> reply_request_path_;
  int response_signal_connect_count_ = 0;
  std::vector<dbus::ObjectPath> requested_request_paths_;
  // Request object proxies removed from the bus. The mock request proxy
  // never runs Close() callbacks, so removals after Close() aren't included.
  std::vector<dbus::ObjectPath> removed_request_paths_;
  int close_count_ = 0;
  std::vector<std::string> name_has_owner_queries_;
};

}  // namespace

// kPreventAppSuspension (eg. audio-only playback) must not suspend the
// screensaver, otherwise the display is prevented from dimming and sleeping.
// On Wayland this creates a zwp_idle_inhibitor_v1. https://crbug.com/570067499
TEST_F(PowerSaveBlockerLinuxTest, AppSuspensionDoesNotSuspendScreenSaver) {
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventAppSuspension);
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

TEST_F(PowerSaveBlockerLinuxTest, DisplaySleepSuspendsScreenSaver) {
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_EQ(1, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

TEST_F(PowerSaveBlockerLinuxTest, DisplaySleepAllowDimmingSuspendsScreenSaver) {
  auto blocker =
      CreateBlocker(mojom::WakeLockType::kPreventDisplaySleepAllowDimming);
  FlushTasks();
  EXPECT_EQ(1, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

TEST_F(PowerSaveBlockerLinuxDBusTest, SandboxedUsesPortal) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  ASSERT_TRUE(portal_inhibit_call_);
  EXPECT_EQ(portal_inhibit_call_->window, "");
  EXPECT_EQ(portal_inhibit_call_->flags, kInhibitIdle);
  EXPECT_EQ(portal_inhibit_call_->reason, "test");

  RespondToPortalInhibit();
  // xdg-desktop-portal also emits a successful Response signal.
  EmitPortalResponse(0);
  EXPECT_EQ(close_count_, 0);
  EXPECT_TRUE(name_has_owner_queries_.empty());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 1);
  EXPECT_TRUE(name_has_owner_queries_.empty());
}

TEST_F(PowerSaveBlockerLinuxDBusTest, SandboxedAppSuspensionInhibitsSuspend) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kSnap);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventAppSuspension);
  FlushTasks();
  ASSERT_TRUE(portal_inhibit_call_);
  EXPECT_EQ(portal_inhibit_call_->flags, kInhibitSuspend);

  RespondToPortalInhibit();
  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 1);
}

TEST_F(PowerSaveBlockerLinuxDBusTest, NotSandboxedSkipsPortal) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kNone);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_FALSE(portal_inhibit_call_);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});
}

TEST_F(PowerSaveBlockerLinuxDBusTest, FeatureDisabledSkipsPortal) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWakeLockInhibitPortal);
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_FALSE(portal_inhibit_call_);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});
}

TEST_F(PowerSaveBlockerLinuxDBusTest, PortalUnavailableFallsBack) {
  dbus_xdg::SetPortalInterfaceVersionForTesting(kInhibitInterface, 0);
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_FALSE(portal_inhibit_call_);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});
}

TEST_F(PowerSaveBlockerLinuxDBusTest, PortalErrorFallsBack) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  ASSERT_TRUE(portal_inhibit_call_);
  EXPECT_TRUE(name_has_owner_queries_.empty());

  RespondToPortalInhibit(/*success=*/false);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});
  EXPECT_EQ(removed_request_paths_,
            std::vector<dbus::ObjectPath>{request_path_});

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 0);
}

// If the blocker is destroyed while the Inhibit() call is in flight, the
// inhibition is released as soon as the reply arrives.
TEST_F(PowerSaveBlockerLinuxDBusTest, PortalReplyAfterDestructionIsReleased) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  ASSERT_TRUE(portal_inhibit_call_);

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 0);

  RespondToPortalInhibit();
  EXPECT_EQ(close_count_, 1);
  EXPECT_TRUE(name_has_owner_queries_.empty());
  // The proxy is removed once Close() completes.
  EXPECT_TRUE(removed_request_paths_.empty());
}

// The portal reports a backend failure in the Response signal after Inhibit()
// has returned the handle.
TEST_F(PowerSaveBlockerLinuxDBusTest, PortalResponseFailureFallsBack) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  RespondToPortalInhibit();
  EXPECT_TRUE(name_has_owner_queries_.empty());

  EmitPortalResponse(2);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});

  // The failed Request is already gone, so it isn't closed.
  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 0);
}

TEST_F(PowerSaveBlockerLinuxDBusTest,
       PortalResponseFailureBeforeReplyFallsBack) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EmitPortalResponse(2);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});

  // The reply to the failed request is ignored.
  RespondToPortalInhibit();
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});
  EXPECT_EQ(removed_request_paths_,
            std::vector<dbus::ObjectPath>{request_path_});

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 0);
}

// Portals older than 0.9 ignore handle_token and return a Request at another
// path. The Response signal is watched there, and that Request is closed.
TEST_F(PowerSaveBlockerLinuxDBusTest, OldPortalRequestPathIsUsed) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  reply_request_path_ =
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1_2/old");
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_EQ(response_signal_connect_count_, 1);

  RespondToPortalInhibit();
  EXPECT_EQ(response_signal_connect_count_, 2);
  ASSERT_FALSE(requested_request_paths_.empty());
  EXPECT_EQ(requested_request_paths_.back(), *reply_request_path_);
  // The proxy at the path derived from handle_token is no longer needed.
  EXPECT_EQ(removed_request_paths_,
            std::vector<dbus::ObjectPath>{request_path_});

  requested_request_paths_.clear();
  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 1);
  EXPECT_EQ(requested_request_paths_,
            std::vector<dbus::ObjectPath>{*reply_request_path_});
}

TEST_F(PowerSaveBlockerLinuxDBusTest, OldPortalResponseFailureFallsBack) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  reply_request_path_ =
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1_2/old");
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  RespondToPortalInhibit();
  EXPECT_TRUE(name_has_owner_queries_.empty());

  EmitPortalResponse(2);
  EXPECT_EQ(name_has_owner_queries_,
            std::vector<std::string>{"org.gnome.SessionManager"});

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(close_count_, 0);
}

// If the blocker is destroyed while the Inhibit() call is in flight and an old
// portal returns another path, that Request is closed.
TEST_F(PowerSaveBlockerLinuxDBusTest,
       OldPortalReplyAfterDestructionIsReleased) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  reply_request_path_ =
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1_2/old");
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  blocker.reset();
  FlushTasks();

  requested_request_paths_.clear();
  RespondToPortalInhibit();
  EXPECT_EQ(close_count_, 1);
  EXPECT_EQ(requested_request_paths_,
            std::vector<dbus::ObjectPath>{*reply_request_path_});
  EXPECT_EQ(removed_request_paths_,
            std::vector<dbus::ObjectPath>{request_path_});
}

// If the blocker is destroyed while the Inhibit() call is in flight and the
// call fails, nothing is closed, and the Request proxy is removed.
TEST_F(PowerSaveBlockerLinuxDBusTest, PortalErrorAfterDestruction) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  blocker.reset();
  FlushTasks();

  RespondToPortalInhibit(/*success=*/false);
  EXPECT_EQ(close_count_, 0);
  EXPECT_TRUE(name_has_owner_queries_.empty());
  EXPECT_EQ(removed_request_paths_,
            std::vector<dbus::ObjectPath>{request_path_});
}

}  // namespace device
