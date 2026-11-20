// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/worker_main_script_load_parameters.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_exception_details.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_fetch_client_settings_object.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sharedworkeroptions_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_trustedscripturl_usvstring.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

class ErrorEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    EXPECT_EQ(event->type(), event_type_names::kError);
    error_count_++;
  }
  int ErrorCount() const { return error_count_; }

 private:
  int error_count_ = 0;
};

}  // namespace

class SharedWorkerPermissionsPolicyTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  std::optional<network::PermissionsPolicyFeaturesBitset>
  ParsePermissionsPolicy(SharedWorker* worker,
                         const KURL& script_url,
                         const String& allow) {
    return worker->ParsePermissionsPolicy(script_url, allow);
  }

 private:
  ScopedPermissionsPolicyForSharedWorkersForTest scoped_feature_{true};
};

TEST_F(SharedWorkerPermissionsPolicyTest, AllowedFeaturesSucceed) {
  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  KURL script_url("https://example.com/worker.js");

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, script_url, "geolocation; usb");
  ASSERT_TRUE(policy.has_value());

  EXPECT_TRUE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kGeolocation));
  EXPECT_TRUE(policy->Contains(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kSerial));
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       UnrecognizedFeatureLogsConsoleWarning) {
  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  KURL script_url("https://example.com/worker.js");

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, script_url, "unknown-feature");
  EXPECT_TRUE(policy.has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Unrecognized feature: 'unknown-feature'.");
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       DisallowedFeatureInCreatorFiresErrorEventAndLogsConsoleError) {
  // Disable geolocation in the creator frame's PermissionsPolicy.
  network::ParsedPermissionsPolicy header_policy = {
      network::ParsedPermissionsPolicyDeclaration(
          network::mojom::PermissionsPolicyFeature::kGeolocation)};
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr, header_policy, /*container_policy=*/{},
          GetFrame().DomWindow()->GetSecurityOrigin()->ToUrlOrigin()));
  ASSERT_FALSE(GetFrame().DomWindow()->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  auto* listener = MakeGarbageCollected<ErrorEventListener>();
  worker->addEventListener(event_type_names::kError, listener);
  KURL script_url("https://example.com/worker.js");

  // Creating a worker without requesting 'geolocation' (or explicitly setting
  // "geolocation 'none'") should succeed even though geolocation is disabled in
  // the creator context.
  EXPECT_TRUE(
      ParsePermissionsPolicy(worker, script_url, g_empty_string).has_value());
  EXPECT_TRUE(ParsePermissionsPolicy(worker, script_url, "geolocation 'none'")
                  .has_value());
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 0);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().size(), 0u);

  // Requesting 'geolocation' when it is disabled in the creator context logs a
  // console error naming the feature and dispatches an error event
  // asynchronously.
  EXPECT_FALSE(
      ParsePermissionsPolicy(worker, script_url, "geolocation").has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);

  // Same check for a data: URL worker.
  EXPECT_FALSE(ParsePermissionsPolicy(worker, KURL("data:text/javascript,"),
                                      "geolocation")
                   .has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 2u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(1)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 2);
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       AllowedFeaturesForOpaqueOriginWithDefaultPolicySucceed) {
  // When the creator context has no declared policy for 'geolocation' or 'usb'
  // (which have a default allowlist of 'self') and the features are enabled in
  // the creator context, the creator can delegate them to an opaque origin
  // (such as a data: URL), even though IsFeatureEnabledForOrigin() returns
  // false for the opaque origin.
  const network::PermissionsPolicy* creator_policy =
      GetFrame().DomWindow()->GetSecurityContext().GetPermissionsPolicy();
  ASSERT_TRUE(creator_policy);
  KURL data_url("data:text/javascript,");
  url::Origin opaque_origin = SecurityOrigin::Create(data_url)->ToUrlOrigin();
  ASSERT_TRUE(opaque_origin.opaque());
  ASSERT_FALSE(creator_policy->IsFeatureEnabledForOrigin(
      network::mojom::PermissionsPolicyFeature::kGeolocation, opaque_origin));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();

  std::optional<network::PermissionsPolicyFeaturesBitset> policy =
      ParsePermissionsPolicy(worker, data_url, "geolocation; usb");
  ASSERT_TRUE(policy.has_value());

  EXPECT_TRUE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kGeolocation));
  EXPECT_TRUE(policy->Contains(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(
      policy->Contains(network::mojom::PermissionsPolicyFeature::kSerial));
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       DisallowedFeatureForScriptOriginFiresErrorEventAndLogsConsoleError) {
  // Declare `Permissions-Policy: geolocation=(self)` on the creator frame.
  // 'geolocation' is enabled for the document's own origin, but because there
  // is an explicit declared policy only matching 'self', it cannot be delegated
  // to an opaque origin (such as a data: URL).
  url::Origin creator_origin =
      GetFrame().DomWindow()->GetSecurityOrigin()->ToUrlOrigin();
  network::ParsedPermissionsPolicyDeclaration decl(
      network::mojom::PermissionsPolicyFeature::kGeolocation);
  decl.self_if_matches = creator_origin;
  network::ParsedPermissionsPolicy header_policy = {decl};
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr, header_policy, /*container_policy=*/{},
          creator_origin));
  ASSERT_TRUE(GetFrame().DomWindow()->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
  worker->UpdateStateIfNeeded();
  auto* listener = MakeGarbageCollected<ErrorEventListener>();
  worker->addEventListener(event_type_names::kError, listener);

  // Same-origin worker can still be delegated 'geolocation'.
  EXPECT_TRUE(ParsePermissionsPolicy(worker, KURL("https://example.com/w.js"),
                                     "geolocation")
                  .has_value());

  // Opaque-origin (data: URL) worker cannot be delegated 'geolocation'.
  EXPECT_FALSE(ParsePermissionsPolicy(worker, KURL("data:text/javascript,"),
                                      "geolocation")
                   .has_value());
  ASSERT_EQ(GetPage().GetConsoleMessageStorage().size(), 1u);
  EXPECT_EQ(GetPage().GetConsoleMessageStorage().at(0)->Message(),
            "Permissions-Policy feature 'geolocation' specified in the "
            "'allow' option cannot be delegated by the creating context "
            "to the worker script's origin.");
  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);
}

