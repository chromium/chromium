// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/common/features.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using RequestWorkletServiceOutcome =
    AuctionProcessManager::RequestWorkletServiceOutcome;

// Alias constants to improve readability.
const size_t kMaxSellerProcesses = AuctionProcessManager::kMaxSellerProcesses;
const size_t kMaxBidderProcesses = AuctionProcessManager::kMaxBidderProcesses;

base::OnceClosure NeverInvokedClosure() {
  return base::BindOnce([]() { ADD_FAILURE() << "This should not be called"; });
}

template <class AuctionManagerBaseType>
class TestAuctionProcessManager
    : public AuctionManagerBaseType,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  TestAuctionProcessManager() = default;

  TestAuctionProcessManager(const TestAuctionProcessManager&) = delete;
  const TestAuctionProcessManager& operator=(const TestAuctionProcessManager&) =
      delete;

  ~TestAuctionProcessManager() override = default;

  void SetTrustedSignalsCache(
      mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCache>
          trusted_signals_cache) override {}

  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& script_source_url,
      const std::optional<GURL>& bidding_wasm_helper_url,
      const std::optional<GURL>& trusted_bidding_signals_url,
      const std::string& trusted_bidding_signals_slot_size_param,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
          permissions_policy_state,
      std::optional<uint16_t> experiment_id,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    NOTREACHED_IN_MIGRATION();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet,
      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& script_source_url,
      const std::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
          permissions_policy_state,
      std::optional<uint16_t> experiment_id,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ClosePipes() {
    receiver_set_.Clear();
    // No wait to flush a closed pipe from the end that was closed. Run until
    // the other side has noticed the pipe was closed instead.
    base::RunLoop().RunUntilIdle();
  }

  // Get the index that this process was created (e.g. if it was the first
  // created process, return 0). Returns -1 and has an EXPECT failure if the
  // process wasn't found or has since been destroyed. `handle` must have been
  // assigned a WorkletProcess.
  size_t ProcessCreationOrder(
      const AuctionProcessManager::ProcessHandle& handle) {
    EXPECT_TRUE(handle.worklet_process_for_testing());
    for (size_t i = 0u; i < launched_processes_.size(); ++i) {
      if (handle.worklet_process_for_testing() ==
          launched_processes_[i].get()) {
        return i;
      }
    }
    ADD_FAILURE() << "Worklet process not found";
    return -1;
  }

  // Simulate readiness of the creation_index'th launched process.
  // This function will return fail if there is no ith process.
  // Do not call this function after any process in the test has been
  // destroyed.
  //
  // Only works for the dedicated process case. In the non-dedicated case, these
  // should go through the MockRenderProcessHost.
  void SimulateReadyProcess(size_t creation_index) {
    if constexpr (std::is_same<AuctionManagerBaseType,
                               InRendererAuctionProcessManager>::value) {
      // This should not be used in the  InRendererAuctionProcessManager case.
      NOTREACHED();
      return;
    }
    if (launched_processes_.size() <= creation_index) {
      ADD_FAILURE() << "Process unexpectedly doesn't exist: " << creation_index;
      return;
    }
    launched_processes_[creation_index]->OnLaunchedWithProcess(
        base::Process::Current());
    return;
  }

 private:
  AuctionProcessManager::WorkletProcess::ProcessContext CreateProcessInternal(
      AuctionProcessManager::WorkletProcess& worklet_process) override {
    launched_processes_.emplace_back(worklet_process.GetWeakPtrForTesting());
    if constexpr (std::is_same<AuctionManagerBaseType,
                               InRendererAuctionProcessManager>::value) {
      // The MockRenderProcessHost drops Mojo pipes on the floor by default, so
      // need to set a binder so `this` can intercept them and bind them to
      // `receiver_set_`, while still exercising the production
      // CreateProcessInternal() call.
      static_cast<MockRenderProcessHost*>(
          worklet_process.site_instance()->GetProcess())
          ->OverrideBinderForTesting(
              auction_worklet::mojom::AuctionWorkletService::Name_,
              base::BindRepeating(&TestAuctionProcessManager<
                                      AuctionManagerBaseType>::BindInterface,
                                  weak_ptr_factory_.GetWeakPtr()));
      // Defer to the RendererProcessHost mocks when using the InRenderer path.
      return AuctionManagerBaseType::CreateProcessInternal(worklet_process);
    } else {
      mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService>
          service;
      receiver_set_.Add(this, service.InitWithNewPipeAndPassReceiver());
      return AuctionProcessManager::WorkletProcess::ProcessContext(
          std::move(service));
    }
  }

  // Callback when trying to bind a pipe through the MockRenderProcessHost.
  void BindInterface(mojo::ScopedMessagePipeHandle pipe) {
    receiver_set_.Add(
        this,
        mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>(
            std::move(pipe)));
  }

  std::vector<base::WeakPtr<AuctionProcessManager::WorkletProcess>>
      launched_processes_;

  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;

  base::WeakPtrFactory<TestAuctionProcessManager<AuctionManagerBaseType>>
      weak_ptr_factory_{this};
};

// ContentBrowserClient to disable strict site isolation.
class PartialSiteIsolationContentBrowserClient
    : public TestContentBrowserClient {
 public:
  bool ShouldEnableStrictSiteIsolation() override { return false; }

  bool ShouldDisableSiteIsolation(
      SiteIsolationMode site_isolation_mode) override {
    switch (site_isolation_mode) {
      case SiteIsolationMode::kStrictSiteIsolation:
        return true;
      case SiteIsolationMode::kPartialSiteIsolation:
        return false;
    }
  }
};

// The three ways the base test fixture can be configured.
enum class ProcessMode {
  // Use DedicatedAuctionProcessManager.
  kDedicated,
  // Use InRendererProcessManager and kSitePerProcess.
  kInRendererSitePerProcess,
  // Use InRendererProcessManager and disabled kSitePerProcess.
  kInRendererSharedProcess,
};

