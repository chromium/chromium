// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace extensions {

namespace {

constexpr char kTestAppId[] = "jabiebdnficieldhmegebckfhpfidfla";

class FakeCecPrivate : public crosapi::mojom::CecPrivate {
 public:
  FakeCecPrivate() = default;
  ~FakeCecPrivate() override = default;
  FakeCecPrivate(const FakeCecPrivate&) = delete;
  FakeCecPrivate& operator=(const FakeCecPrivate&) = delete;

  mojo::PendingRemote<crosapi::mojom::CecPrivate> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::CecPrivate overrides
  void SendStandBy(SendStandByCallback callback) override {
    stand_by_call_count_++;
    UpdateDisplayPowerStates(crosapi::mojom::PowerState::kStandBy);
    std::move(callback).Run();
  }

  void SendWakeUp(SendWakeUpCallback callback) override {
    wake_up_call_count_++;
    UpdateDisplayPowerStates(crosapi::mojom::PowerState::kOn);
    std::move(callback).Run();
  }
  void QueryDisplayCecPowerState(
      QueryDisplayCecPowerStateCallback callback) override {
    std::move(callback).Run(power_states_);
  }

  int stand_by_call_count() const { return stand_by_call_count_; }
  int wake_up_call_count() const { return wake_up_call_count_; }

  void set_power_states(
      const std::vector<crosapi::mojom::PowerState>& power_states) {
    power_states_ = power_states;
  }

  void UpdateDisplayPowerStates(crosapi::mojom::PowerState new_state) {
    for (size_t i = 0; i < power_states_.size(); i++) {
      power_states_[i] = new_state;
    }
  }

 private:
  mojo::Receiver<crosapi::mojom::CecPrivate> receiver_{this};
  int stand_by_call_count_ = 0;
  int wake_up_call_count_ = 0;
  std::vector<crosapi::mojom::PowerState> power_states_;
};

class CecPrivateKioskApiTest : public ShellApiTest {
 public:
  CecPrivateKioskApiTest()
      : session_type_(ScopedCurrentFeatureSessionType(
            mojom::FeatureSessionType::kKiosk)) {}

  CecPrivateKioskApiTest(const CecPrivateKioskApiTest&) = delete;
  CecPrivateKioskApiTest& operator=(const CecPrivateKioskApiTest&) = delete;

  ~CecPrivateKioskApiTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        receiver_.BindNewPipeAndPassRemote());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestAppId);
    ShellApiTest::SetUpCommandLine(command_line);
  }

 protected:
  FakeCecPrivate fake_cec_private_;

 private:
  std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>> session_type_;
  mojo::Receiver<crosapi::mojom::CecPrivate> receiver_{&fake_cec_private_};
};

using CecPrivateNonKioskApiTest = ShellApiTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(CecPrivateKioskApiTest, TestLacrosServiceAccess) {
  auto* service = chromeos::LacrosService::Get();
  EXPECT_NE(service, nullptr);
}

IN_PROC_BROWSER_TEST_F(CecPrivateKioskApiTest, TestCanFake) {
  EXPECT_TRUE(
      chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::CecPrivate>())
      << "the fake CecPrivate didn't really bind";
  EXPECT_NE(chromeos::LacrosService::Get()
                ->GetRemote<crosapi::mojom::CecPrivate>()
                .get(),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(CecPrivateKioskApiTest, TestAllApiFunctions) {
  fake_cec_private_.set_power_states(
      {crosapi::mojom::PowerState::kOn, crosapi::mojom::PowerState::kOn});
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener standby_call_count("standby_call_count",
                                                  ReplyBehavior::kWillReply);
  ExtensionTestMessageListener wakeup_call_count("wakeup_call_count",
                                                 ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadApp("api_test/cec_private/api"));

  ASSERT_TRUE(standby_call_count.WaitUntilSatisfied())
      << standby_call_count.message();
  standby_call_count.Reply(fake_cec_private_.stand_by_call_count());

  ASSERT_TRUE(wakeup_call_count.WaitUntilSatisfied())
      << wakeup_call_count.message();
  wakeup_call_count.Reply(fake_cec_private_.wake_up_call_count());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(CecPrivateNonKioskApiTest, TestCecPrivateNotAvailable) {
  ASSERT_TRUE(RunAppTest("api_test/cec_private/non_kiosk_api_not_available"))
      << message_;
}

}  // namespace extensions