TEST_F(SharedWorkerPermissionsPolicyTest,
       DisallowedFeatureUnreferencedWorkerFiresErrorAndCanBeCollected) {
  network::ParsedPermissionsPolicy header_policy = {
      network::ParsedPermissionsPolicyDeclaration(
          network::mojom::PermissionsPolicyFeature::kGeolocation)};
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr, header_policy, /*container_policy=*/{},
          GetFrame().DomWindow()->GetSecurityOrigin()->ToUrlOrigin()));
  ASSERT_FALSE(GetFrame().DomWindow()->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  Persistent<ErrorEventListener> listener =
      MakeGarbageCollected<ErrorEventListener>();
  WeakPersistent<SharedWorker> weak_worker;
  {
    auto* worker = MakeGarbageCollected<SharedWorker>(GetFrame().DomWindow());
    worker->UpdateStateIfNeeded();
    worker->addEventListener(event_type_names::kError, listener.Get());
    EXPECT_FALSE(ParsePermissionsPolicy(
                     worker, KURL("https://example.com/w.js"), "geolocation")
                     .has_value());
    weak_worker = worker;
  }

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_worker);

  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_worker);
}

namespace {

class TestSharedWorkerHost final : public mojom::blink::SharedWorkerHost {
 public:
  mojo::PendingRemote<mojom::blink::SharedWorkerHost>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void WaitUntilConnected() {
    if (connected_) {
      return;
    }
    base::RunLoop run_loop;
    connected_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitUntilFeatureUsed(mojom::blink::WebFeature feature) {
    if (used_features_.contains(feature)) {
      return;
    }
    expected_feature_ = feature;
    base::RunLoop run_loop;
    feature_used_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool HasUsedFeature(mojom::blink::WebFeature feature) const {
    return used_features_.contains(feature);
  }

  // mojom::blink::SharedWorkerHost:
  void OnConnected(int connection_request_id) override {
    connected_ = true;
    if (connected_closure_) {
      std::move(connected_closure_).Run();
    }
  }
  void OnContextClosed() override {}
  void OnReadyForInspection(
      mojo::PendingRemote<mojom::blink::DevToolsAgent>,
      mojo::PendingReceiver<mojom::blink::DevToolsAgentHost>) override {}
  void OnScriptLoadFailed(const String& error_message) override {}
  void OnFeatureUsed(mojom::blink::WebFeature feature) override {
    used_features_.insert(feature);
    if (expected_feature_ == feature && feature_used_closure_) {
      std::move(feature_used_closure_).Run();
    }
  }
  void OnReportException(
      mojom::blink::SharedWorkerExceptionDetailsPtr details) override {}
  void SetPermissionsPolicyHeader(
      const Vector<network::ParsedPermissionsPolicyDeclaration>& headers)
      override {
    headers_ = headers;
  }

  const std::optional<Vector<network::ParsedPermissionsPolicyDeclaration>>&
  headers() const {
    return headers_;
  }

 private:
  bool connected_ = false;
  base::OnceClosure connected_closure_;
  std::set<mojom::blink::WebFeature> used_features_;
  std::optional<mojom::blink::WebFeature> expected_feature_;
  base::OnceClosure feature_used_closure_;
  std::optional<Vector<network::ParsedPermissionsPolicyDeclaration>> headers_;
  mojo::Receiver<mojom::blink::SharedWorkerHost> receiver_{this};
};

class TestSharedWorkerClient final : public WebSharedWorkerClient {
 public:
  void WorkerContextDestroyed() override {
    if (destroyed_closure_) {
      std::move(destroyed_closure_).Run();
    }
  }

  void WaitUntilWorkerDestroyed() {
    base::RunLoop run_loop;
    destroyed_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::OnceClosure destroyed_closure_;
};

}  // namespace

class WebSharedWorkerPermissionsPolicyTest : public testing::Test {
 protected:
  struct WorkerConsoleMessage {
    mojom::blink::ConsoleMessageSource source;
    mojom::blink::ConsoleMessageLevel level;
    String message;
  };

  void TearDown() override {
    if (worker_) {
      worker_->TerminateWorkerContext();
      client_.WaitUntilWorkerDestroyed();
      worker_.reset();
    }
  }

  void StartWorker(
      const std::string& permissions_policy_header,
      const std::vector<network::mojom::PermissionsPolicyFeature>& allow) {
    network::PermissionsPolicyFeaturesBitset allow_bitset;
    for (network::mojom::PermissionsPolicyFeature feature : allow) {
      allow_bitset.Add(feature);
    }
    const KURL kScriptUrl("https://example.com/worker.js");
    const WebSecurityOrigin kOrigin(SecurityOrigin::Create(kScriptUrl));

    auto load_params = std::make_unique<WorkerMainScriptLoadParameters>();
    auto head = network::mojom::URLResponseHead::New();
    net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200 OK");
    builder.AddHeader("Content-Type", "text/javascript");
    if (!permissions_policy_header.empty()) {
      builder.AddHeader("Permissions-Policy", permissions_policy_header);
    }
    head->headers = builder.Build();
    head->headers->GetMimeType(&head->mime_type);
    load_params->response_head = std::move(head);

    mojo::PendingReceiver<network::mojom::URLLoader> loader;
    mojo::Remote<network::mojom::URLLoaderClient> loader_client;
    load_params->url_loader_client_endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            loader.InitWithNewPipeAndPassRemote(),
            loader_client.BindNewPipeAndPassReceiver());
    loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));

