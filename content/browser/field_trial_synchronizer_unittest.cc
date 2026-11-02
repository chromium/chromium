// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_synchronizer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/runtime_field_trial_overrides.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/pass_key.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/hashing.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "content/common/renderer_variations_configuration.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsService {
 public:
  static base::PassKey<VariationsService> CreatePassKeyForTesting() {
    return base::PassKey<VariationsService>();
  }
};
}  // namespace variations

namespace content {
namespace {

class FieldTrialSynchronizerTest : public testing::Test {
 public:
  constexpr static int32_t kAllocatorMemorySize = 1 << 20;  // 1 MiB

  explicit FieldTrialSynchronizerTest(bool optimize_variations_ipc = true)
      : scoped_variations_ids_provider_(
            variations::VariationsIdsProvider::Mode::kUseSignedInState) {
    feature_list_.InitWithFeatureState(variations::kOptimizeVariationsIpc,
                                       optimize_variations_ipc);
    FieldTrialSynchronizer::CreateInstance();
  }
  ~FieldTrialSynchronizerTest() override {
    FieldTrialSynchronizer::DeleteInstanceForTesting();
  }

  void SetUp() override {
    testing::Test::SetUp();

    memory_allocator_ = std::make_unique<base::LocalPersistentMemoryAllocator>(
        kAllocatorMemorySize, 0, "");
    FieldTrialSynchronizer::RegisterPersistentAllocatorForTesting(
        memory_allocator_.get());
    metrics::SystemProfileProto profile;
    profile.set_client_uuid("id");
    std::string serialized_profile;
    profile.SerializeToString(&serialized_profile);
    FieldTrialSynchronizer::SetSystemProfileForTesting(serialized_profile,
                                                       false);
  }

  void TearDown() override {
    FieldTrialSynchronizer::DeregisterPersistentAllocatorForTesting(
        memory_allocator_.get());
    base::RuntimeFieldTrialOverrides::GetInstance()->ResetForTesting();
    testing::Test::TearDown();
  }

  metrics::SystemProfileProto GetSystemProfile() {
    metrics::SystemProfileProto profile;
    metrics::PersistentSystemProfile::GetSystemProfile(*memory_allocator_,
                                                       &profile);
    return profile;
  }

  bool ProfileHasFieldTrial(const std::string& trial,
                            const std::string& group) {
    metrics::SystemProfileProto profile = GetSystemProfile();
    uint64_t trial_hash = variations::HashName(trial);
    uint64_t group_hash = variations::HashName(group);
    for (int i = 0; i < profile.field_trial_size(); ++i) {
      if (profile.field_trial(i).name_id() == trial_hash &&
          profile.field_trial(i).group_id() == group_hash) {
        return true;
      }
    }
    return false;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_env_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_;
  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator_;
};

TEST_F(FieldTrialSynchronizerTest, RuntimeOverrides) {
  auto* overrides = base::RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  // Create an overridden field trial.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("OverriddenTrial", "Group");
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  trial->Activate();
  // Activating the trial should have added it to the profile.
  EXPECT_TRUE(ProfileHasFieldTrial("OverriddenTrial", "Group"));

  // Apply the runtime override, replacing the overridden trial, and activate
  // it.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled", base::FieldTrialParams(), trial.get()),
      ""));
  overrides->GetRuntimeOverride("Killswitch")->Activate();

  // The overridden trial should be removed from the profile, and the new
  // override added.
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  EXPECT_TRUE(ProfileHasFieldTrial("Killswitch", "Disabled"));

  // Now, apply another override targeting the same overridden trial, replacing
  // the previous override.
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "NewKillswitch", "NewDisabled", base::FieldTrialParams(),
          trial.get()),
      "Killswitch"));
  overrides->GetRuntimeOverride("NewKillswitch")->Activate();

  // The previous override "Killswitch" should be removed, and "NewKillswitch"
  // added.
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  EXPECT_FALSE(ProfileHasFieldTrial("Killswitch", "Disabled"));
  EXPECT_TRUE(ProfileHasFieldTrial("NewKillswitch", "NewDisabled"));
}