class AuctionProcessManagerTest
    : public testing::TestWithParam<
          std::tuple<AuctionProcessManager::WorkletType, ProcessMode>> {
 protected:
  AuctionProcessManagerTest() {
    SiteIsolationPolicy::DisableFlagCachingForTesting();
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {features::kFledgeStartAnticipatoryProcesses,
         {{"AnticipatoryProcessHoldTime", "10s"}}}};
    std::vector<base::test::FeatureRef> disabled_features;
    switch (GetProcessMode()) {
      case ProcessMode::kDedicated:
        dedicated_process_manager_.emplace();
        auction_process_manager_ = &dedicated_process_manager_.value();
        break;
      case ProcessMode::kInRendererSitePerProcess:
        scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
            switches::kSitePerProcess);
        in_renderer_process_manager_.emplace();
        auction_process_manager_ = &in_renderer_process_manager_.value();
        break;
      case ProcessMode::kInRendererSharedProcess:
        enabled_features.emplace_back(
            features::kProcessSharingWithDefaultSiteInstances,
            base::FieldTrialParams());
        disabled_features.emplace_back(
            features::kProcessSharingWithStrictSiteInstances);
        disabled_features.emplace_back(
            features::kOriginKeyedProcessesByDefault);
        scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
            switches::kSitePerProcess);
        original_browser_client_ =
            content::SetBrowserClientForTesting(&browser_client_);
        in_renderer_process_manager_.emplace();
        auction_process_manager_ = &in_renderer_process_manager_.value();
        break;
    }
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &rph_factory_);
    // Note: if we're going to disable kOriginKeyedProcessesByDefault, as is
    // done in the kInRendererSharedProcess case, it's important to do it here
    // before we create any SiteInstances, since that will create
    // BrowsingInstances, and each BrowsingInstance will create a default
    // isolation state based on kOriginKeyedProcessesByDefault.
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    // This StartIsolatingSite() call should be done before any SiteInstances
    // are created, so that it applies to them.
    SiteInstance::StartIsolatingSite(
        &test_browser_context_, kIsolatedOrigin.GetURL(),
        ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

    site_instance1_ = SiteInstance::Create(&test_browser_context_);
    site_instance2_ = SiteInstance::Create(&test_browser_context_);
  }

  virtual ~AuctionProcessManagerTest() {
    if (original_browser_client_) {
      content::SetBrowserClientForTesting(original_browser_client_);
    }
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
  }

  // Closes all worklet pipes, much like a crash.
  void ClosePipes() {
    if (dedicated_process_manager_) {
      dedicated_process_manager_->ClosePipes();
    } else {
      in_renderer_process_manager_->ClosePipes();
    }
  }

  // Wraps calling ProcessCreationOrder() on the correct
  // TestAuctionProcessManager.
  size_t ProcessCreationOrder(
      const AuctionProcessManager::ProcessHandle& handle) {
    if (dedicated_process_manager_) {
      return dedicated_process_manager_->ProcessCreationOrder(handle);
    } else {
      return in_renderer_process_manager_->ProcessCreationOrder(handle);
    }
  }

  // Currently only works when testing the dedicated path.
  void SimulateReadyProcess(size_t creation_index) {
    CHECK(dedicated_process_manager_);
    dedicated_process_manager_->SimulateReadyProcess(creation_index);
  }

  void MaybeStartAnticipatoryProcess(
      const url::Origin& origin,
      std::optional<AuctionProcessManager::WorkletType> worklet_type =
          std::nullopt) {
    auction_process_manager_->MaybeStartAnticipatoryProcess(
        origin, site_instance1_.get(), worklet_type.value_or(GetWorkletType()));
  }

  std::string RequestWorkletServiceOutcomeUmaName(
      std::optional<AuctionProcessManager::WorkletType> worklet_type =
          std::nullopt) {
    return base::StrCat({"Ads.InterestGroup.Auction.",
                         worklet_type.value_or(GetWorkletType()) ==
                                 AuctionProcessManager::WorkletType::kSeller
                             ? "Seller."
                             : "Buyer.",
                         "RequestWorkletServiceOutcome"});
  }

  void RequestWorkletService(
      AuctionProcessManager::ProcessHandle* process_handle,
      const url::Origin& origin,
      AuctionProcessManager::WorkletType worklet_type,
      bool expect_success,
      RequestWorkletServiceOutcome expected_outcome) {
    base::HistogramTester histogram_tester;
    bool success = auction_process_manager_->RequestWorkletService(
        worklet_type, origin, site_instance1_.get(), process_handle,
        base::DoNothing());
    EXPECT_EQ(expect_success, success);
    histogram_tester.ExpectUniqueSample(
        RequestWorkletServiceOutcomeUmaName(worklet_type), expected_outcome,
        1u);
  }

  // Request a worklet service and expect the request to complete synchronously.
  // There's no async version, since async calls are only triggered by deleting
  // another handle. Uses `site_instance1_` if no `site_instance` is provided.
  std::unique_ptr<AuctionProcessManager::ProcessHandle>
  GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> site_instance = nullptr) {
    if (!site_instance) {
      site_instance = site_instance1_;
    }
    auto process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        worklet_type, origin, std::move(site_instance), process_handle.get(),
        NeverInvokedClosure()));
    EXPECT_TRUE(process_handle->GetService());
    return process_handle;
  }

  // Requests a process of type GetWorkletType().
  std::unique_ptr<AuctionProcessManager::ProcessHandle> GetServiceExpectSuccess(
      const url::Origin& origin) {
    return GetServiceOfTypeExpectSuccess(GetWorkletType(), origin);
  }

  // Returns the maximum number of processes of type GetWorkletType().
  size_t GetMaxProcesses() const {
    switch (GetWorkletType()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return kMaxSellerProcesses;
      case AuctionProcessManager::WorkletType::kBidder:
        return kMaxBidderProcesses;
    }
  }

  ProcessMode GetProcessMode() const {
    return std::get<ProcessMode>(GetParam());
  }

  AuctionProcessManager::WorkletType GetWorkletType() const {
    return std::get<AuctionProcessManager::WorkletType>(GetParam());
  }

  AuctionProcessManager::WorkletType GetOtherWorkletType() const {
    switch (GetWorkletType()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return AuctionProcessManager::WorkletType::kBidder;
      case AuctionProcessManager::WorkletType::kBidder:
        return AuctionProcessManager::WorkletType::kSeller;
    }
  }

  // Returns the number of pending requests of GetWorkletType() type.
  size_t GetPendingRequestsOfWorkletType() {
    switch (GetWorkletType()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return auction_process_manager_->GetPendingSellerRequestsForTesting();
      case AuctionProcessManager::WorkletType::kBidder:
        return auction_process_manager_->GetPendingBidderRequestsForTesting();
    }
  }

  // Returns active processes of GetWorkletType() by default.
  size_t GetActiveProcessesOfWorkletType(
      std::optional<AuctionProcessManager::WorkletType> type = std::nullopt) {
    switch (type.value_or(GetWorkletType())) {
      case AuctionProcessManager::WorkletType::kSeller:
        return auction_process_manager_->GetSellerProcessCountForTesting();
      case AuctionProcessManager::WorkletType::kBidder:
        return auction_process_manager_->GetBidderProcessCountForTesting();
    }
  }

  void CheckOnlyIdleProcessesWithCount(size_t expected_idle_process_count) {
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
              expected_idle_process_count);
    EXPECT_EQ(auction_process_manager_->GetBidderProcessCountForTesting(), 0u);
    EXPECT_EQ(auction_process_manager_->GetSellerProcessCountForTesting(), 0u);
  }

  // Isolated by StartIsolatingSite() call in the constructor.
  const url::Origin kIsolatedOrigin =
      url::Origin::Create(GURL("https://bank.test"));

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
  const url::Origin kOriginC = url::Origin::Create(GURL("https://c.test"));

  BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;

  // `scoped_command_line_` must be destroyeded after any WorkletProcessManager.
  // Otherwise idle worklet processes may try to read it after destruction.
  base::test::ScopedCommandLine scoped_command_line_;

  MockRenderProcessHostFactory rph_factory_;

  TestBrowserContext test_browser_context_;

  // Used by the kInRendererSharedProcess case to disable strict site isolation.
  PartialSiteIsolationContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_browser_client_;

  // `site_instance1_` and `site_instance2_` are in different browsing
  // instances.
  scoped_refptr<SiteInstance> site_instance1_;
  scoped_refptr<SiteInstance> site_instance2_;

  // Only one of these two is populated, based on the ProcessMode.
  std::optional<TestAuctionProcessManager<DedicatedAuctionProcessManager>>
      dedicated_process_manager_;
  std::optional<TestAuctionProcessManager<InRendererAuctionProcessManager>>
      in_renderer_process_manager_;

  // Points to whichever of the above is non-null.
  raw_ptr<AuctionProcessManager> auction_process_manager_;
};