    mojo::ScopedDataPipeProducerHandle producer_handle;
    CHECK_EQ(
        mojo::CreateDataPipe(1u, producer_handle, load_params->response_body),
        MOJO_RESULT_OK);
    producer_handle.reset();

    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker;
    std::ignore = browser_interface_broker.InitWithNewPipeAndPassReceiver();

    auto policy_container = std::make_unique<WebPolicyContainer>(
        WebPolicyContainerPolicies(),
        mock_policy_container_host_.BindNewEndpointAndPassDedicatedRemote());

    worker_ = WebSharedWorker::CreateAndStart(
        SharedWorkerToken(), kScriptUrl, mojom::blink::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, WebString(), kOrigin,
        kOrigin, /*is_constructor_secure_context=*/true,
        WebString("user-agent"), UserAgentMetadata(),
        /*content_security_policies=*/{},
        WebFetchClientSettingsObject(
            WebPolicyContainerPolicies(), kScriptUrl,
            mojom::InsecureRequestsPolicy::kDoNotUpgrade),
        /*outside_settings_reporter=*/mojo::NullRemote(),
        base::UnguessableToken::Create(),
        /*content_settings=*/mojo::NullRemote(),
        std::move(browser_interface_broker),
        /*pause_worker_context_on_start=*/false, std::move(load_params),
        std::move(policy_container),
        base::MakeRefCounted<EmptyWebWorkerFetchContext>(),
        host_.BindNewPipeAndPassRemote(), &client_, ukm::kInvalidSourceId,
        /*require_cross_site_request_for_cookies=*/false,
        net::StorageAccessApiStatus::kNone,
        /*coep_reporting_observer=*/mojo::NullReceiver(),
        /*dip_reporting_observer=*/mojo::NullReceiver(),
        /*cross_origin_isolated=*/false, allow_bitset);