TEST_F(FieldTrialSynchronizerTest, InactiveRuntimeOverride) {
  auto* overrides = base::RuntimeFieldTrialOverrides::GetInstance();
  auto pass_key = variations::VariationsService::CreatePassKeyForTesting();

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("OverriddenTrial", "Group");
  trial->Activate();
  EXPECT_TRUE(ProfileHasFieldTrial("OverriddenTrial", "Group"));

  // Apply inactive runtime override (starts inactive).
  EXPECT_TRUE(overrides->ApplyRuntimeOverride(
      pass_key,
      std::make_unique<base::RuntimeFieldTrialInfo>(
          "Killswitch", "Disabled", base::FieldTrialParams(), trial.get()),
      ""));

  // The overridden trial should be removed from the profile, but the new
  // override should not be added yet since it's inactive.
  EXPECT_FALSE(ProfileHasFieldTrial("OverriddenTrial", "Group"));
  EXPECT_FALSE(ProfileHasFieldTrial("Killswitch", "Disabled"));

  // Activating the override adds it to the profile.
  overrides->GetRuntimeOverride("Killswitch")->Activate();
  EXPECT_TRUE(ProfileHasFieldTrial("Killswitch", "Disabled"));
}

class TestVariationsClient : public variations::VariationsClient {
 public:
  // variations::VariationsClient:
  bool IsOffTheRecord() const override { return false; }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(/*is_signed_in=*/false);
  }
};

class VariationsTestBrowserContext : public TestBrowserContext {
 public:
  // BrowserContext:
  variations::VariationsClient* GetVariationsClient() override {
    return &variations_client_;
  }

 private:
  TestVariationsClient variations_client_;
};

class ChannelMockRenderProcessHost
    : public MockRenderProcessHost,
      public mojom::RendererVariationsConfiguration {
 public:
  explicit ChannelMockRenderProcessHost(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
    ResetChannelProxy();
    Init();
  }

  ~ChannelMockRenderProcessHost() override = default;

  void ResetChannelProxy() {
    receivers_.Clear();
    mojo::MessagePipe pipe;
    auto io_task_runner = GetIOThreadTaskRunner({});
    auto ui_task_runner = GetUIThreadTaskRunner({});
    server_channel_ = IPC::ChannelProxy::Create(
        std::move(pipe.handle0), IPC::Channel::MODE_SERVER,
        /*listener=*/nullptr, io_task_runner, ui_task_runner);
    client_channel_ = IPC::ChannelProxy::Create(
        std::move(pipe.handle1), IPC::Channel::MODE_CLIENT,
        /*listener=*/this, io_task_runner, ui_task_runner);
  }

  // MockRenderProcessHost:
  IPC::ChannelProxy* GetChannel() override { return server_channel_.get(); }

  // IPC::Listener:
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    if (interface_name == mojom::RendererVariationsConfiguration::Name_) {
      ++interface_request_count_;
      receivers_.Add(
          this, mojo::PendingAssociatedReceiver<
                    mojom::RendererVariationsConfiguration>(std::move(handle)));
    }
  }

  // mojom::RendererVariationsConfiguration:
  void SetVariationsHeaders(
      variations::mojom::VariationsHeadersPtr variations_headers) override {
    ++set_headers_count_;
    if (on_message_received_) {
      on_message_received_.Run();
    }
  }
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override {
    ++set_field_trial_count_;
    if (on_message_received_) {
      on_message_received_.Run();
    }
  }

  void WaitUntilMessagesReceived(int expected_headers_count,
                                 int expected_field_trial_count) {
    while (set_headers_count_ < expected_headers_count ||
           set_field_trial_count_ < expected_field_trial_count) {
      base::RunLoop run_loop;
      on_message_received_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    on_message_received_.Reset();
  }

  int interface_request_count() const { return interface_request_count_; }
  int set_headers_count() const { return set_headers_count_; }
  int set_field_trial_count() const { return set_field_trial_count_; }

 private:
  std::unique_ptr<IPC::ChannelProxy> server_channel_;
  std::unique_ptr<IPC::ChannelProxy> client_channel_;
  mojo::AssociatedReceiverSet<mojom::RendererVariationsConfiguration>
      receivers_;
  base::RepeatingClosure on_message_received_;
  int interface_request_count_ = 0;
  int set_headers_count_ = 0;
  int set_field_trial_count_ = 0;
};