// Run most tests in both kDedicated and kInRendererSitePerProcess modes, as
// their behavior should be very similar in most cases.
INSTANTIATE_TEST_SUITE_P(
    All,
    AuctionProcessManagerTest,
    testing::Combine(
        testing::Values(AuctionProcessManager::WorkletType::kSeller,
                        AuctionProcessManager::WorkletType::kBidder),
        testing::Values(ProcessMode::kDedicated,
                        ProcessMode::kInRendererSitePerProcess)));

TEST_P(AuctionProcessManagerTest, Basic) {
  auto worklet = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(worklet->GetService());
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());
  EXPECT_EQ(0u, GetActiveProcessesOfWorkletType(GetOtherWorkletType()));
  EXPECT_EQ(0u, auction_process_manager_->GetIdleProcessCountForTesting());
}

// Make sure requests for different origins don't share processes, nor do
// sellers and bidders.
TEST_P(AuctionProcessManagerTest, MultipleRequestsForDifferentProcesses) {
  auto worlket_a = GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA);
  auto worklet_b = GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginB);
  auto worklet_of_other_type_a =
      GetServiceOfTypeExpectSuccess(GetOtherWorkletType(), kOriginA);
  auto worklet_of_other_type_b =
      GetServiceOfTypeExpectSuccess(GetOtherWorkletType(), kOriginB);

  EXPECT_EQ(2u, GetActiveProcessesOfWorkletType(
                    AuctionProcessManager::WorkletType::kBidder));
  EXPECT_EQ(2u, GetActiveProcessesOfWorkletType(
                    AuctionProcessManager::WorkletType::kSeller));
  EXPECT_EQ(0u, auction_process_manager_->GetIdleProcessCountForTesting());
  EXPECT_NE(worlket_a->GetService(), worklet_b->GetService());
  EXPECT_NE(worlket_a->GetService(), worklet_of_other_type_a->GetService());
  EXPECT_NE(worlket_a->GetService(), worklet_of_other_type_b->GetService());
  EXPECT_NE(worklet_b->GetService(), worklet_of_other_type_a->GetService());
  EXPECT_NE(worklet_b->GetService(), worklet_of_other_type_b->GetService());
  EXPECT_NE(worklet_of_other_type_a->GetService(),
            worklet_of_other_type_b->GetService());
}

TEST_P(AuctionProcessManagerTest, MultipleRequestsForSameProcess) {
  // Request 3 processes of the same type for the same origin. All requests
  // should get the same process.
  auto process_a1 = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(process_a1->GetService());
  auto process_a2 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a2->GetService());
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());
  auto process_a3 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a3->GetService());
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());

  // Request process of the other type with the same origin. It should get a
  // different process.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> other_process_a1 =
      GetServiceOfTypeExpectSuccess(GetOtherWorkletType(), kOriginA);
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType(GetOtherWorkletType()));
  EXPECT_NE(process_a1->GetService(), other_process_a1->GetService());
}

// Test requesting and releasing worklet processes, exceeding the limit. This
// test does not cover the case of multiple requests sharing the same process,
// which is covered by the next test.
TEST_P(AuctionProcessManagerTest, LimitExceeded) {
  // The list of operations below assumes at least 3 processes are allowed at
  // once.
  CHECK_GE(GetMaxProcesses(), 3u);

  // Operations applied to the process manager. All requests use unique origins,
  // so no need to specify that.
  struct Operation {
    enum class Op {
      // Request the specified number of handle. If there are less than
      // GetMaxProcesses() handles already, expects a process to be immediately
      // assigned. All requests use different origins from every other request.
      kRequestHandles,

      // Destroy a handle with the given index. If the index is less than
      // GetMaxProcesses(), then expect a ProcessHandle to have its callback
      // invoked, if there are more than GetMaxProcesses() already.
      kDestroyHandle,

      // Same as destroy handle, but additionally destroys the next handle that
      // would have been assigned the next available process slot, and makes
      // sure the handle after that one gets a process instead.
      kDestroyHandleAndNextInQueue,
    };

    Op op;

    // Number of handles to request for kRequestHandles operations.
    std::optional<size_t> num_handles;

    // Used for kDestroyHandle and kDestroyHandleAndNextInQueue operations.
    std::optional<size_t> index;

    // The number of total handles expected after this operation. This can be
    // inferred by sum of requested handles requests less handles destroyed
    // handles, but having it explcitly in the struct makes sure the test cases
    // are testing what they're expected to.
    size_t expected_total_handles;

    // If `num_handles` is set, this represents whether each request caused us
    // to hit the limit for the number of processes.
    std::vector<bool> hit_limit_after_requesting_handles = {};
  };

  const Operation kOperationList[] = {
      {Operation::Op::kRequestHandles,
       /*num_handles=*/GetMaxProcesses(),
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false, false, false}},

      // Check destroying intermediate, last, and first handle when there are no
      // queued requests. Keep exactly GetMaxProcesses() requests, to ensure
      // there are in fact first, last, and intermediate requests (as long as
      // GetMaxProcesses() is at least 3).
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/1u, /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/0u, /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() - 1,
       /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},

      // Queue 3 more requests, but delete the last and first of them, to test
      // deleting queued requests.
      {Operation::Op::kRequestHandles,
       /*num_handles=*/3,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/GetMaxProcesses() +
           3, /*hit_limit_after_requesting_handles=*/
       {true, true, true}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses(),
       /*expected_total_handles=*/GetMaxProcesses() + 2},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() + 1,
       /*expected_total_handles=*/GetMaxProcesses() + 1},

      // Request 4 more processes.
      {Operation::Op::kRequestHandles,
       /*num_handles=*/4,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/GetMaxProcesses() +
           5, /*hit_limit_after_requesting_handles=*/
       {true, true, true, true}},

      // Destroy the first handle and the first pending in the queue immediately
      // afterwards. The next pending request should get a process.
      {Operation::Op::kDestroyHandleAndNextInQueue,
       /*num_handles=*/std::nullopt, /*index=*/0u,
       /*expected_total_handles=*/GetMaxProcesses() + 3},

      // Destroy three more requests that have been asssigned processes, being
      // sure to destroy the first, last, and some request request with nether,
      // amongst requests with assigned processes.
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() - 1,
       /*expected_total_handles=*/GetMaxProcesses() + 2},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/0u, /*expected_total_handles=*/GetMaxProcesses() + 1},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/1u, /*expected_total_handles=*/GetMaxProcesses()},
  };

  struct ProcessHandleData {
    std::unique_ptr<AuctionProcessManager::ProcessHandle> process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  };

  std::vector<ProcessHandleData> data;

  // Used to create distinct origins for each handle
  int num_origins = 0;
  for (const auto& operation : kOperationList) {
    switch (operation.op) {
      case Operation::Op::kRequestHandles:
        for (size_t i = 0; i < *operation.num_handles; ++i) {
          size_t original_size = data.size();
          data.emplace_back(ProcessHandleData());
          url::Origin distinct_origin = url::Origin::Create(
              GURL(base::StringPrintf("https://%i.test", ++num_origins)));
          base::HistogramTester histogram_tester;
          ASSERT_EQ(original_size < GetMaxProcesses(),
                    auction_process_manager_->RequestWorkletService(
                        GetWorkletType(), distinct_origin, site_instance1_,
                        data.back().process_handle.get(),
                        data.back().run_loop->QuitClosure()));
          RequestWorkletServiceOutcome expected_result =
              operation.hit_limit_after_requesting_handles[i]
                  ? RequestWorkletServiceOutcome::kHitProcessLimit
                  : RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess;
          histogram_tester.ExpectUniqueSample(
              RequestWorkletServiceOutcomeUmaName(), expected_result,
              /*expected_bucket_count=*/1);
        }
        break;

      case Operation::Op::kDestroyHandle: {
        size_t original_size = data.size();

        ASSERT_GT(data.size(), *operation.index);
        data.erase(data.begin() + *operation.index);
        // If destroying one of the first GetMaxProcesses() handles, and
        // there were more than GetMaxProcesses() handles before, the
        // first of the handles waiting on a process should get a process.
        if (*operation.index < GetMaxProcesses() &&
            original_size > GetMaxProcesses()) {
          data[GetMaxProcesses() - 1].run_loop->Run();
          EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        }
        break;
      }

      case Operation::Op::kDestroyHandleAndNextInQueue: {
        ASSERT_GT(data.size(), *operation.index);
        ASSERT_GT(data.size(), GetMaxProcesses() + 1);

        data.erase(data.begin() + *operation.index);
        data.erase(data.begin() + GetMaxProcesses());
        data[GetMaxProcesses() - 1].run_loop->Run();
        EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        break;
      }
    }

    EXPECT_EQ(operation.expected_total_handles, data.size());

    // The first GetMaxProcesses() ProcessHandles should all have
    // assigned processes, which should all be distinct.
    for (size_t i = 0; i < data.size() && i < GetMaxProcesses(); ++i) {
      EXPECT_TRUE(data[i].process_handle->GetService());
      for (size_t j = 0; j < i; ++j) {
        EXPECT_NE(data[i].process_handle->GetService(),
                  data[j].process_handle->GetService());
      }
    }

    // Make sure all pending tasks have been run.
    base::RunLoop().RunUntilIdle();

    // All other requests should not have been assigned processes yet.
    for (size_t i = GetMaxProcesses(); i < data.size(); ++i) {
      EXPECT_FALSE(data[i].run_loop->AnyQuitCalled());
      EXPECT_FALSE(data[i].process_handle->GetService());
    }
  }
}