    base::RunLoop run_loop;
    SharedWorkerThread* worker_thread =
        static_cast<WebSharedWorkerImpl*>(worker_.get())
            ->GetWorkerThreadForTesting();
    worker_thread->GetTaskRunner(TaskType::kInternalDefault)
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              const network::PermissionsPolicy* policy =
                  worker_thread->GlobalScope()
                      ->GetSecurityContext()
                      .GetPermissionsPolicy();
              ASSERT_TRUE(policy);
              for (const auto& [feature, default_value] :
                   policy->GetFeatureList()) {
                if (worker_thread->GlobalScope()->IsFeatureEnabled(feature)) {
                  allowed_features_.push_back(feature);
                }
              }
              ConsoleMessageStorage* storage =
                  worker_thread->GetConsoleMessageStorage();
              for (wtf_size_t i = 0; i < storage->size(); ++i) {
                ConsoleMessage* msg = storage->at(i);
                console_messages_.push_back(
                    {msg->GetSource(), msg->GetLevel(), msg->Message()});
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    host_.FlushForTesting();
  }

  test::TaskEnvironment task_environment_;
  ScopedPermissionsPolicyForSharedWorkersForTest scoped_feature_{true};
  MockPolicyContainerHost mock_policy_container_host_;
  TestSharedWorkerHost host_;
  TestSharedWorkerClient client_;
  std::unique_ptr<WebSharedWorker> worker_;
  std::vector<network::mojom::PermissionsPolicyFeature> allowed_features_;
  std::vector<WorkerConsoleMessage> console_messages_;
};

TEST_F(WebSharedWorkerPermissionsPolicyTest, NoHeaderAndNoAllowOption) {
  StartWorker("", {});
  EXPECT_THAT(allowed_features_, IsEmpty());
  EXPECT_THAT(console_messages_, IsEmpty());
  EXPECT_THAT(host_.headers(), Optional(IsEmpty()));
}

TEST_F(WebSharedWorkerPermissionsPolicyTest, HeaderRestrictsDelegatedFeatures) {
  StartWorker("serial=()", {network::mojom::PermissionsPolicyFeature::kSerial,
                            network::mojom::PermissionsPolicyFeature::kUsb});
  EXPECT_THAT(
      allowed_features_,
      UnorderedElementsAre(network::mojom::PermissionsPolicyFeature::kUsb));
  EXPECT_THAT(console_messages_, IsEmpty());
  EXPECT_THAT(host_.headers(),
              Optional(ElementsAre(network::ParsedPermissionsPolicyDeclaration(
                  network::mojom::PermissionsPolicyFeature::kSerial))));
}

TEST_F(WebSharedWorkerPermissionsPolicyTest,
       HeaderCannotEnableFeaturesThatWereNotDelegated) {
  StartWorker("usb=(self)",
              {network::mojom::PermissionsPolicyFeature::kSerial});
  EXPECT_THAT(
      allowed_features_,
      UnorderedElementsAre(network::mojom::PermissionsPolicyFeature::kSerial));
  EXPECT_THAT(console_messages_, IsEmpty());
  EXPECT_THAT(
      host_.headers(),
      Optional(ElementsAre(network::ParsedPermissionsPolicyDeclaration(
          network::mojom::PermissionsPolicyFeature::kUsb,
          /*allowed_origins=*/{},
          SecurityOrigin::Create(KURL("https://example.com/worker.js"))
              ->ToUrlOrigin(),
          /*matches_all_origins=*/false, /*matches_opaque_src=*/false))));
}

TEST_F(WebSharedWorkerPermissionsPolicyTest, UnrecognizedFeatureLogsWarning) {
  StartWorker("serial=(), unknown-feature=()",
              {network::mojom::PermissionsPolicyFeature::kSerial,
               network::mojom::PermissionsPolicyFeature::kUsb});
  EXPECT_THAT(
      allowed_features_,
      UnorderedElementsAre(network::mojom::PermissionsPolicyFeature::kUsb));
  ASSERT_EQ(console_messages_.size(), 1u);
  EXPECT_EQ(console_messages_[0].source,
            mojom::blink::ConsoleMessageSource::kSecurity);
  EXPECT_EQ(console_messages_[0].level,
            mojom::blink::ConsoleMessageLevel::kWarning);
  EXPECT_EQ(console_messages_[0].message,
            "Error with Permissions-Policy header: Unrecognized feature: "
            "'unknown-feature'.");
  EXPECT_THAT(host_.headers(),
              Optional(ElementsAre(network::ParsedPermissionsPolicyDeclaration(
                  network::mojom::PermissionsPolicyFeature::kSerial))));
}

TEST_F(WebSharedWorkerPermissionsPolicyTest,
       MultipleUnrecognizedFeaturesLogMultipleWarnings) {
  StartWorker("unknown-1=(), unknown-2=()",
              {network::mojom::PermissionsPolicyFeature::kSerial,
               network::mojom::PermissionsPolicyFeature::kUsb});
  EXPECT_THAT(
      allowed_features_,
      UnorderedElementsAre(network::mojom::PermissionsPolicyFeature::kSerial,
                           network::mojom::PermissionsPolicyFeature::kUsb));
  ASSERT_EQ(console_messages_.size(), 2u);
  EXPECT_EQ(console_messages_[0].message,
            "Error with Permissions-Policy header: Unrecognized feature: "
            "'unknown-1'.");
  EXPECT_EQ(console_messages_[1].message,
            "Error with Permissions-Policy header: Unrecognized feature: "
            "'unknown-2'.");
  EXPECT_THAT(host_.headers(), Optional(IsEmpty()));
}

TEST_F(WebSharedWorkerPermissionsPolicyTest,
       InvalidHeaderLogsErrorAndIsIgnored) {
  StartWorker("serial=(", {network::mojom::PermissionsPolicyFeature::kSerial,
                           network::mojom::PermissionsPolicyFeature::kUsb});
  EXPECT_THAT(
      allowed_features_,
      UnorderedElementsAre(network::mojom::PermissionsPolicyFeature::kSerial,
                           network::mojom::PermissionsPolicyFeature::kUsb));
  ASSERT_EQ(console_messages_.size(), 1u);
  EXPECT_EQ(console_messages_[0].source,
            mojom::blink::ConsoleMessageSource::kSecurity);
  EXPECT_EQ(console_messages_[0].level,
            mojom::blink::ConsoleMessageLevel::kError);
  EXPECT_EQ(console_messages_[0].message,
            "Error with Permissions-Policy header: Parse of permissions policy "
            "failed because of errors reported by structured header parser.");
  EXPECT_THAT(host_.headers(), Optional(IsEmpty()));
}

class WebSharedWorkerMixedContentUseCounterTest : public testing::Test {
 protected:
  void TearDown() override {
    if (worker_) {
      worker_->TerminateWorkerContext();
      client_.WaitUntilWorkerDestroyed();
      worker_.reset();
    }
  }

