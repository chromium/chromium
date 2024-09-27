// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A MojoLPM fuzzer targeting the public API surfaces of the Protected Audiences
// API.
//
// See documentation in ad_auction_service_mojolpm_fuzzer_docs.md.

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer.pb.h"
#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer_stringifiers.h"
#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer_stringifiers.pb.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/privacy_sandbox_invoking_api.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::ad_auction_service_mojolpm_fuzzer {

class SiteInstance;

class AllowInterestGroupContentBrowserClient
    : public content::TestContentBrowserClient {
 public:
  explicit AllowInterestGroupContentBrowserClient() = default;
  ~AllowInterestGroupContentBrowserClient() override = default;

  AllowInterestGroupContentBrowserClient(
      const AllowInterestGroupContentBrowserClient&) = delete;
  AllowInterestGroupContentBrowserClient& operator=(
      const AllowInterestGroupContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    return true;
  }

  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }

  bool IsCookieDeprecationLabelAllowed(
      content::BrowserContext* browser_context) override {
    return true;
  }
};

constexpr char kFledgeUpdateHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/JSON\n"
    "Ad-Auction-Allowed: true\n";

constexpr char kInterestGroupUpdate[] = R"({
"ads": [{"renderURL": "https://example.com/new_render"
        }]
})";

constexpr char kFledgeScriptHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/Javascript\n"
    "Ad-Auction-Allowed: true\n";

constexpr char kBasicScript[] = R"(
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
    trustedBiddingSignals, browserSignals, directFromSellerSignals) {
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderURL,
                'allowComponentAuction': true};
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
    browserSignals, directFromSellerSignals) {
}

function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  return {desirability: bid,
          allowComponentAuction: true};
}

function reportResult(auctionConfig, browserSignals) {
    return {};
}
)";

// For handling network requests made by the Protected Audience API -- also
// prevents those requests from being made to real servers.
class NetworkResponder {
 public:
  void SetScript(content::fuzzing::ad_auction_service::proto::Script script) {
    base::AutoLock auto_lock(lock_);
    script_ = Stringify(script);
  }

 private:
  bool RequestHandler(content::URLLoaderInterceptor::RequestParams* params) {
    base::AutoLock auto_lock(lock_);
    if (params->url_request.url.path().find(".js") != std::string::npos) {
      content::URLLoaderInterceptor::WriteResponse(
          kFledgeScriptHeaders, script_ ? *script_ : kBasicScript,
          params->client.get());
    } else {
      content::URLLoaderInterceptor::WriteResponse(
          kFledgeUpdateHeaders, kInterestGroupUpdate, params->client.get());
    }
    return true;
  }

  mutable base::Lock lock_;

  // Handles network requests for interest group updates.
  content::URLLoaderInterceptor network_interceptor_{
      base::BindRepeating(&NetworkResponder::RequestHandler,
                          base::Unretained(this))};

  std::optional<std::string> script_ GUARDED_BY(lock_);
};

// AuctionProcessManager that allows running auctions in-proc.
class SameProcessAuctionProcessManager : public content::AuctionProcessManager {
 public:
  SameProcessAuctionProcessManager() = default;
  SameProcessAuctionProcessManager(const SameProcessAuctionProcessManager&) =
      delete;
  SameProcessAuctionProcessManager& operator=(
      const SameProcessAuctionProcessManager&) = delete;
  ~SameProcessAuctionProcessManager() override = default;

 private:
  scoped_refptr<WorkletProcess> LaunchProcess(
      const ProcessHandle* process_handle,
      const std::string& display_name) override {
    // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
    // production code. Don't bother to delete the service on pipe close,
    // though; just keep it in a vector instead.
    mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
    auction_worklet_services_.push_back(
        auction_worklet::AuctionWorkletServiceImpl::CreateForService(
            service.InitWithNewPipeAndPassReceiver()));
    return base::MakeRefCounted<WorkletProcess>(
        this, /*render_process_host=*/nullptr, std::move(service),
        process_handle->worklet_type(), process_handle->origin(),
        /*uses_shared_process=*/false);
  }