class RespawningProcessExitObserver : public RenderProcessHostObserver {
 public:
  explicit RespawningProcessExitObserver(ChannelMockRenderProcessHost* host)
      : host_(host) {
    observation_.Observe(host);
  }
  ~RespawningProcessExitObserver() override = default;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    // Synchronously respawn the renderer inside `RenderProcessExited()`,
    // mirroring `RenderProcessHostImpl::Init()` being invoked from an observer
    // during `ProcessDied()`. The cached `RendererVariationsConfiguration`
    // remote must already have been reset before observers run so this update
    // binds to the new channel instead of the dead one.
    host_->ResetChannelProxy();
    host_->Init();
    FieldTrialSynchronizer::UpdateRendererVariationsHeader(host_);
  }

 private:
  raw_ptr<ChannelMockRenderProcessHost> host_;
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_{this};
};

TEST_F(FieldTrialSynchronizerTest,
       CachesRendererVariationsConfigurationAndResetsOnProcessExit) {
  VariationsTestBrowserContext browser_context;
  ChannelMockRenderProcessHost host(&browser_context);

  FieldTrialSynchronizer::UpdateRendererVariationsHeader(&host);
  FieldTrialSynchronizer::UpdateRendererVariationsHeader(&host);

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("CachedRemoteTrial", "GroupA");
  trial->Activate();
  host.WaitUntilMessagesReceived(/*expected_headers_count=*/2,
                                 /*expected_field_trial_count=*/1);

  // All three IPCs (two header updates + one field trial update) should share
  // a single bound `RendererVariationsConfiguration` associated interface.
  EXPECT_EQ(1, host.interface_request_count());
  EXPECT_EQ(2, host.set_headers_count());
  EXPECT_EQ(1, host.set_field_trial_count());

  // Simulate the renderer crashing while a `RenderProcessHostObserver`
  // synchronously respawns the renderer inside `RenderProcessExited()`.
  // Because `RenderProcessHost` resets its cached remote before notifying
  // `RenderProcessExited()` observers, the respawned renderer receives its
  // initial headers on a newly bound interface.
  RespawningProcessExitObserver respawn_observer(&host);
  host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 1);
  host.WaitUntilMessagesReceived(/*expected_headers_count=*/3,
                                 /*expected_field_trial_count=*/1);

  EXPECT_EQ(2, host.interface_request_count());
  EXPECT_EQ(3, host.set_headers_count());
}

class FieldTrialSynchronizerDisabledFeatureTest
    : public FieldTrialSynchronizerTest {
 public:
  FieldTrialSynchronizerDisabledFeatureTest()
      : FieldTrialSynchronizerTest(/*optimize_variations_ipc=*/false) {}
};

TEST_F(FieldTrialSynchronizerDisabledFeatureTest,
       BindsRendererVariationsConfigurationPerUpdateWhenFeatureDisabled) {
  VariationsTestBrowserContext browser_context;
  ChannelMockRenderProcessHost host(&browser_context);

  FieldTrialSynchronizer::UpdateRendererVariationsHeader(&host);
  FieldTrialSynchronizer::UpdateRendererVariationsHeader(&host);

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("UncachedRemoteTrial", "GroupB");
  trial->Activate();
  host.WaitUntilMessagesReceived(/*expected_headers_count=*/2,
                                 /*expected_field_trial_count=*/1);

  EXPECT_EQ(3, host.interface_request_count());
  EXPECT_EQ(2, host.set_headers_count());
  EXPECT_EQ(1, host.set_field_trial_count());
}

}  // namespace
}  // namespace content