  void StartWorker(const KURL& script_url,
                   const KURL& constructor_url,
                   bool is_constructor_secure_context) {
    const WebSecurityOrigin constructor_origin(
        SecurityOrigin::Create(constructor_url));
    const WebSecurityOrigin origin_from_browser(
        SecurityOrigin::Create(script_url));

    auto load_params = std::make_unique<WorkerMainScriptLoadParameters>();
    auto head = network::mojom::URLResponseHead::New();
    net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200 OK");
    builder.AddHeader("Content-Type", "text/javascript");
    head->headers = builder.Build();
    head->headers->GetMimeType(&head->mime_type);
    load_params->response_head = std::move(head);

    mojo::PendingReceiver<network::mojom::URLLoader> loader;
    mojo::Remote<network::mojom::URLLoaderClient> loader_client;
    load_params->url_loader_client_endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            loader.InitWithNewPipeAndPassRemote(),
            loader_client.BindNewPipeAndPassReceiver());
    loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));

    mojo::ScopedDataPipeProducerHandle producer_handle;
    CHECK_EQ(
        mojo::CreateDataPipe(1u, producer_handle, load_params->response_body),
        MOJO_RESULT_OK);
    producer_handle.reset();

    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker;
    std::ignore = browser_interface_broker.InitWithNewPipeAndPassReceiver();

    auto policy_container = std::make_unique<WebPolicyContainer>(
        WebPolicyContainerPolicies(),
        mock_policy_container_host_.BindNewEndpointAndPassDedicatedRemote());

    worker_ = WebSharedWorker::CreateAndStart(
        SharedWorkerToken(), script_url, mojom::blink::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, WebString(),
        constructor_origin, origin_from_browser, is_constructor_secure_context,
        WebString("user-agent"), UserAgentMetadata(),
        /*content_security_policies=*/{},
        WebFetchClientSettingsObject(
            WebPolicyContainerPolicies(), constructor_url,
            mojom::InsecureRequestsPolicy::kDoNotUpgrade),
        /*outside_settings_reporter=*/mojo::NullRemote(),
        base::UnguessableToken::Create(),
        /*content_settings=*/mojo::NullRemote(),
        std::move(browser_interface_broker),
        /*pause_worker_context_on_start=*/false, std::move(load_params),
        std::move(policy_container),
        base::MakeRefCounted<EmptyWebWorkerFetchContext>(),
        host_.BindNewPipeAndPassRemote(), &client_, ukm::kInvalidSourceId,
        /*require_cross_site_request_for_cookies=*/false,
        net::StorageAccessApiStatus::kNone,
        /*coep_reporting_observer=*/mojo::NullReceiver(),
        /*dip_reporting_observer=*/mojo::NullReceiver(),
        /*cross_origin_isolated=*/false,
        network::PermissionsPolicyFeaturesBitset());

    MessagePortDescriptorPair port_pair;
    worker_->Connect(1, port_pair.TakePort0());
    host_.WaitUntilConnected();
  }

  template <typename Func>
  void RunOnWorkerThread(Func&& func) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    SharedWorkerThread* worker_thread =
        static_cast<WebSharedWorkerImpl*>(worker_.get())
            ->GetWorkerThreadForTesting();
    ASSERT_TRUE(worker_thread);
    worker_thread->GetTaskRunner(TaskType::kInternalDefault)
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              base::ScopedClosureRunner quit_runner(std::move(quit_closure));
              auto* global_scope =
                  To<SharedWorkerGlobalScope>(worker_thread->GlobalScope());
              ASSERT_TRUE(global_scope);
              func(*global_scope);
            }));
    run_loop.Run();
  }

  test::TaskEnvironment task_environment_;
  MockPolicyContainerHost mock_policy_container_host_;
  TestSharedWorkerHost host_;
  TestSharedWorkerClient client_;
  std::unique_ptr<WebSharedWorker> worker_;
};