  scoped_refptr<content::SiteInstance> MaybeComputeSiteInstance(
      content::SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

const char* const kCmdline[] = {"ad_auction_service_mojolpm_fuzzer", nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence (checked with `this->sequence_checker_`).
//
// Test cases may create one or more service instances, send Mojo messages to
// remotes for those service instances, and run IO and UI thread tasks (the
// fuzzer itself runs on its own thread, distinct from the UI and IO threads).
//
// For each input Testcase proto, SetUp() is run first. (This is why expensive
// "stateless" initialization happens just once, in GetEnvironment(), before
// SetUp() is run). Then, "new service" actions from the Testcase proto instruct
// the fuzzer to create new service implementation instances; they are owned by
// the RFH through DocumentService, and the RFH is owned by `test_adapter_`.
// Actions use an ID to determine which service instance to use, allowing
// control over which remote to use when running remote actions.  When all the
// actions in the current Testcase proto have been executed, TearDown() is
// called, and then this AdAuctionServiceTestcase is destroyed.  After that, the
// process repeats with the next Testcase proto input.
class AdAuctionServiceTestcase
    : public ::mojolpm::Testcase<
          content::fuzzing::ad_auction_service::proto::Testcase,
          content::fuzzing::ad_auction_service::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::ad_auction_service::proto::Testcase;
  using ProtoAction = content::fuzzing::ad_auction_service::proto::Action;
  explicit AdAuctionServiceTestcase(
      const content::fuzzing::ad_auction_service::proto::Testcase& testcase);
  ~AdAuctionServiceTestcase();

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread();
  void TearDownOnUIThread();

  // Create and bind a new AdAuctionServiceImpl instance, and register the
  // remote with MojoLPM.
  //
  // Runs on fuzzer thread, calling CreateAdAuctionServiceImplOnUIThread() on
  // the UI thread to create the implementation.
  void AddAdAuctionService(uint32_t id, base::OnceClosure done_closure);
  void CreateAdAuctionServiceImplOnUIThread(
      mojo::PendingReceiver<blink::mojom::AdAuctionService>&& receiver);

  // This is run every time we run the RunUntilIdle action -- this ensures that,
  // for instance, completion callbacks posted from the AdAuctionService
  // implementation's database thread are run on the UI thread.
  void RunUntilIdleOnUIThread();

  // All the below fields must be accessed on the UI thread.
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList fenced_frame_feature_list_;

  AllowInterestGroupContentBrowserClient content_browser_client_;
  raw_ptr<content::ContentBrowserClient> old_content_browser_client_ = nullptr;
  content::mojolpm::RenderViewHostTestHarnessAdapter test_adapter_;
  raw_ptr<content::TestRenderFrameHost> render_frame_host_ = nullptr;

  // Must be destroyed before test_adapter_::TearDown().
  std::optional<NetworkResponder> network_responder_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

AdAuctionServiceTestcase::AdAuctionServiceTestcase(
    const ProtoTestcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {blink::features::kInterestGroupStorage,
       blink::features::kAdInterestGroupAPI, blink::features::kFledge},
      /*disabled_features=*/{});
  fenced_frame_feature_list_.InitAndEnableFeatureWithParameters(
      blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  test_adapter_.SetUp();
  network_responder_.emplace();
}

AdAuctionServiceTestcase::~AdAuctionServiceTestcase() {
  network_responder_.reset();
  test_adapter_.TearDown();
}

void AdAuctionServiceTestcase::RunAction(const ProtoAction& action,
                                         base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  const auto ThreadId_UI =
      content::fuzzing::ad_auction_service::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::ad_auction_service::proto::RunThreadAction_ThreadId_IO;
  switch (action.action_case()) {
    case ProtoAction::kRunThread:
      // These actions ensure that any tasks currently queued on the named
      // thread have chance to run before the fuzzer continues.
      //
      // We don't provide any particular guarantees here; this does not mean
      // that the named thread is idle, nor does it prevent any other threads
      // from running (or the consequences of any resulting callbacks, for
      // example).
      if (action.run_thread().id() == ThreadId_UI) {
        content::GetUIThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      } else if (action.run_thread().id() == ThreadId_IO) {
        content::GetIOThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      }
      return;
    case ProtoAction::kNewAdAuctionService:
      // Create and bind a new AdAuctionServiceImpl instance, and register the
      // remote with MojoLPM.
      AddAdAuctionService(action.new_ad_auction_service().id(),
                          std::move(run_closure));
      return;
    case ProtoAction::kRunUntilIdle:
      // On the UI thread, call RunUntilIdle() -- this is often needed to wait
      // for other threads, like the AdAuctionService API implementation's
      // database thread -- that thread posts results to the UI thread, the UI
      // thread needs to be able to run the completion callbacks that receive
      // the database results.
      content::GetUIThreadTaskRunner({})->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&AdAuctionServiceTestcase::RunUntilIdleOnUIThread,
                         base::Unretained(this)),
          std::move(run_closure));
      return;
    case ProtoAction::kNetResponseAction:
      network_responder_->SetScript(action.net_response_action().script());
      break;
    case ProtoAction::kAdAuctionServiceRemoteAction:
      // Invoke one of the service methods on AdAuctionService, with parameters
      // specified in the ad_auction_service_remote_action() proto, on the
      // remote given by the id in the proto.
      ::mojolpm::HandleRemoteAction(action.ad_auction_service_remote_action());
      break;
    case ProtoAction::ACTION_NOT_SET:
      break;
  }
  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void AdAuctionServiceTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AdAuctionServiceTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      std::move(done_closure));
}