// Check the process sharing logic - specifically, that requests share processes
// when origins match, and that handles that share a process only count once
// towrads the process limit the process limit.
TEST_P(AuctionProcessManagerTest, ProcessSharing) {
  // This test assumes GetMaxProcesses() is greater than 1.
  DCHECK_GT(GetMaxProcesses(), 1u);

  // Make 2*GetMaxProcesses() requests for each of GetMaxProcesses() different
  // origins. All requests should succeed immediately.
  std::vector<std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>>
      processes(GetMaxProcesses());
  for (size_t origin_index = 0; origin_index < GetMaxProcesses();
       ++origin_index) {
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://%zu.test", origin_index)));
    base::HistogramTester histogram_tester;
    for (size_t i = 0; i < 2 * GetMaxProcesses(); ++i) {
      processes[origin_index].emplace_back(GetServiceExpectSuccess(origin));
      // All requests for the same origin share a process.
      EXPECT_EQ(processes[origin_index].back()->GetService(),
                processes[origin_index].front()->GetService());
      EXPECT_EQ(origin_index + 1, GetActiveProcessesOfWorkletType());
    }
    histogram_tester.ExpectBucketCount(
        RequestWorkletServiceOutcomeUmaName(),
        RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 1);
    histogram_tester.ExpectBucketCount(
        RequestWorkletServiceOutcomeUmaName(),
        RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess,
        2 * GetMaxProcesses() - 1);

    // Each origin should have a different process.
    for (size_t origin_index2 = 0; origin_index2 < origin_index;
         ++origin_index2) {
      EXPECT_NE(processes[origin_index].front()->GetService(),
                processes[origin_index2].front()->GetService());
    }
  }

  // Make two process requests for kOriginA and one one for kOriginB, which
  // should all be blocked due to the process limit being reached.

  base::RunLoop run_loop_delayed_a1;
  auto process_delayed_a1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginA, site_instance1_, process_delayed_a1.get(),
      run_loop_delayed_a1.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());

  base::RunLoop run_loop_delayed_a2;
  auto process_delayed_a2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginA, site_instance1_, process_delayed_a2.get(),
      run_loop_delayed_a2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());

  base::RunLoop run_loop_delayed_b;
  auto process_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginB, site_instance1_, process_delayed_b.get(),
      run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());

  // Release processes for first origin one at a time, until only one is left.
  // The pending requests for kOriginA and kOriginB should remain stalled.
  while (processes[0].size() > 1u) {
    processes[0].pop_front();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a1->GetService());
    EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a2->GetService());
    EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_b->GetService());
    EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());
  }

  // Remove the final process for the first origin. It should queue a callback
  // to resume the kOriginA requests (prioritized alphabetically), but nothing
  // should happen until the callbacks are invoked.
  processes[0].pop_front();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // The two kOriginA callbacks should be invoked when the message loop next
  // spins. The two kOriginA requests should now have been assigned the same
  // service, while the kOriginB request is still pending.
  run_loop_delayed_a1.Run();
  run_loop_delayed_a2.Run();
  EXPECT_TRUE(process_delayed_a1->GetService());
  EXPECT_TRUE(process_delayed_a2->GetService());
  EXPECT_EQ(process_delayed_a1->GetService(), process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());

  // Freeing one of the two kOriginA processes should have no effect.
  process_delayed_a2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // Freeing the other one should queue a task to give the kOriginB requests a
  // process.
  process_delayed_a1.reset();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  run_loop_delayed_b.Run();
  EXPECT_TRUE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), GetActiveProcessesOfWorkletType());
}

TEST_P(AuctionProcessManagerTest, DestroyHandlesWithPendingRequests) {
  // Make GetMaxProcesses() requests for worklets with different origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> processes;
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    processes.emplace_back(GetServiceExpectSuccess(origin));
  }

  // Make a pending request.
  auto pending_process1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginA, site_instance1_, pending_process1.get(),
      NeverInvokedClosure()));
  EXPECT_EQ(1u, GetPendingRequestsOfWorkletType());

  // Destroy the pending request. Its callback should not be invoked.
  pending_process1.reset();
  EXPECT_EQ(0u, GetPendingRequestsOfWorkletType());
  base::RunLoop().RunUntilIdle();

  // Make two more pending process requests.
  auto pending_process2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginA, site_instance1_, pending_process2.get(),
      NeverInvokedClosure()));
  auto pending_process3 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  base::RunLoop pending_process3_run_loop;
  ASSERT_FALSE(auction_process_manager_->RequestWorkletService(
      GetWorkletType(), kOriginB, site_instance1_, pending_process3.get(),
      pending_process3_run_loop.QuitClosure()));
  EXPECT_EQ(2u, GetPendingRequestsOfWorkletType());

  // Delete a process. This should result in a posted task to give
  // `pending_process2` a process.
  processes.pop_front();
  EXPECT_EQ(1u, GetPendingRequestsOfWorkletType());

  // Destroy `pending_process2` before it gets passed a process.
  pending_process2.reset();

  // `pending_process3` should get a process instead.
  pending_process3_run_loop.Run();
  EXPECT_TRUE(pending_process3->GetService());
  EXPECT_EQ(0u, auction_process_manager_->GetPendingSellerRequestsForTesting());
}