TEST_F(WebSharedWorkerMixedContentUseCounterTest,
       CountsMixedContentFetchInHttpsSharedWorker) {
  StartWorker(KURL("https://example.com/worker.js"),
              KURL("https://example.com/"),
              /*is_constructor_secure_context=*/true);

  RunOnWorkerThread([](SharedWorkerGlobalScope& global_scope) {
    auto& fetch_context =
        static_cast<WorkerFetchContext&>(global_scope.Fetcher()->Context());
    fetch_context.ShouldBlockFetchByMixedContentCheck(
        mojom::blink::RequestContextType::FETCH,
        network::mojom::blink::IPAddressSpace::kUnknown, std::nullopt,
        KURL("https://helper.example/data.txt"),
        ReportingDisposition::kSuppressReporting, String());
    fetch_context.ShouldBlockFetchByMixedContentCheck(
        mojom::blink::RequestContextType::FETCH,
        network::mojom::blink::IPAddressSpace::kUnknown, std::nullopt,
        KURL("http://127.0.0.1/data.txt"),
        ReportingDisposition::kSuppressReporting, String());
  });
  test::RunPendingTasks();
  host_.FlushForTesting();
  EXPECT_FALSE(host_.HasUsedFeature(WebFeature::kMixedContentInSharedWorker));

  RunOnWorkerThread([](SharedWorkerGlobalScope& global_scope) {
    auto& fetch_context =
        static_cast<WorkerFetchContext&>(global_scope.Fetcher()->Context());
    fetch_context.ShouldBlockFetchByMixedContentCheck(
        mojom::blink::RequestContextType::FETCH,
        network::mojom::blink::IPAddressSpace::kUnknown, std::nullopt,
        KURL("http://helper.example/data.txt"),
        ReportingDisposition::kSuppressReporting, String());
  });
  host_.WaitUntilFeatureUsed(WebFeature::kMixedContentInSharedWorker);
  EXPECT_TRUE(host_.HasUsedFeature(WebFeature::kMixedContentInSharedWorker));
}

