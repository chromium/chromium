// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_desktop_environment.h"
#include <memory>

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/client_session_events.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using ::testing::Eq;

class FakeClientSessionControl : public ClientSessionControl {
 public:
  FakeClientSessionControl() = default;
  FakeClientSessionControl(const FakeClientSessionControl&) = delete;
  FakeClientSessionControl& operator=(const FakeClientSessionControl&) = delete;
  ~FakeClientSessionControl() override = default;

  // ClientSessionControl implementation:
  const std::string& client_jid() const override { return client_jid_; }
  void DisconnectSession(protocol::ErrorCode error) override {}
  void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                           ui::EventType type) override {}
  void OnLocalKeyPressed(uint32_t usb_keycode) override {}
  void SetDisableInputs(bool disable_inputs) override {}
  void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) override {}

  base::WeakPtr<FakeClientSessionControl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_ = "<fake-client-jid>";
  base::WeakPtrFactory<FakeClientSessionControl> weak_ptr_factory_{this};
};

class FakeClientSessionEvents : public ClientSessionEvents {
 public:
  FakeClientSessionEvents() = default;
  FakeClientSessionEvents(const FakeClientSessionEvents&) = delete;
  FakeClientSessionEvents& operator=(const FakeClientSessionEvents&) = delete;
  ~FakeClientSessionEvents() override = default;

  // ClientSessionEvents implementation:
  void OnDesktopAttached(uint32_t session_id) override {}
  void OnDesktopDetached() override {}

  base::WeakPtr<FakeClientSessionEvents> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeClientSessionEvents> weak_ptr_factory_{this};
};

class FakeSecurityCurtainController
    : public ash::curtain::SecurityCurtainController {
 public:
  FakeSecurityCurtainController() = default;
  FakeSecurityCurtainController(const FakeSecurityCurtainController&) = delete;
  FakeSecurityCurtainController& operator=(
      const FakeSecurityCurtainController&) = delete;
  ~FakeSecurityCurtainController() override = default;

  // ash::curtain::SecurityCurtainController implementation:
  void Enable() override {
    DCHECK(!is_enabled_);
    is_enabled_ = true;
  }
  void Disable() override {
    DCHECK(is_enabled_);
    is_enabled_ = false;
  }
  bool IsEnabled() const override { return is_enabled_; }

 private:
  bool is_enabled_ = false;
};

class It2MeDesktopEnvironmentTest : public ::testing::Test {
 public:
  It2MeDesktopEnvironmentTest() = default;
  ~It2MeDesktopEnvironmentTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  }

  DesktopEnvironmentOptions default_options() {
    DesktopEnvironmentOptions options;
    // These options must be false or we run into crashes in HostWindowProxy.
    options.set_enable_user_interface(false);
    options.set_enable_notifications(false);
    return options;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return environment_.GetMainThreadTaskRunner();
  }

  std::unique_ptr<It2MeDesktopEnvironment> Create(
      DesktopEnvironmentOptions options) {
    auto base_ptr = It2MeDesktopEnvironmentFactory(task_runner(), task_runner(),
                                                   task_runner(), task_runner())
                        .Create(session_control_.GetWeakPtr(),
                                session_events_.GetWeakPtr(), options);
    // Cast to It2MeDesktopEnvironment
    return std::unique_ptr<It2MeDesktopEnvironment>(
        static_cast<It2MeDesktopEnvironment*>(base_ptr.release()));
  }

  std::unique_ptr<It2MeDesktopEnvironment> CreateCurtainedSession() {
    DesktopEnvironmentOptions options(default_options());
    options.set_enable_curtaining(true);
    return Create(options);
  }

  void FlushUiSequence() {
    // In our test scenario all sequences are single threaded,
    // so to flush the UI sequence we can simply flush the main thread.
    environment_.RunUntilIdle();
  }

  ash::curtain::SecurityCurtainController& security_curtain_controller() {
    return security_curtain_controller_;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  base::test::ScopedFeatureList feature_list_;
  FakeClientSessionControl session_control_;
  FakeClientSessionEvents session_events_;
  FakeSecurityCurtainController security_curtain_controller_;
  test::ScopedFakeAshProxy ash_proxy_{&security_curtain_controller_};
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(It2MeDesktopEnvironmentTest,
       ShouldStartCurtainWhenEnableCurtainingIsTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(true);

  auto desktop_environment = Create(options);
  EXPECT_THAT(desktop_environment->is_curtained(), Eq(true));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotStartCurtainWhenEnableCurtainingIsFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(false);

  auto desktop_environment = Create(options);
  EXPECT_THAT(desktop_environment->is_curtained(), Eq(false));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotStartCurtainWhenCrdAdminRemoteAccessFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(true);

  auto desktop_environment = Create(options);
  EXPECT_THAT(desktop_environment->is_curtained(), Eq(false));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ACurtainedSessionShouldEnableSecurityCurtain) {
  ASSERT_THAT(security_curtain_controller().IsEnabled(), Eq(false));

  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(true));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ClosingACurtainedSessionShouldDisableSecurityCurtain) {
  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();

  // Closing the CRD session will destroy the desktop environment.
  desktop_environment.reset();
  FlushUiSequence();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace remoting