// Check that process crash is handled properly, by creating a new process.
TEST_P(AuctionProcessManagerTest, ProcessCrash) {
  auto process = GetServiceExpectSuccess(kOriginA);
  auction_worklet::mojom::AuctionWorkletService* service =
      process->GetService();
  EXPECT_TRUE(service);
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());

  // Close pipes. No new pipe should be created.
  ClosePipes();
  EXPECT_EQ(0u, GetActiveProcessesOfWorkletType());

  // Requesting a new process will create a new pipe.
  auto process2 = GetServiceExpectSuccess(kOriginA);
  auction_worklet::mojom::AuctionWorkletService* service2 =
      process2->GetService();
  EXPECT_TRUE(service2);
  EXPECT_NE(service, service2);
  EXPECT_NE(process, process2);
  EXPECT_EQ(1u, GetActiveProcessesOfWorkletType());
}

TEST_P(AuctionProcessManagerTest, DisconnectBeforeDelete) {
  // Exercise the codepath where the mojo pipe to a service is broken when
  // a handle to its process is still alive, to make sure this is handled
  // correctly (rather than hitting a DCHECK on incorrect refcounting).
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceExpectSuccess(kOriginA);
  ClosePipes();
  task_environment_.RunUntilIdle();
  handle_a1.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(AuctionProcessManagerTest,
       DoesNotStartAnticipatoryProcessIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kFledgeStartAnticipatoryProcesses);
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(0);
}

TEST_P(AuctionProcessManagerTest,
       ProcessLimitIsRespected_AnticipatoryProcessesOnly) {
  // Create the maximum possible # of anticipatory processes.
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i)));
    MaybeStartAnticipatoryProcess(origin, GetWorkletType());
    CheckOnlyIdleProcessesWithCount(1 + i);
  }
  // We can't make more.
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(GetMaxProcesses());
}

// Make sure the process limit is respected when we have a combination of
// anticipatory and active processes. Make sure we can make processes of
// the other type (active and idle) even if we've hit the limit of one type.
TEST_P(AuctionProcessManagerTest,
       ProcessLimitIsRespected_ActiveAndAnticipatoryProcesses) {
  // Alternate creating anticipatory and active processes. Each active processes
  // will consume 1 anticipatory process. After the for loop, we end up with 1
  // anticipatory process and GetMaxProcesses() - 1 active processes.
  MaybeStartAnticipatoryProcess(url::Origin::Create(GURL("https://0.test")),
                                GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>> handles;
  for (size_t i = 0; i < GetMaxProcesses() - 1; ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i)));
    handles.emplace_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    RequestWorkletService(handles.back().get(), origin, GetWorkletType(),
                          /*expect_success=*/true,
                          RequestWorkletServiceOutcome::kUsedIdleProcess);
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), handles.size());
    origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i + 1)));
    MaybeStartAnticipatoryProcess(origin, GetWorkletType());
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), handles.size());
  }

  // Can't make more anticipatory processes of this type.
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), GetMaxProcesses() - 1);

  // Can make an anticipatory process of the other type.
  MaybeStartAnticipatoryProcess(kOriginB, GetOtherWorkletType());
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 2u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), GetMaxProcesses() - 1);

  // We should still be able to create another worklet with the
  // anticipatory process we made of this type.
  handles.emplace_back(
      std::make_unique<AuctionProcessManager::ProcessHandle>());
  RequestWorkletService(
      handles.back().get(),
      url::Origin::Create(
          GURL(base::StringPrintf("https://%i.test", GetMaxProcesses() - 1))),
      GetWorkletType(),
      /*expect_success=*/true, RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), GetMaxProcesses());

  // Can't make more processes of this type.
  handles.emplace_back(
      std::make_unique<AuctionProcessManager::ProcessHandle>());
  RequestWorkletService(handles.back().get(), kOriginB, GetWorkletType(),
                        /*expect_success=*/false,
                        RequestWorkletServiceOutcome::kHitProcessLimit);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), GetMaxProcesses());

  // Create a handle that should be backed by the anticipatory process of the
  // other type for kOriginB, created three code blocks back.
  handles.emplace_back(
      std::make_unique<AuctionProcessManager::ProcessHandle>());
  if (GetProcessMode() == ProcessMode::kDedicated) {
    // If using the dedicated process manager, the origin doesn't need to match
    // that of the idle process to reuse it, so can request a service for
    // kOriginC to get the WorkletProcess for kOriginB.
    RequestWorkletService(handles.back().get(), kOriginC, GetOtherWorkletType(),
                          /*expect_success=*/true,
                          RequestWorkletServiceOutcome::kUsedIdleProcess);
  } else {
    // In the in-renderer case, have to request the same origin to use the
    // anticipatory process.
    RequestWorkletService(handles.back().get(), kOriginB, GetOtherWorkletType(),
                          /*expect_success=*/true,
                          RequestWorkletServiceOutcome::kUsedIdleProcess);
  }
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), GetMaxProcesses());

  handles.clear();
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 0u);
  // Now we should no longer be at the process limit. We can make more
  // processes.
  handles.emplace_back(
      std::make_unique<AuctionProcessManager::ProcessHandle>());
  RequestWorkletService(
      handles.back().get(), url::Origin::Create(GURL("https://worklet2.test")),
      GetWorkletType(),
      /*expect_success=*/true,
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  MaybeStartAnticipatoryProcess(
      url::Origin::Create(GURL("https://worklet3.test")), GetWorkletType());
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
}

TEST_P(AuctionProcessManagerTest,
       DoNotStartMultipleProcessesSameOriginAndType) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
}

TEST_P(AuctionProcessManagerTest,
       CanStartProcessesWithSameOriginIfOneIsSellerAndOneIsBuyer) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  MaybeStartAnticipatoryProcess(kOriginA, GetOtherWorkletType());
  CheckOnlyIdleProcessesWithCount(2);
}

TEST_P(AuctionProcessManagerTest,
       DoNotStartProcessWithSameOriginAndTypeAsExistingProcess) {
  AuctionProcessManager::ProcessHandle process_handle;
  RequestWorkletService(
      &process_handle, kOriginA, GetWorkletType(), /*expect_success=*/true,
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
}

// This test covers the different behavior when there's an unused anticipatory
// process created with varying parameters.
TEST_P(AuctionProcessManagerTest,
       TryToUseAnticipatoryProcessOfSameOrDifferentOriginAndType) {
  for (const auto& origin_to_request : {kOriginA, kOriginB}) {
    SCOPED_TRACE(origin_to_request);
    for (AuctionProcessManager::WorkletType worklet_type_to_request :
         {GetWorkletType(), GetOtherWorkletType()}) {
      SCOPED_TRACE(static_cast<int>(worklet_type_to_request));
      MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
      CheckOnlyIdleProcessesWithCount(1);
      EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);

      if (GetProcessMode() == ProcessMode::kDedicated ||
          (origin_to_request == kOriginA &&
           worklet_type_to_request == GetWorkletType())) {
        // In the dedicated case, or the in-renderer case where the origin and
        // worklet types both match, requesting a process should result in using
        // the idle process.
        AuctionProcessManager::ProcessHandle handle;
        RequestWorkletService(&handle, origin_to_request,
                              worklet_type_to_request,
                              /*expect_success=*/true,
                              RequestWorkletServiceOutcome::kUsedIdleProcess);
        EXPECT_EQ(GetActiveProcessesOfWorkletType(worklet_type_to_request), 1u);
        EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
                  0u);
      } else {
        // In the other case, a new process should be created, leaving the
        // anticipatory process idle.
        AuctionProcessManager::ProcessHandle handle;
        RequestWorkletService(
            &handle, origin_to_request, worklet_type_to_request,
            /*expect_success=*/true,
            RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess);
        EXPECT_EQ(GetActiveProcessesOfWorkletType(worklet_type_to_request), 1u);
        // The antipatory process should still exist and be idle.
        EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
                  1u);

        // Make a request matching the parameters of the anticipatory process.
        AuctionProcessManager::ProcessHandle handle2;
        RequestWorkletService(&handle2, kOriginA, GetWorkletType(),
                              /*expect_success=*/true,
                              RequestWorkletServiceOutcome::kUsedIdleProcess);
        // There should be no more idle processes.
        EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
                  0u);
      }

      // There should be no processes, idle or otherwise, at this point, so the
      // manager is in a clean state for the next loop iteration.
      CheckOnlyIdleProcessesWithCount(0);
    }
  }
}