TEST_F(WebSharedWorkerMixedContentUseCounterTest,
       CountsMixedContentWebSocketInHttpsSharedWorker) {
  StartWorker(KURL("https://example.com/worker.js"),
              KURL("https://example.com/"),
              /*is_constructor_secure_context=*/true);

  RunOnWorkerThread([](SharedWorkerGlobalScope& global_scope) {
    auto& fetch_context =
        static_cast<WorkerFetchContext&>(global_scope.Fetcher()->Context());
    fetch_context.ShouldBlockWebSocketByMixedContentCheck(
        KURL("wss://helper.example/socket"),
        network::mojom::blink::IPAddressSpace::kUnknown);
  });
  test::RunPendingTasks();
  host_.FlushForTesting();
  EXPECT_FALSE(host_.HasUsedFeature(WebFeature::kMixedContentInSharedWorker));

  RunOnWorkerThread([](SharedWorkerGlobalScope& global_scope) {
    auto& fetch_context =
        static_cast<WorkerFetchContext&>(global_scope.Fetcher()->Context());
    fetch_context.ShouldBlockWebSocketByMixedContentCheck(
        KURL("ws://helper.example/socket"),
        network::mojom::blink::IPAddressSpace::kUnknown);
  });
  host_.WaitUntilFeatureUsed(WebFeature::kMixedContentInSharedWorker);
  EXPECT_TRUE(host_.HasUsedFeature(WebFeature::kMixedContentInSharedWorker));
}