void AdAuctionServiceTestcase::SetUpOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  test_adapter_.NavigateAndCommit(GURL("https://owner.test:443"));
  render_frame_host_ =
      static_cast<content::TestWebContents*>(test_adapter_.web_contents())
          ->GetPrimaryMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();
  old_content_browser_client_ =
      SetBrowserClientForTesting(&content_browser_client_);

  auto* manager_ = static_cast<content::InterestGroupManagerImpl*>(
      test_adapter_.browser_context()
          ->GetDefaultStoragePartition()
          ->GetInterestGroupManager());
  // Process creation crashes in the Chrome zygote init in unit tests, so run
  // the auction "processes" in-process instead.
  manager_->set_auction_process_manager_for_testing(
      std::make_unique<SameProcessAuctionProcessManager>());
}

void AdAuctionServiceTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AdAuctionServiceTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      std::move(done_closure));
}

void AdAuctionServiceTestcase::TearDownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetBrowserClientForTesting(old_content_browser_client_);
}

void AdAuctionServiceTestcase::AddAdAuctionService(
    uint32_t id,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  mojo::Remote<blink::mojom::AdAuctionService> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  ::mojolpm::GetContext()->AddInstance(id, std::move(remote));
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &AdAuctionServiceTestcase::CreateAdAuctionServiceImplOnUIThread,
          base::Unretained(this), std::move(receiver)),
      std::move(run_closure));
}

void AdAuctionServiceTestcase::CreateAdAuctionServiceImplOnUIThread(
    mojo::PendingReceiver<blink::mojom::AdAuctionService>&& receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::AdAuctionServiceImpl::CreateMojoService(render_frame_host_,
                                                   std::move(receiver));
}

void AdAuctionServiceTestcase::RunUntilIdleOnUIThread() {
  test_adapter_.task_environment()->RunUntilIdle();
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::ad_auction_service::proto::Testcase&
        proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  AdAuctionServiceTestcase testcase(proto_testcase);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&::mojolpm::RunTestcase<AdAuctionServiceTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}

}  // namespace content::ad_auction_service_mojolpm_fuzzer