TEST_P(AuctionProcessManagerTest,
       ReassignsOrDestroysIdleProcessOfSameTypeOnlyAfterReachingLimit) {
  // Make an anticipatory process of the other type. This will not be
  // convertible to a process of our type after we hit the limit.
  MaybeStartAnticipatoryProcess(kOriginA, GetOtherWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i)));
    MaybeStartAnticipatoryProcess(origin, GetWorkletType());
    CheckOnlyIdleProcessesWithCount(2 + i);
  }
  // Try assigning these to different origins.
  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>> handles;
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i_2.test", i)));
    handles.emplace_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    if (GetProcessMode() == ProcessMode::kDedicated) {
      // We should assign the oldest anticipatory process of the same type
      // because we've hit the process limit -- we'd prefer to assign a newer
      // anticipatory process than to use the older process & have to remove one
      // of our anticipatory processes. All anticipatory processes were of type
      // GetWorkletType() except the first one.
      RequestWorkletService(handles.back().get(), origin, GetWorkletType(),
                            /*expect_success=*/true,
                            RequestWorkletServiceOutcome::kUsedIdleProcess);
      EXPECT_EQ(ProcessCreationOrder(*handles.back()), i + 1u);
    } else {
      // In the renderer case, processes could not be used for another origin,
      // so a new process was created, and an old idle process destroyed.
      RequestWorkletService(
          handles.back().get(), origin, GetWorkletType(),
          /*expect_success=*/true,
          RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess);
      EXPECT_EQ(ProcessCreationOrder(*handles.back()),
                GetMaxProcesses() + i + 1u);
    }
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), i + 1u);
  }
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
}

TEST_P(AuctionProcessManagerTest,
       ProcessesCanBeAssignedInDifferentOrderFromHowTheyWereMade) {
  std::vector<std::tuple<url::Origin, AuctionProcessManager::WorkletType>>
      origins_and_types;
  for (url::Origin origin : {kOriginA, kOriginB, kOriginC}) {
    for (AuctionProcessManager::WorkletType type :
         {GetWorkletType(), GetOtherWorkletType()}) {
      origins_and_types.emplace_back(origin, type);
      MaybeStartAnticipatoryProcess(origin, type);
    }
  }

  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>> handles;
  for (size_t i = 0; i < origins_and_types.size(); i++) {
    size_t inverse_index = origins_and_types.size() - 1 - i;
    const auto& origin_and_type = origins_and_types[inverse_index];
    std::unique_ptr<AuctionProcessManager::ProcessHandle> handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    RequestWorkletService(
        handle.get(), std::get<url::Origin>(origin_and_type),
        std::get<AuctionProcessManager::WorkletType>(origin_and_type),
        /*expect_success=*/true,
        RequestWorkletServiceOutcome::kUsedIdleProcess);
    if (GetProcessMode() == ProcessMode::kDedicated) {
      // We assigned the oldest available idle process.
      EXPECT_EQ(ProcessCreationOrder(*handle), i);
    } else {
      // We assigned process created with the same origin.
      EXPECT_EQ(ProcessCreationOrder(*handle), inverse_index);
    }
    EXPECT_EQ(GetActiveProcessesOfWorkletType(
                  AuctionProcessManager::WorkletType::kBidder) +
                  GetActiveProcessesOfWorkletType(
                      AuctionProcessManager::WorkletType::kSeller),
              i + 1u);
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
              inverse_index);
    handles.push_back(std::move(handle));
  }
}

// Make sure we're not creating duplicate processes for
// an origin, even if we've assigned one of our anticipatory
// processes to a worklet.
TEST_P(AuctionProcessManagerTest,
       DoesNotRecreateAnticipatoryProcessForOriginAfterAssigned) {
  url::Origin origins[] = {kOriginA, kOriginB, kOriginC};
  for (const url::Origin& origin_to_request_service : origins) {
    CheckOnlyIdleProcessesWithCount(0);
    for (const url::Origin& origin_for_anticipatory_process : origins) {
      MaybeStartAnticipatoryProcess(origin_for_anticipatory_process,
                                    GetWorkletType());
    }
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 3u);

    AuctionProcessManager::ProcessHandle handle;
    RequestWorkletService(&handle, origin_to_request_service, GetWorkletType(),
                          /*expect_success=*/true,
                          RequestWorkletServiceOutcome::kUsedIdleProcess);
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 2u);

    for (const url::Origin& origin_for_anticipatory_process : origins) {
      MaybeStartAnticipatoryProcess(origin_for_anticipatory_process,
                                    GetWorkletType());
    }
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 2u);

    // Reset the number of processes for the next loop by letting the idle
    // processes expire. The active process will go out of scope.
    task_environment_.FastForwardBy(
        features::kFledgeStartAnticipatoryProcessExpirationTime.Get());
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
  }
}

TEST_P(AuctionProcessManagerTest, RemovesProcessAfterExpirationTime) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  task_environment_.FastForwardBy(
      features::kFledgeStartAnticipatoryProcessExpirationTime.Get() -
      base::Milliseconds(1));
  CheckOnlyIdleProcessesWithCount(1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  CheckOnlyIdleProcessesWithCount(0);
}

TEST_P(AuctionProcessManagerTest, CorrectProcessGetsDeletedAfterExpiration) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  MaybeStartAnticipatoryProcess(kOriginB, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(2);

  // One processes should be deleted after the first
  // features::kFledgeStartAnticipatoryProcessExpirationTime
  // passes.
  task_environment_.FastForwardBy(
      features::kFledgeStartAnticipatoryProcessExpirationTime.Get() -
      base::Milliseconds(2));
  CheckOnlyIdleProcessesWithCount(2);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  CheckOnlyIdleProcessesWithCount(1);

  // Should not add a new idle process of kOriginB. We shouldn't
  // have deleted that process.
  MaybeStartAnticipatoryProcess(kOriginB, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);

  // Should add a new idle process of kOriginA.
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(2);
}

TEST_P(AuctionProcessManagerTest,
       DoesNotRemoveActiveProcessAfterExpirationTime) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  AuctionProcessManager::ProcessHandle handle;
  RequestWorkletService(&handle, kOriginA, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);

  task_environment_.FastForwardBy(
      features::kFledgeStartAnticipatoryProcessExpirationTime.Get());
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
}