TEST_F(WebSharedWorkerMixedContentUseCounterTest,
       DoesNotCountInHttpSharedWorker) {
  StartWorker(KURL("http://example.com/worker.js"), KURL("http://example.com/"),
              /*is_constructor_secure_context=*/false);

  RunOnWorkerThread([](SharedWorkerGlobalScope& global_scope) {
    auto& fetch_context =
        static_cast<WorkerFetchContext&>(global_scope.Fetcher()->Context());
    fetch_context.ShouldBlockFetchByMixedContentCheck(
        mojom::blink::RequestContextType::FETCH,
        network::mojom::blink::IPAddressSpace::kUnknown, std::nullopt,
        KURL("http://helper.example/data.txt"),
        ReportingDisposition::kSuppressReporting, String());
    fetch_context.ShouldBlockWebSocketByMixedContentCheck(
        KURL("ws://helper.example/socket"),
        network::mojom::blink::IPAddressSpace::kUnknown);
  });
  test::RunPendingTasks();
  host_.FlushForTesting();
  EXPECT_FALSE(host_.HasUsedFeature(WebFeature::kMixedContentInSharedWorker));
}

class SharedWorkerEarlyAbortTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.test/"));
  }

  SharedWorker* CreateWorker(const String& url,
                             ExceptionState& exception_state) {
    return SharedWorker::Create(
        GetFrame().DomWindow(),
        MakeGarbageCollected<V8UnionTrustedScriptURLOrUSVString>(url),
        MakeGarbageCollected<V8UnionSharedWorkerOptionsOrString>(
            g_empty_string),
        exception_state);
  }
};

TEST_F(SharedWorkerEarlyAbortTest,
       CSPBlockedUnreferencedWorkerFiresErrorAndCanBeCollected) {
  NavigateTo(KURL("https://example.test/"),
             {{"Content-Security-Policy", "worker-src 'none'"}});

  Persistent<ErrorEventListener> listener =
      MakeGarbageCollected<ErrorEventListener>();
  WeakPersistent<SharedWorker> weak_worker;
  {
    DummyExceptionStateForTesting exception_state;
    SharedWorker* worker =
        CreateWorker("https://example.test/worker.js", exception_state);
    ASSERT_FALSE(exception_state.HadException());
    ASSERT_TRUE(worker);
    worker->addEventListener(event_type_names::kError, listener.Get());
    weak_worker = worker;
  }

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_worker);

  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_worker);
}

TEST_F(SharedWorkerEarlyAbortTest,
       CrossOriginBlockedUnreferencedWorkerFiresErrorAndCanBeCollected) {
  ScopedNoSynchronousThrowForCrossOriginBlockedWorkerForTest scoped_feature(
      true);

  Persistent<ErrorEventListener> listener =
      MakeGarbageCollected<ErrorEventListener>();
  WeakPersistent<SharedWorker> weak_worker;
  {
    DummyExceptionStateForTesting exception_state;
    SharedWorker* worker =
        CreateWorker("https://cross-origin.test/worker.js", exception_state);
    ASSERT_FALSE(exception_state.HadException());
    ASSERT_TRUE(worker);
    worker->addEventListener(event_type_names::kError, listener.Get());
    weak_worker = worker;
  }

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_worker);

  test::RunPendingTasks();
  EXPECT_EQ(listener->ErrorCount(), 1);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_worker);
}

}  // namespace blink