TEST_P(AuctionProcessManagerTest, PrioritizesReadyIdleUnboundProcess) {
  // Unbound processes are only created in the dedicated process case.
  if (GetProcessMode() != ProcessMode::kDedicated) {
    return;
  }

  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  MaybeStartAnticipatoryProcess(kOriginB, GetOtherWorkletType());
  MaybeStartAnticipatoryProcess(kOriginC, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(3);

  const size_t kLastCreatedProcessIndex = 2;
  SimulateReadyProcess(kLastCreatedProcessIndex);
  AuctionProcessManager::ProcessHandle handle1, handle2;
  RequestWorkletService(&handle1, kOriginA, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
            kLastCreatedProcessIndex);
  EXPECT_EQ(ProcessCreationOrder(handle1), kLastCreatedProcessIndex);

  // The next best process is the first created one.
  RequestWorkletService(&handle2, kOriginB, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 2u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 1u);
  EXPECT_EQ(ProcessCreationOrder(handle2), 0u);
}

TEST_P(AuctionProcessManagerTest, PrioritizesEarliestReadyUnboundIdleProcess) {
  // Unbound processes are only created in the dedicated process case.
  if (GetProcessMode() != ProcessMode::kDedicated) {
    return;
  }

  std::vector<url::Origin> origins = {kOriginA, kOriginB, kOriginC};
  for (const auto& origin : origins) {
    MaybeStartAnticipatoryProcess(origin, GetWorkletType());
  }
  CheckOnlyIdleProcessesWithCount(3);

  for (size_t i = 0; i < origins.size(); ++i) {
    SimulateReadyProcess(i);
  }

  // Because the processes are all ready, they should be allocated in order.
  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>> handles;
  for (size_t i = 0; i < origins.size(); ++i) {
    handles.emplace_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    RequestWorkletService(handles.back().get(), origins[i], GetWorkletType(),
                          /*expect_success=*/true,
                          RequestWorkletServiceOutcome::kUsedIdleProcess);
    EXPECT_EQ(GetActiveProcessesOfWorkletType(), i + 1);
    EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 2 - i);
    EXPECT_EQ(ProcessCreationOrder(*handles.back()), i);
  }
}

TEST_P(AuctionProcessManagerTest,
       PrioritizesReadyUnboundIdleProcessOfSameTypeIfOverLimit) {
  // Unbound processes are only created in the dedicated process case.
  if (GetProcessMode() != ProcessMode::kDedicated) {
    return;
  }

  MaybeStartAnticipatoryProcess(kOriginA, GetOtherWorkletType());
  CheckOnlyIdleProcessesWithCount(1);
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%i.test", i)));
    MaybeStartAnticipatoryProcess(origin, GetWorkletType());
    CheckOnlyIdleProcessesWithCount(2 + i);
  }
  // Both the process of the other type and the last process
  // of the same type are ready.
  SimulateReadyProcess(GetMaxProcesses());
  SimulateReadyProcess(0);

  // We are at the limit so we should use that last process.
  AuctionProcessManager::ProcessHandle handle1, handle2, handle3;
  RequestWorkletService(&handle1, kOriginA, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 1u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
            GetMaxProcesses());
  EXPECT_EQ(ProcessCreationOrder(handle1), GetMaxProcesses());

  // Even though the first process is ready we have to use the same type because
  // we're at the limit.
  RequestWorkletService(&handle2, kOriginB, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 2u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
            GetMaxProcesses() - 1);
  EXPECT_EQ(ProcessCreationOrder(handle2), 1u);

  // We can use the first process when we request a process for
  // GetOtherWorkletType().
  RequestWorkletService(&handle3, kOriginC, GetOtherWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(), 2u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(GetOtherWorkletType()), 1u);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(),
            GetMaxProcesses() - 2);
  EXPECT_EQ(ProcessCreationOrder(handle3), 0u);
}

// Tests for the kInRendererSharedProcess ProcessMode only. These are different
// enough for the the other two modes test, that no tests are currently run in
// all three modes.
class InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault
    : public AuctionProcessManagerTest {
 public:
  InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFledgeStartAnticipatoryProcesses,
        {{"AnticipatoryProcessHoldTime", "3s"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
    testing::Combine(
        testing::Values(AuctionProcessManager::WorkletType::kSeller,
                        AuctionProcessManager::WorkletType::kBidder),
        testing::Values(ProcessMode::kInRendererSharedProcess)));

// Exercise the codepath where a RenderProcessHostDestroyed is received, to
// make sure it doesn't crash.
TEST_P(AuctionProcessManagerTest, ProcessDeleteBeforeHandle) {
  // The process crashing case in the dedicated process world is covered by the
  // ProcessCrash test, rather than this one.
  if (GetProcessMode() == ProcessMode::kDedicated) {
    return;
  }

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceExpectSuccess(kOriginA);
  ASSERT_FALSE(rph_factory_.GetProcesses()->empty());
  for (std::unique_ptr<MockRenderProcessHost>& proc :
       *rph_factory_.GetProcesses()) {
    proc.reset();
  }
  task_environment_.RunUntilIdle();
  handle_a1.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(AuctionProcessManagerTest, PidLookup) {
  auto handle = GetServiceExpectSuccess(kOriginA);

  base::ProcessId expected_pid = base::Process::Current().Pid();

  // Request PID twice. Should happen asynchronously, but only use one RPC.
  base::RunLoop run_loop0, run_loop1;
  bool got_pid0 = false, got_pid1 = false;
  std::optional<base::ProcessId> pid0 =
      handle->GetPid(base::BindLambdaForTesting(
          [&run_loop0, &got_pid0, expected_pid](base::ProcessId pid) {
            EXPECT_EQ(expected_pid, pid);
            got_pid0 = true;
            run_loop0.Quit();
          }));
  EXPECT_FALSE(pid0.has_value());
  std::optional<base::ProcessId> pid1 =
      handle->GetPid(base::BindLambdaForTesting(
          [&run_loop1, &got_pid1, expected_pid](base::ProcessId pid) {
            EXPECT_EQ(expected_pid, pid);
            got_pid1 = true;
            run_loop1.Quit();
          }));
  EXPECT_FALSE(pid1.has_value());

  if (dedicated_process_manager_) {
    SimulateReadyProcess(/*creation_index=*/0);
  } else {
    for (std::unique_ptr<MockRenderProcessHost>& proc :
         *rph_factory_.GetProcesses()) {
      proc->SimulateReady();
    }
  }

  run_loop0.Run();
  EXPECT_TRUE(got_pid0);
  run_loop1.Run();
  EXPECT_TRUE(got_pid1);

  // Next attempt should be synchronous.
  std::optional<base::ProcessId> pid2 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid2 case";
      }));
  EXPECT_EQ(expected_pid, pid2);

  // Reusing the process with another handle should also result in synchronous
  // PID lookups.
  auto handle2 = GetServiceExpectSuccess(kOriginA);
  std::optional<base::ProcessId> pid3 =
      handle2->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid2 case";
      }));
  EXPECT_EQ(expected_pid, pid3);
}

TEST_P(AuctionProcessManagerTest, PidLookupRendererProcessAlreadyRunning) {
  // There's no analog to a renderer process already existing in the dedicated
  // process world.
  if (GetProcessMode() == ProcessMode::kDedicated) {
    return;
  }

  // "Launch" the appropriate process before we even ask for it, and mark its
  // launch as completed. |frame_site_instance| will help keep it alive.
  scoped_refptr<SiteInstance> frame_site_instance =
      site_instance1_->GetRelatedSiteInstance(kOriginA.GetURL());
  frame_site_instance->GetProcess()->Init();
  for (std::unique_ptr<MockRenderProcessHost>& proc :
       *rph_factory_.GetProcesses()) {
    proc->SimulateReady();
  }

  auto handle = GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                              frame_site_instance);

  base::ProcessId expected_pid = base::Process::Current().Pid();

  // Request PID twice. Should happen asynchronously, but only use one RPC.
  std::optional<base::ProcessId> pid0 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid0 case";
      }));
  ASSERT_TRUE(pid0.has_value());
  EXPECT_EQ(expected_pid, pid0.value());
  std::optional<base::ProcessId> pid1 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid1 case";
      }));
  ASSERT_TRUE(pid1.has_value());
  EXPECT_EQ(expected_pid, pid1.value());
}

TEST_P(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       MultipleSiteInstances) {
  base::HistogramTester histogram_tester;

  // Launch some services in different origins and browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance1_);
  int id_a1 = handle_a1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance2_);
  int id_a2 = handle_a2->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginB,
                                    site_instance1_);
  int id_b1 = handle_b1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginB,
                                    site_instance2_);
  int id_b2 = handle_b2->GetRenderProcessHostForTesting()->GetID();

  // Non-site-isolation requiring origins can share processes, but not across
  // different browsing instances.
  EXPECT_NE(id_a1, id_a2);
  EXPECT_EQ(id_a1, id_b1);
  EXPECT_NE(id_a1, id_b2);
  EXPECT_NE(id_a2, id_b1);
  EXPECT_EQ(id_a2, id_b2);
  EXPECT_NE(id_b1, id_b2);
  histogram_tester.ExpectUniqueSample(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kUsedSharedProcess, 4);

  // Site-isolation requiring origins are distinct from non-isolated ones, but
  // can share across browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kIsolatedOrigin,
                                    site_instance1_);
  int id_i1 = handle_i1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kIsolatedOrigin,
                                    site_instance2_);
  int id_i2 = handle_i2->GetRenderProcessHostForTesting()->GetID();

  EXPECT_EQ(id_i1, id_i2);
  EXPECT_NE(id_i1, id_a1);
  EXPECT_NE(id_i1, id_a2);
  EXPECT_NE(id_i1, id_b1);
  EXPECT_NE(id_i1, id_b2);
  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 1);
  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 1);
}

// Test that anticipatory processes are not created for origins that can use the
// shared renderer process.
TEST_P(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       MaybeStartAnticipatoryProcess_DoesNotStartIfSharedProcessPossible) {
  MaybeStartAnticipatoryProcess(kOriginA, GetWorkletType());
  CheckOnlyIdleProcessesWithCount(0);
}

// Test that anticipatory processes can be created for isolated origins.
TEST_P(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       MaybeStartAnticipatoryProcess_StartsProcessForIsolatedOrigin) {
  MaybeStartAnticipatoryProcess(kIsolatedOrigin);
  CheckOnlyIdleProcessesWithCount(1);

  // Don't use this process for an origin that can use a shared process.
  AuctionProcessManager::ProcessHandle handle1, handle2;
  RequestWorkletService(&handle1, kOriginA, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedSharedProcess);
  CheckOnlyIdleProcessesWithCount(1);

  // Can use this process for the same isolated origin.
  RequestWorkletService(&handle2, kIsolatedOrigin, GetWorkletType(),
                        /*expect_success=*/true,
                        RequestWorkletServiceOutcome::kUsedIdleProcess);
  EXPECT_EQ(auction_process_manager_->GetIdleProcessCountForTesting(), 0u);
  EXPECT_EQ(GetActiveProcessesOfWorkletType(GetWorkletType()), 1u);
}

// Tests the site-per-process sharing model, focusing on the multiple
// SiteInstances case, which should not affect process sharing.
TEST_P(AuctionProcessManagerTest, MultipleSiteInstances) {
  base::HistogramTester histogram_tester;

  // Launch some services in different origins and browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance1_);
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance2_);
  // Despite having different SiteInstances, `handle_a1` and `handle_a2` should
  // share the same process and service, since they share an origin.
  EXPECT_EQ(handle_a1->worklet_process_for_testing(),
            handle_a2->worklet_process_for_testing());
  EXPECT_EQ(handle_a1->GetService(), handle_a2->GetService());

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginB,
                                    site_instance1_);
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginB,
                                    site_instance2_);
  // Similarly, `handle_b1` and `handle_b2` should share a process and service.
  EXPECT_EQ(handle_b1->worklet_process_for_testing(),
            handle_b2->worklet_process_for_testing());
  EXPECT_EQ(handle_b1->GetService(), handle_b2->GetService());

  // Since sites are partitioned by origin, the `a` handles and `b` handles
  // should use different processes and services.
  EXPECT_NE(handle_a1->worklet_process_for_testing(),
            handle_b1->worklet_process_for_testing());
  EXPECT_NE(handle_a1->GetService(), handle_b1->GetService());

  // If using InRendererMode, they should also use different RenderProcessHosts.
  if (GetProcessMode() != ProcessMode::kDedicated) {
    EXPECT_NE(handle_a1->GetRenderProcessHostForTesting()->GetID(),
              handle_b1->GetRenderProcessHostForTesting()->GetID());
  }

  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 2);
  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 2);

  // Stuff that's also isolated by explicit requests gets the same treatment.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kIsolatedOrigin,
                                    site_instance1_);
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kIsolatedOrigin,
                                    site_instance2_);
  EXPECT_EQ(handle_i1->worklet_process_for_testing(),
            handle_i2->worklet_process_for_testing());
  EXPECT_EQ(handle_i1->GetService(), handle_i2->GetService());

  // If using InRendererMode, they should also use different RenderProcessHosts.
  if (GetProcessMode() != ProcessMode::kDedicated) {
    EXPECT_NE(handle_i1->GetRenderProcessHostForTesting()->GetID(),
              handle_a1->GetRenderProcessHostForTesting()->GetID());
    EXPECT_NE(handle_i1->GetRenderProcessHostForTesting()->GetID(),
              handle_b1->GetRenderProcessHostForTesting()->GetID());
  }

  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 3);
  histogram_tester.ExpectBucketCount(
      RequestWorkletServiceOutcomeUmaName(),
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 3);
}

TEST_P(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       PolicyChange) {
  // Launch site in default instance.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance1_);
  EXPECT_FALSE(
      handle_a1->site_instance_for_testing()->RequiresDedicatedProcess());
  RenderProcessHost* shared_process =
      handle_a1->GetRenderProcessHostForTesting();

  // Change policy so that A can no longer use shared instances.
  SiteInstance::StartIsolatingSite(
      &test_browser_context_, kOriginA.GetURL(),
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  site_instance1_ = SiteInstance::Create(&test_browser_context_);

  // Launch another A-origin worklet, this should get a different process.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance1_);
  EXPECT_TRUE(
      handle_a2->site_instance_for_testing()->RequiresDedicatedProcess());
  EXPECT_NE(handle_a2->GetRenderProcessHostForTesting(), shared_process);

  // Destroy shared process and try to get another A one --- should reuse the
  // same non-shared process.
  handle_a1.reset();
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a3 =
      GetServiceOfTypeExpectSuccess(GetWorkletType(), kOriginA,
                                    site_instance1_);
  EXPECT_TRUE(
      handle_a3->site_instance_for_testing()->RequiresDedicatedProcess());
  EXPECT_EQ(handle_a2->GetRenderProcessHostForTesting(),
            handle_a3->GetRenderProcessHostForTesting());
  // Checking GetRenderProcessHostForTesting isn't enough since SiteInstance
  // can share it, too.
  EXPECT_EQ(handle_a2->worklet_process_for_testing(),
            handle_a3->worklet_process_for_testing());
}

}  // namespace
}  // namespace content
