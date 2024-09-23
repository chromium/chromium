// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "gin/array_buffer.h"
#include "gin/dictionary.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message_mojom_traits.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

constexpr char kModuleScriptSource[] = "https://foo.com/module_script.js";
constexpr char kMaxChar16StringLengthPlusOneLiteral[] = "2621441";
constexpr base::Time kScriptResponseTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(100));

struct VoidOperationResult {
  bool success = true;
  std::string error_message;
};

using AddModuleResult = VoidOperationResult;
using RunResult = VoidOperationResult;
using SetResult = VoidOperationResult;
using AppendResult = VoidOperationResult;
using DeleteResult = VoidOperationResult;
using ClearResult = VoidOperationResult;

struct SelectURLResult {
  bool success = true;
  std::string error_message;
  uint32_t index = 0;
};

struct GetResult {
  blink::mojom::SharedStorageGetStatus status =
      blink::mojom::SharedStorageGetStatus::kSuccess;
  std::string error_message;
  std::u16string value;
};

struct LengthResult {
  bool success = true;
  std::string error_message;
  uint32_t length = 0;
};

struct RemainingBudgetResult {
  bool success = true;
  std::string error_message;
  double bits = 0;
};

struct SetParams {
  std::u16string key;
  std::u16string value;
  bool ignore_if_present = false;
};

struct AppendParams {
  std::u16string key;
  std::u16string value;
};

std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> CreateBatchResult(
    std::vector<std::pair<std::u16string, std::u16string>> input) {
  std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> result;
  for (const auto& p : input) {
    blink::mojom::SharedStorageKeyAndOrValuePtr e =
        blink::mojom::SharedStorageKeyAndOrValue::New(p.first, p.second);
    result.push_back(std::move(e));
  }
  return result;
}

class TestWorkletDevToolsHost : public mojom::blink::WorkletDevToolsHost {
 public:
  explicit TestWorkletDevToolsHost(
      mojo::PendingReceiver<mojom::blink::WorkletDevToolsHost> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnReadyForInspection(
      mojo::PendingRemote<mojom::blink::DevToolsAgent> agent,
      mojo::PendingReceiver<mojom::blink::DevToolsAgentHost> agent_host)
      override {
    EXPECT_FALSE(ready_for_inspection_);
    ready_for_inspection_ = true;
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  bool ready_for_inspection() const { return ready_for_inspection_; }

 private:
  bool ready_for_inspection_ = false;

  mojo::Receiver<mojom::blink::WorkletDevToolsHost> receiver_{this};
};

class TestClient : public blink::mojom::SharedStorageWorkletServiceClient {
 public:
  explicit TestClient(mojo::PendingAssociatedReceiver<
                      blink::mojom::SharedStorageWorkletServiceClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override {
    observed_set_params_.push_back({key, value, ignore_if_present});
    std::move(callback).Run(set_result_.success, set_result_.error_message);
  }

  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override {
    observed_append_params_.push_back({key, value});
    std::move(callback).Run(append_result_.success,
                            append_result_.error_message);
  }

  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override {
    observed_delete_params_.push_back(key);
    std::move(callback).Run(delete_result_.success,
                            delete_result_.error_message);
  }

  void SharedStorageClear(SharedStorageClearCallback callback) override {
    observed_clear_count_++;
    std::move(callback).Run(clear_result_.success, clear_result_.error_message);
  }

  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override {
    observed_get_params_.push_back(key);
    std::move(callback).Run(get_result_.status, get_result_.error_message,
                            get_result_.value);
  }

  void SharedStorageKeys(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    pending_keys_listeners_.push_back(std::move(pending_listener));
  }

  void SharedStorageEntries(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override {
    pending_entries_listeners_.push_back(std::move(pending_listener));
  }

  void SharedStorageLength(SharedStorageLengthCallback callback) override {
    observed_length_count_++;
    std::move(callback).Run(length_result_.success,
                            length_result_.error_message,
                            length_result_.length);
  }

  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override {
    observed_remaining_budget_count_++;
    std::move(callback).Run(remaining_budget_result_.success,
                            remaining_budget_result_.error_message,
                            remaining_budget_result_.bits);
  }

  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                              const std::string& message) override {
    observed_console_log_messages_.push_back(message);
  }

  void RecordUseCounters(
      const std::vector<mojom::WebFeature>& features) override {
    base::ranges::for_each(features, [&](mojom::WebFeature feature) {
      observed_use_counters_.push_back(feature);
    });
  }

  mojo::Remote<blink::mojom::SharedStorageEntriesListener>
  TakeKeysListenerAtFront() {
    CHECK(!pending_keys_listeners_.empty());

    auto pending_listener = std::move(pending_keys_listeners_.front());
    pending_keys_listeners_.pop_front();

    return mojo::Remote<blink::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

  mojo::Remote<blink::mojom::SharedStorageEntriesListener>
  TakeEntriesListenerAtFront() {
    CHECK(!pending_entries_listeners_.empty());

    auto pending_listener = std::move(pending_entries_listeners_.front());
    pending_entries_listeners_.pop_front();

    return mojo::Remote<blink::mojom::SharedStorageEntriesListener>(
        std::move(pending_listener));
  }

  std::deque<mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>>
      pending_keys_listeners_;

  std::deque<mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>>
      pending_entries_listeners_;

  std::vector<SetParams> observed_set_params_;
  std::vector<AppendParams> observed_append_params_;
  std::vector<std::u16string> observed_delete_params_;
  size_t observed_clear_count_ = 0;
  std::vector<std::u16string> observed_get_params_;
  size_t observed_length_count_ = 0;
  size_t observed_remaining_budget_count_ = 0;
  std::vector<std::string> observed_console_log_messages_;
  std::vector<mojom::WebFeature> observed_use_counters_;

  // Default results to be returned for corresponding operations. They can be
  // overridden.
  SetResult set_result_;
  AppendResult append_result_;
  DeleteResult delete_result_;
  ClearResult clear_result_;
  GetResult get_result_;
  LengthResult length_result_;
  RemainingBudgetResult remaining_budget_result_;

 private:
  mojo::AssociatedReceiver<blink::mojom::SharedStorageWorkletServiceClient>
      receiver_{this};
};

class MockMojomPrivateAggregationHost
    : public blink::mojom::blink::PrivateAggregationHost {
 public:
  MockMojomPrivateAggregationHost() = default;

  void FlushForTesting() { receiver_set_.FlushForTesting(); }

  mojo::ReceiverSet<blink::mojom::blink::PrivateAggregationHost>&
  receiver_set() {
    return receiver_set_;
  }

  // blink::mojom::blink::PrivateAggregationHost:
  MOCK_METHOD(
      void,
      ContributeToHistogram,
      (Vector<blink::mojom::blink::AggregatableReportHistogramContributionPtr>),
      (override));
  MOCK_METHOD(void,
              EnableDebugMode,
              (blink::mojom::blink::DebugKeyPtr),
              (override));

 private:
  mojo::ReceiverSet<blink::mojom::blink::PrivateAggregationHost> receiver_set_;
};

class MockMojomCoceCacheHost : public blink::mojom::blink::CodeCacheHost {
 public:
  MockMojomCoceCacheHost() = default;

  void FlushForTesting() { receiver_set_.FlushForTesting(); }

  mojo::ReceiverSet<blink::mojom::blink::CodeCacheHost>& receiver_set() {
    return receiver_set_;
  }

  // blink::mojom::blink::CoceCacheHost:
  void DidGenerateCacheableMetadata(mojom::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {
    did_generate_cacheable_metadata_count_++;

    // Store the time and data. This mirrors the real-world behavior.
    response_time_ = expected_response_time;
    data_ = std::move(data);
  }

  void FetchCachedCode(mojom::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback callback) override {
    fetch_cached_code_count_++;
    std::move(callback).Run(response_time_, data_.Clone());
  }

  void ClearCodeCacheEntry(mojom::CodeCacheType cache_type,
                           const KURL& url) override {
    clear_code_cache_entry_count_++;
  }

  void DidGenerateCacheableMetadataInCacheStorage(
      const KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const String& cache_storage_cache_name) override {
    NOTREACHED();
  }

  void OverrideFetchCachedCodeResult(base::Time response_time,
                                     mojo_base::BigBuffer data) {
    response_time_ = response_time;
    data_ = std::move(data);
  }

  size_t did_generate_cacheable_metadata_count() const {
    return did_generate_cacheable_metadata_count_;
  }

  size_t fetch_cached_code_count() const { return fetch_cached_code_count_; }

  size_t clear_code_cache_entry_count() const {
    return clear_code_cache_entry_count_;
  }

 private:
  base::Time response_time_;
  mojo_base::BigBuffer data_;

  size_t did_generate_cacheable_metadata_count_ = 0;
  size_t fetch_cached_code_count_ = 0;
  size_t clear_code_cache_entry_count_ = 0;

  mojo::ReceiverSet<blink::mojom::blink::CodeCacheHost> receiver_set_;
};

std::unique_ptr<GlobalScopeCreationParams> MakeTestGlobalScopeCreationParams() {
  return std::make_unique<GlobalScopeCreationParams>(
      KURL("https://foo.com"),
      /*script_type=*/mojom::blink::ScriptType::kModule, "SharedStorageWorklet",
      /*user_agent=*/String(),
      /*ua_metadata=*/std::optional<UserAgentMetadata>(),
      /*web_worker_fetch_context=*/nullptr,
      /*outside_content_security_policies=*/
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      /*response_content_security_policies=*/
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      /*referrer_policy=*/network::mojom::ReferrerPolicy::kDefault,
      /*starter_origin=*/nullptr,
      /*starter_secure_context=*/false,
      /*starter_https_state=*/HttpsState::kNone,
      /*worker_clients=*/nullptr,
      /*content_settings_client=*/nullptr,
      /*inherited_trial_features=*/nullptr,
      /*parent_devtools_token=*/base::UnguessableToken::Create(),
      /*worker_settings=*/nullptr,
      /*v8_cache_options=*/mojom::blink::V8CacheOptions::kDefault,
      /*module_responses_map=*/nullptr);
}

}  // namespace

class SharedStorageWorkletTest : public PageTestBase {
 public:
  SharedStorageWorkletTest() {
    mock_code_cache_host_ = std::make_unique<MockMojomCoceCacheHost>();
  }

  void TearDown() override {
    // Shut down the worklet gracefully. Otherwise, there could the a data race
    // on accessing the base::FeatureList: the worklet thread may access the
    // feature during SharedStorageWorkletGlobalScope::FinishOperation() or
    // SharedStorageWorkletGlobalScope::NotifyContextDestroyed(), which can
    // occur after the (maybe implicit) ScopedFeatureList is destroyed in the
    // main thread.
    shared_storage_worklet_service_.reset();
    EXPECT_TRUE(worklet_terminated_future_.Wait());

    PageTestBase::TearDown();
  }

  AddModuleResult AddModule(const std::string& script_content,
                            std::string mime_type = "application/javascript") {
    InitializeWorkletServiceOnce();

    mojo::Remote<network::mojom::URLLoaderFactory> factory;

    network::TestURLLoaderFactory proxied_url_loader_factory;

    auto head = network::mojom::URLResponseHead::New();
    head->mime_type = mime_type;
    head->charset = "us-ascii";
    head->response_time = kScriptResponseTime;

    proxied_url_loader_factory.AddResponse(
        GURL(kModuleScriptSource), std::move(head),
        /*content=*/script_content, network::URLLoaderCompletionStatus());

    proxied_url_loader_factory.Clone(factory.BindNewPipeAndPassReceiver());

    base::test::TestFuture<bool, const std::string&> future;
    shared_storage_worklet_service_->AddModule(
        factory.Unbind(), GURL(kModuleScriptSource), future.GetCallback());

    return {future.Get<0>(), future.Get<1>()};
  }

  SelectURLResult SelectURL(const std::string& name,
                            const std::vector<GURL>& urls,
                            blink::CloneableMessage serialized_data) {
    InitializeWorkletServiceOnce();

    base::test::TestFuture<bool, const std::string&, uint32_t> future;
    shared_storage_worklet_service_->RunURLSelectionOperation(
        name, urls, std::move(serialized_data), MaybeInitPAOperationDetails(),
        future.GetCallback());

    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  RunResult Run(const std::string& name,
                blink::CloneableMessage serialized_data,
                int filtering_id_max_bytes = 1) {
    InitializeWorkletServiceOnce();

    base::test::TestFuture<bool, const std::string&> future;
    shared_storage_worklet_service_->RunOperation(
        name, std::move(serialized_data),
        MaybeInitPAOperationDetails(filtering_id_max_bytes),
        future.GetCallback());

    return {future.Get<0>(), future.Get<1>()};
  }

  // CrossVariantMojoRemote<mojom::blink::PrivateAggregationHostInterfaceBase>
  mojom::PrivateAggregationOperationDetailsPtr MaybeInitPAOperationDetails(
      int filtering_id_max_bytes =
          kPrivateAggregationApiDefaultFilteringIdMaxBytes) {
    CHECK_EQ(ShouldDefinePrivateAggregationInSharedStorage(),
             !!mock_private_aggregation_host_);

    if (!ShouldDefinePrivateAggregationInSharedStorage()) {
      return nullptr;
    }

    mojo::PendingRemote<mojom::blink::PrivateAggregationHost>
        pending_pa_host_remote;
    mojo::PendingReceiver<mojom::blink::PrivateAggregationHost>
        pending_pa_host_receiver =
            pending_pa_host_remote.InitWithNewPipeAndPassReceiver();

    mock_private_aggregation_host_->receiver_set().Add(
        mock_private_aggregation_host_.get(),
        std::move(pending_pa_host_receiver));

    return mojom::PrivateAggregationOperationDetails::New(
        CrossVariantMojoRemote<
            mojom::blink::PrivateAggregationHostInterfaceBase>(
            std::move(pending_pa_host_remote)),
        filtering_id_max_bytes);
  }

  CloneableMessage CreateSerializedUndefined() {
    return CreateSerializedDictOrUndefined(nullptr);
  }

  CloneableMessage CreateSerializedDict(
      const std::map<std::string, std::string>& dict) {
    return CreateSerializedDictOrUndefined(&dict);
  }

 protected:
  ScopedSharedStorageAPIM125ForTest shared_storage_m125_runtime_enabled_feature{
      /*enabled=*/true};

  mojo::Remote<mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  Persistent<SharedStorageWorkletMessagingProxy> messaging_proxy_;

  std::optional<std::u16string> embedder_context_;
  bool private_aggregation_permissions_policy_allowed_ = true;

  base::test::TestFuture<void> worklet_terminated_future_;

  std::unique_ptr<TestClient> test_client_;
  std::unique_ptr<TestWorkletDevToolsHost> test_worklet_devtools_host_;
  std::unique_ptr<MockMojomPrivateAggregationHost>
      mock_private_aggregation_host_;
  std::unique_ptr<MockMojomCoceCacheHost> mock_code_cache_host_;

  base::HistogramTester histogram_tester_;

  bool worklet_service_initialized_ = false;

 private:
  CloneableMessage CreateSerializedDictOrUndefined(
      const std::map<std::string, std::string>* dict) {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope scope(script_state);
    v8::MicrotasksScope microtasksScope(script_state->GetContext(),
                                        v8::MicrotasksScope::kRunMicrotasks);

    v8::Isolate* isolate = script_state->GetIsolate();

    scoped_refptr<SerializedScriptValue> serialized_value;
    if (dict) {
      v8::Local<v8::Object> v8_value = v8::Object::New(isolate);
      gin::Dictionary gin_dict(isolate, v8_value);
      for (auto const& [key, val] : *dict) {
        gin_dict.Set<std::string>(key, val);
      }

      serialized_value = SerializedScriptValue::SerializeAndSwallowExceptions(
          isolate, v8_value);
    } else {
      serialized_value = SerializedScriptValue::UndefinedValue();
    }

    BlinkCloneableMessage original;
    original.message = std::move(serialized_value);
    original.sender_agent_cluster_id = base::UnguessableToken::Create();

    mojo::Message message =
        mojom::CloneableMessage::SerializeAsMessage(&original);
    mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
    message = mojo::Message::CreateFromMessageHandle(&handle);
    DCHECK(!message.IsNull());

    CloneableMessage converted;
    mojom::CloneableMessage::DeserializeFromMessage(std::move(message),
                                                    &converted);
    return converted;
  }

  void InitializeWorkletServiceOnce() {
    if (worklet_service_initialized_) {
      return;
    }

    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver =
        shared_storage_worklet_service_.BindNewPipeAndPassReceiver();

    mojo::PendingRemote<mojom::blink::WorkletDevToolsHost>
        pending_devtools_host_remote;
    mojo::PendingReceiver<mojom::blink::WorkletDevToolsHost>
        pending_devtools_host_receiver =
            pending_devtools_host_remote.InitWithNewPipeAndPassReceiver();
    test_worklet_devtools_host_ = std::make_unique<TestWorkletDevToolsHost>(
        std::move(pending_devtools_host_receiver));

    mojo::PendingRemote<mojom::blink::CodeCacheHost>
        pending_code_cache_host_remote;
    mojo::PendingReceiver<mojom::blink::CodeCacheHost>
        pending_code_cache_host_receiver =
            pending_code_cache_host_remote.InitWithNewPipeAndPassReceiver();

    mock_code_cache_host_->receiver_set().Add(
        mock_code_cache_host_.get(),
        std::move(pending_code_cache_host_receiver));

    messaging_proxy_ = MakeGarbageCollected<SharedStorageWorkletMessagingProxy>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        CrossVariantMojoReceiver<
            mojom::blink::SharedStorageWorkletServiceInterfaceBase>(
            std::move(receiver)),
        mojom::blink::WorkletGlobalScopeCreationParams::New(
            KURL(kModuleScriptSource),
            /*starter_origin=*/
            SecurityOrigin::Create(KURL(kModuleScriptSource)),
            Vector<blink::mojom::OriginTrialFeature>(),
            /*devtools_worker_token=*/base::UnguessableToken(),
            std::move(pending_devtools_host_remote),
            std::move(pending_code_cache_host_remote),
            mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>(),
            /*wait_for_debugger=*/false),
        worklet_terminated_future_.GetCallback());

    mojo::PendingAssociatedRemote<mojom::SharedStorageWorkletServiceClient>
        pending_shared_storage_service_client_remote;
    mojo::PendingAssociatedReceiver<mojom::SharedStorageWorkletServiceClient>
        pending_shared_storage_service_client_receiver =
            pending_shared_storage_service_client_remote
                .InitWithNewEndpointAndPassReceiver();

    test_client_ = std::make_unique<TestClient>(
        std::move(pending_shared_storage_service_client_receiver));

    if (ShouldDefinePrivateAggregationInSharedStorage()) {
      mock_private_aggregation_host_ =
          std::make_unique<MockMojomPrivateAggregationHost>();
    }

    shared_storage_worklet_service_->Initialize(
        std::move(pending_shared_storage_service_client_remote),
        private_aggregation_permissions_policy_allowed_, embedder_context_);

    worklet_service_initialized_ = true;
  }
};

TEST_F(SharedStorageWorkletTest, AddModule_EmptyScriptSuccess) {
  AddModuleResult result = AddModule(/*script_content=*/"");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, AddModule_SimpleScriptSuccess) {
  AddModuleResult result = AddModule(/*script_content=*/"let a = 1;");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());

  test_worklet_devtools_host_->FlushForTesting();
  EXPECT_TRUE(test_worklet_devtools_host_->ready_for_inspection());
}

TEST_F(SharedStorageWorkletTest, AddModule_SimpleScriptError) {
  AddModuleResult result = AddModule(/*script_content=*/"a;");
  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));

  test_worklet_devtools_host_->FlushForTesting();
  EXPECT_TRUE(test_worklet_devtools_host_->ready_for_inspection());
}

TEST_F(SharedStorageWorkletTest, AddModule_ScriptDownloadError) {
  AddModuleResult result = AddModule(/*script_content=*/"",
                                     /*mime_type=*/"unsupported_mime_type");
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message,
            "Rejecting load of https://foo.com/module_script.js due to "
            "unexpected MIME type.");
}

TEST_F(SharedStorageWorkletTest,
       CodeCache_NoClearDueToEmptyCache_NoGenerateData) {
  // Configure to return empty data, with matched response time.
  mock_code_cache_host_->OverrideFetchCachedCodeResult(
      /*response_time=*/kScriptResponseTime,
      /*data=*/std::vector<uint8_t>());

  AddModule(/*script_content=*/"");

  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 1u);

  // No invalidation was triggered, as `FetchCachedCode()` responded with empty
  // data.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 0u);

  // No code cache was generated, as the script size is too small.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 0u);
}

TEST_F(SharedStorageWorkletTest,
       CodeCache_DidClearDueToUnmatchedTime_NoGenerateData) {
  // Configure to return non-empty data, with unmatched response time.
  mock_code_cache_host_->OverrideFetchCachedCodeResult(
      /*response_time=*/kScriptResponseTime - base::Days(1),
      /*data=*/std::vector<uint8_t>(1));

  AddModule(/*script_content=*/"");
  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 1u);

  // Cache was cleared, as the response time did not match the time from the
  // script loading.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 1u);

  // No code cache was generated, as the script size is too small.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 0u);
}

TEST_F(SharedStorageWorkletTest,
       CodeCache_NoClearDueToMatchedTime_NoGenerateData) {
  // Configure to return non-empty data, with matched response time.
  mock_code_cache_host_->OverrideFetchCachedCodeResult(
      /*response_time=*/kScriptResponseTime,
      /*data=*/std::vector<uint8_t>(1));

  AddModule(/*script_content=*/"");
  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 1u);

  // No invalidation was triggered, as `FetchCachedCode()` responded with some
  // data with a matched response time.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 0u);

  // No code cache was generated, as the script size is too small.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 0u);
}

TEST_F(SharedStorageWorkletTest, CodeCache_DidGenerateData) {
  // Code cache will be generated when the code length is at least 1024 bytes.
  std::string large_script;
  while (large_script.size() < 1024) {
    large_script += "a=1;";
  }

  AddModule(large_script);
  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 1u);

  // No invalidation was triggered, as `FetchCachedCode()` responded with empty
  // data.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 0u);

  // Code cache was generated.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 1u);
}

TEST_F(SharedStorageWorkletTest, CodeCache_AddModuleTwice) {
  // Code cache will be generated when the code length is at least 1024 bytes.
  std::string large_script;
  while (large_script.size() < 1024) {
    large_script += "a=1;";
  }

  AddModule(large_script);
  AddModule(large_script);
  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 2u);

  // No invalidation was triggered. The second code cache fetch returns a
  // response time from the first result, which matches the response time from
  // the second script loading.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 0u);

  // The second script loading also triggered the code cache generation. This
  // implies that the code cache was still not used. This is expected, as we
  // won't store the cached code entirely for first seen URLs.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 2u);
}

TEST_F(SharedStorageWorkletTest, CodeCache_AddModuleThreeTimes) {
  // Code cache will be generated when the code length is at least 1024 bytes.
  std::string large_script;
  while (large_script.size() < 1024) {
    large_script += "a=1;";
  }

  AddModule(large_script);
  AddModule(large_script);
  AddModule(large_script);
  mock_code_cache_host_->FlushForTesting();

  EXPECT_EQ(mock_code_cache_host_->fetch_cached_code_count(), 3u);

  // No invalidation was triggered. The second and third code cache fetch
  // returns a response time from the first result, which matches the response
  // time from the second and third script loading.
  EXPECT_EQ(mock_code_cache_host_->clear_code_cache_entry_count(), 0u);

  // The third script loading did not trigger the code cache generation. This
  // implies that the cached code was used for the third script loading.
  EXPECT_EQ(mock_code_cache_host_->did_generate_cacheable_metadata_count(), 2u);
}

TEST_F(SharedStorageWorkletTest, WorkletTerminationDueToDisconnect) {
  AddModuleResult result = AddModule(/*script_content=*/"");

  // Trigger the disconnect handler.
  shared_storage_worklet_service_.reset();

  // Callback called means the worklet has terminated successfully.
  EXPECT_TRUE(worklet_terminated_future_.Wait());
}

TEST_F(SharedStorageWorkletTest, ConsoleLog_DuringAddModule) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
    console.log(123, "abc");
  )");

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "123 abc");
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_DuringAddModule) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
    var expectedObjects = [
      "console",
      "crypto"
    ];

    var expectedFunctions = [
      "SharedStorage",
      "Crypto",
      "CryptoKey",
      "SubtleCrypto",
      "TextEncoder",
      "TextDecoder",
      "register",
      "console.log"
    ];

    var expectedUndefinedVariables = [];

    for (let expectedObject of expectedObjects) {
      if (eval("typeof " + expectedObject) !== "object") {
        throw Error(expectedObject + " is not object type.")
      }
    }

    for (let expectedFunction of expectedFunctions) {
      if (eval("typeof " + expectedFunction) !== "function") {
        throw Error(expectedFunction + " is not function type.")
      }
    }

    for (let expectedUndefined of expectedUndefinedVariables) {
      if (eval("typeof " + expectedUndefined) !== "undefined") {
        throw Error(expectedUndefined + " is not undefined.")
      }
    }

    // Verify that trying to access `sharedStorage` would throw a custom error.
    try {
      sharedStorage;
    } catch (e) {
      console.log("Expected error:", e.message);
    }
  )");

  EXPECT_TRUE(add_module_result.success);
  EXPECT_EQ(add_module_result.error_message, "");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "Expected error: Failed to read the 'sharedStorage' property from "
            "'SharedStorageWorkletGlobalScope': sharedStorage cannot be "
            "accessed during addModule().");
}

TEST_F(SharedStorageWorkletTest,
       RegisterOperation_MissingOperationNameArgument) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register();
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_MissingClassArgument) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register("test-operation");
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_EmptyOperationName) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("", TestClass);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Operation name cannot be empty"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_ClassArgumentNotAFunction) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      register("test-operation", {});
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("parameter 2 is not of type 'Function'"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_MissingRunFunction) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        constructor() {
          this.run = 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Property \"run\" doesn't exist"));
}

TEST_F(SharedStorageWorkletTest,
       RegisterOperation_ClassArgumentPrototypeNotAnObject) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      function test() {};
      test.prototype = 123;

      register("test-operation", test);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("constructor prototype is not an object"));
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_Success) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, RegisterOperation_AlreadyRegistered) {
  AddModuleResult result = AddModule(/*script_content=*/R"(
    class TestClass1 {
      async run() {}
    }

    class TestClass2 {
      async run() {}
    }

    register("test-operation", TestClass1);
    register("test-operation", TestClass2);
  )");

  EXPECT_FALSE(result.success);
  EXPECT_THAT(result.error_message,
              testing::HasSubstr("Operation name already registered"));
}

TEST_F(SharedStorageWorkletTest, SelectURL_BeforeAddModuleFinish) {
  SelectURLResult select_url_result =
      SelectURL("test-operation", /*urls=*/{}, CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("The module script hasn't been loaded"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_OperationNameNotRegistered) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result = SelectURL(
      "unregistered-operation", /*urls=*/{}, CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("Cannot find operation name"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FunctionError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          a;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation", /*urls=*/{}, CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FulfilledSynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_RejectedAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ =
      LengthResult{.success = false, .error_message = "error 123", .length = 0};

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(select_url_result.error_message, testing::HasSubstr("error 123"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_FulfilledAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return sharedStorage.length();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ = LengthResult{
      .success = true, .error_message = std::string(), .length = 1};

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_StringConvertedToUint32) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return "1";
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_NumberOverflow) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return -4294967295;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_NonNumericStringConvertedTo0) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return "abc";
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_DefaultUndefinedResultConvertedTo0) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 0u);
}

// For a run() member function that is not marked "async", it will still be
// treated as async.
TEST_F(SharedStorageWorkletTest, SelectURL_NoExplicitAsync) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        run(urls) {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_ReturnValueOutOfRange) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          return 2;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(
      select_url_result.error_message,
      testing::HasSubstr(
          "Promise resolved to a number outside the length of the input urls"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest, SelectURL_ReturnValueToUint32Error) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls) {
          class CustomClass {
            toString() { throw Error('error 123'); }
          }

          return new CustomClass();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_FALSE(select_url_result.success);
  EXPECT_THAT(
      select_url_result.error_message,
      testing::HasSubstr("Promise did not resolve to an uint32 number"));
  EXPECT_EQ(select_url_result.index, 0u);
}

TEST_F(SharedStorageWorkletTest,
       SelectURL_ValidateUrlsAndDataParamViaConsoleLog) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(urls, data) {
          console.log(JSON.stringify(urls, Object.keys(urls).sort()));
          console.log(JSON.stringify(data, Object.keys(data).sort()));

          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedDict({{"customField", "customValue"}}));

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 2u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "[\"https://foo0.com/\",\"https://foo1.com/\"]");
  EXPECT_EQ(test_client_->observed_console_log_messages_[1],
            "{\"customField\":\"customValue\"}");
}

TEST_F(SharedStorageWorkletTest, Run_BeforeAddModuleFinish) {
  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("The module script hasn't been loaded"));
}

TEST_F(SharedStorageWorkletTest, Run_OperationNameNotRegistered) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result =
      Run("unregistered-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("Cannot find operation name"));
}

TEST_F(SharedStorageWorkletTest, Run_FunctionError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          a;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));
}

TEST_F(SharedStorageWorkletTest, Run_FulfilledSynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {}
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_RejectedAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->clear_result_ =
      ClearResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));
}

TEST_F(SharedStorageWorkletTest, Run_FulfilledAsynchronously) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_Microtask) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await Promise.resolve(0);
          return 0;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Run_ValidateDataParamViaConsoleLog) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run(data) {
          console.log(JSON.stringify(data, Object.keys(data).sort()));

          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run(
      "test-operation", CreateSerializedDict({{"customField", "customValue"}}));

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "{\"customField\":\"customValue\"}");
}

TEST_F(SharedStorageWorkletTest, SelectURLAndRunOnSameRegisteredOperation) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          return 1;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  SelectURLResult select_url_result =
      SelectURL("test-operation",
                /*urls=*/{GURL("https://foo0.com"), GURL("https://foo1.com")},
                CreateSerializedUndefined());

  EXPECT_TRUE(select_url_result.success);
  EXPECT_TRUE(select_url_result.error_message.empty());
  EXPECT_EQ(select_url_result.index, 1u);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleSuccess) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "console",
            "sharedStorage",
            "crypto"
          ];

          var expectedFunctions = [
            "SharedStorage",
            "Crypto",
            "CryptoKey",
            "SubtleCrypto",
            "TextEncoder",
            "TextDecoder",
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.createWorklet",
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStorageWorkletTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleFailure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "console",
            "sharedStorage",
            "crypto"
          ];

          var expectedFunctions = [
            "SharedStorage",
            "Crypto",
            "CryptoKey",
            "SubtleCrypto",
            "TextEncoder",
            "TextDecoder",
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);

      // This should fail the addModule()
      a;
  )");

  EXPECT_FALSE(add_module_result.success);
  EXPECT_THAT(add_module_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStorageWorkletTest, Set_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("", "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.set("a".repeat($1), "value");
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_MissingValue) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidValue_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "a".repeat($1));
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"value\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_InvalidOptions) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value", true);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr(
          "The provided value is not of type 'SharedStorageSetMethodOptions'"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Set_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->set_result_ =
      SetResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Set_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Set_IgnoreIfPresent_True) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value", {ignoreIfPresent: true});

          // A non-empty string will evaluate to true.
          await sharedStorage.set("key", "value", {ignoreIfPresent: "false"});

          // A dictionary object will evaluate to true.
          await sharedStorage.set("key", "value", {ignoreIfPresent: {}});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 3u);
  EXPECT_TRUE(test_client_->observed_set_params_[0].ignore_if_present);
  EXPECT_TRUE(test_client_->observed_set_params_[1].ignore_if_present);
  EXPECT_TRUE(test_client_->observed_set_params_[2].ignore_if_present);
}

TEST_F(SharedStorageWorkletTest, Set_IgnoreIfPresent_False) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set("key", "value");
          await sharedStorage.set("key", "value", {});
          await sharedStorage.set("key", "value", {ignoreIfPresent: false});
          await sharedStorage.set("key", "value", {ignoreIfPresent: ""});
          await sharedStorage.set("key", "value", {ignoreIfPresent: null});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 5u);
  EXPECT_FALSE(test_client_->observed_set_params_[0].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[1].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[2].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[3].ignore_if_present);
  EXPECT_FALSE(test_client_->observed_set_params_[4].ignore_if_present);
}

TEST_F(SharedStorageWorkletTest, Set_KeyAndValueConvertedToString) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.set(123, 456);
          await sharedStorage.set(null, null);
          await sharedStorage.set(undefined, undefined);
          await sharedStorage.set({dictKey1: 'dictValue1'}, {dictKey2: 'dictValue2'});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_set_params_.size(), 4u);
  EXPECT_EQ(test_client_->observed_set_params_[0].key, u"123");
  EXPECT_EQ(test_client_->observed_set_params_[0].value, u"456");
  EXPECT_EQ(test_client_->observed_set_params_[1].key, u"null");
  EXPECT_EQ(test_client_->observed_set_params_[1].value, u"null");
  EXPECT_EQ(test_client_->observed_set_params_[2].key, u"undefined");
  EXPECT_EQ(test_client_->observed_set_params_[2].value, u"undefined");
  EXPECT_EQ(test_client_->observed_set_params_[3].key, u"[object Object]");
  EXPECT_EQ(test_client_->observed_set_params_[3].value, u"[object Object]");
}

TEST_F(SharedStorageWorkletTest, Set_ParamConvertedToStringError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          class CustomClass {
            toString() { throw Error("error 123"); }
          };

          await sharedStorage.set(new CustomClass(), "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_set_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("", "value");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.append("a".repeat($1), "value");
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_MissingValue) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("2 arguments required, but only 1 present"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_InvalidValue_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key", "a".repeat($1));
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"value\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Append_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->append_result_ =
      AppendResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_append_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_append_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_append_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Append_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.append("key0", "value0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_append_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_append_params_[0].key, u"key0");
  EXPECT_EQ(test_client_->observed_append_params_[0].value, u"value0");
}

TEST_F(SharedStorageWorkletTest, Delete_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("1 argument required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("a".repeat($1), "value");
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Delete_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("key0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->delete_result_ =
      DeleteResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_delete_params_[0], u"key0");
}

TEST_F(SharedStorageWorkletTest, Delete_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.delete("key0");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_delete_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_delete_params_[0], u"key0");
}

TEST_F(SharedStorageWorkletTest, Clear_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->clear_result_ =
      ClearResult{.success = false, .error_message = "error 123"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_clear_count_, 1u);
}

TEST_F(SharedStorageWorkletTest, Clear_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.clear();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_clear_count_, 1u);
}

TEST_F(SharedStorageWorkletTest, Get_MissingKey) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.get();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message,
              testing::HasSubstr("1 argument required, but only 0 present"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_InvalidKey_Empty) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          await sharedStorage.get("");
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_InvalidKey_TooLong) {
  AddModuleResult add_module_result = AddModule(
      /*script_content=*/base::ReplaceStringPlaceholders(
          R"(
      class TestClass {
        async run() {
          await sharedStorage.get("a".repeat($1), "value");
        }
      }

      register("test-operation", TestClass);
  )",
          {kMaxChar16StringLengthPlusOneLiteral},
          /*offsets=*/nullptr));

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(
      run_result.error_message,
      testing::HasSubstr("Length of the \"key\" parameter is not valid"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kError,
                .error_message = "error 123",
                .value = std::u16string()};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Get_NotFound) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kNotFound,
                .error_message = std::string(),
                .value = std::u16string()};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "undefined");
}

TEST_F(SharedStorageWorkletTest, Get_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.get("key0");
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->get_result_ =
      GetResult{.status = blink::mojom::SharedStorageGetStatus::kSuccess,
                .error_message = std::string(),
                .value = u"value0"};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_get_params_.size(), 1u);
  EXPECT_EQ(test_client_->observed_get_params_[0], u"key0");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "value0");
}

TEST_F(SharedStorageWorkletTest, Length_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.length();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ =
      LengthResult{.success = false, .error_message = "error 123", .length = 0};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_length_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Length_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.length();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->length_result_ = LengthResult{
      .success = true, .error_message = std::string(), .length = 123};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_length_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "123");
}

TEST_F(SharedStorageWorkletTest, Entries_OneEmptyBatch_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          for await (const [key, value] of sharedStorage.entries()) {
            console.log(key + ';' + value);
          }
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_entries_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeEntriesListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{}, CreateBatchResult({}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/0);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Entries_FirstBatchError_Failure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          for await (const [key, value] of sharedStorage.entries()) {
            console.log(key + ';' + value);
          }
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_entries_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeEntriesListenerAtFront();
  listener->DidReadEntries(
      /*success=*/false, /*error_message=*/"Internal error 12345",
      CreateBatchResult({}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/0);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_FALSE(run_result.success);
  EXPECT_EQ(run_result.error_message, "OperationError: Internal error 12345");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, Entries_TwoBatches_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          for await (const [key, value] of sharedStorage.entries()) {
            console.log(key + ';' + value);
          }
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_entries_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeEntriesListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "key0;value0");

  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", u"value1"}, {u"key2", u"value2"}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/3);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 3u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[1], "key1;value1");
  EXPECT_EQ(test_client_->observed_console_log_messages_[2], "key2;value2");
}

TEST_F(SharedStorageWorkletTest, Entries_SecondBatchError_Failure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          for await (const [key, value] of sharedStorage.entries()) {
            console.log(key + ';' + value);
          }
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_entries_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeEntriesListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "key0;value0");

  listener->DidReadEntries(
      /*success=*/false, /*error_message=*/"Internal error 12345",
      CreateBatchResult({}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/3);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_FALSE(run_result.success);
  EXPECT_EQ(run_result.error_message, "OperationError: Internal error 12345");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
}

TEST_F(SharedStorageWorkletTest, Keys_OneBatch_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          for await (const key of sharedStorage.keys()) {
            console.log(key);
          }
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_keys_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeKeysListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}, {u"key1", u"value1"}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/2);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 2u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "key0");
  EXPECT_EQ(test_client_->observed_console_log_messages_[1], "key1");
}

TEST_F(SharedStorageWorkletTest, Keys_ManuallyCallNext) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          const keys_iterator = sharedStorage.keys()[Symbol.asyncIterator]();

          keys_iterator.next(); // result0 skipped
          keys_iterator.next(); // result1 skipped

          const result2 = await keys_iterator.next();
          console.log(JSON.stringify(result2));

          const result3 = await keys_iterator.next();
          console.log(JSON.stringify(result3));

          const result4 = await keys_iterator.next();
          console.log(JSON.stringify(result4));

          const result5 = await keys_iterator.next();
          console.log(JSON.stringify(result5));
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_keys_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeKeysListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", /*value=*/{}}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/4);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);

  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", /*value=*/{}}, {u"key2", /*value=*/{}}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/4);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "{\"done\":false,\"value\":\"key2\"}");

  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key3", /*value=*/{}}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/4);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 4u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[1],
            "{\"done\":false,\"value\":\"key3\"}");
  EXPECT_EQ(test_client_->observed_console_log_messages_[2], "{\"done\":true}");
  EXPECT_EQ(test_client_->observed_console_log_messages_[3], "{\"done\":true}");
}

TEST_F(SharedStorageWorkletTest, Values_ManuallyCallNext) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          const values_iterator = (
            sharedStorage.values()[Symbol.asyncIterator]());

          values_iterator.next(); // result0 skipped
          values_iterator.next(); // result1 skipped

          const result2 = await values_iterator.next();
          console.log(JSON.stringify(result2));

          const result3 = await values_iterator.next();
          console.log(JSON.stringify(result3));

          const result4 = await values_iterator.next();
          console.log(JSON.stringify(result4));

          const result5 = await values_iterator.next();
          console.log(JSON.stringify(result5));
        }
      }

      register("test-operation", TestClass);
  )");

  base::test::TestFuture<bool, const std::string&> run_future;
  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), run_future.GetCallback());
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->pending_entries_listeners_.size(), 1u);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener =
      test_client_->TakeEntriesListenerAtFront();
  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key0", u"value0"}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/4);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);

  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key1", u"value1"}, {u"key2", u"value2"}}),
      /*has_more_entries=*/true, /*total_queued_to_send=*/4);
  shared_storage_worklet_service_.FlushForTesting();

  EXPECT_FALSE(run_future.IsReady());
  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "{\"done\":false,\"value\":\"value2\"}");

  listener->DidReadEntries(
      /*success=*/true, /*error_message=*/{},
      CreateBatchResult({{u"key3", u"value3"}}),
      /*has_more_entries=*/false, /*total_queued_to_send=*/4);

  RunResult run_result{run_future.Get<0>(), run_future.Get<1>()};
  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 4u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[1],
            "{\"done\":false,\"value\":\"value3\"}");
  EXPECT_EQ(test_client_->observed_console_log_messages_[2], "{\"done\":true}");
  EXPECT_EQ(test_client_->observed_console_log_messages_[3], "{\"done\":true}");
}

TEST_F(SharedStorageWorkletTest, RemainingBudget_ClientError) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.remainingBudget();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->remaining_budget_result_ = RemainingBudgetResult{
      .success = false, .error_message = "error 123", .bits = 0};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_FALSE(run_result.success);
  EXPECT_THAT(run_result.error_message, testing::HasSubstr("error 123"));

  EXPECT_EQ(test_client_->observed_remaining_budget_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 0u);
}

TEST_F(SharedStorageWorkletTest, RemainingBudget_Success) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let a = await sharedStorage.remainingBudget();
          console.log(a);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  test_client_->remaining_budget_result_ = RemainingBudgetResult{
      .success = true, .error_message = std::string(), .bits = 2.0};

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_remaining_budget_count_, 1u);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "2");
}

TEST_F(SharedStorageWorkletTest, ContextAttribute_Undefined) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          console.log(sharedStorage.context);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "undefined");

  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.Worklet.Context.IsDefined", /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(SharedStorageWorkletTest, ContextAttribute_String) {
  embedder_context_ = u"some embedder context";

  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          console.log(sharedStorage.context);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "some embedder context");

  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.Worklet.Context.IsDefined", /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that methods on sharedStorage are resolved asynchronously, e.g. param
// validation failures won't affect the result of run().
TEST_F(SharedStorageWorkletTest,
       AsyncFailuresDuringOperation_OperationSucceed) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          sharedStorage.set();
          sharedStorage.append();
          sharedStorage.delete();
          sharedStorage.get();
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_TRUE(run_result.error_message.empty());
}

TEST_F(SharedStorageWorkletTest, Crypto_GetRandomValues) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          const myArray = new BigUint64Array(2);
          crypto.getRandomValues(myArray);
          console.log(myArray[0]);
          console.log(myArray[1]);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 2u);
  // Naive test for randomness: the two numbers are different.
  EXPECT_NE(test_client_->observed_console_log_messages_[0],
            test_client_->observed_console_log_messages_[1]);
}

TEST_F(SharedStorageWorkletTest, Crypto_RandomUUID) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          console.log(crypto.randomUUID());
          console.log(crypto.randomUUID());
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 2u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0].size(), 36u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[1].size(), 36u);
  // Naive test for randomness: the two numbers are different.
  EXPECT_NE(test_client_->observed_console_log_messages_[0],
            test_client_->observed_console_log_messages_[1]);
}

TEST_F(SharedStorageWorkletTest,
       TextEncoderDecoderAndSubtleCryptoEncryptDecrypt) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          let iv = crypto.getRandomValues(new Uint8Array(12));

          let key = await crypto.subtle.generateKey(
            {
              name: "AES-GCM",
              length: 256,
            },
            true,
            ["encrypt", "decrypt"]
          );

          let text = "123abc";
          let encodedText = new TextEncoder().encode(text);

          let ciphertext = await crypto.subtle.encrypt(
            {name:"AES-GCM", iv:iv}, key, encodedText);

          let decipheredText = await crypto.subtle.decrypt(
            {name:"AES-GCM", iv}, key, ciphertext);

          let decodedText = new TextDecoder().decode(decipheredText)

          console.log(decodedText);
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0], "123abc");
}

// TODO(crbug.com/1316659): When the Private Aggregation feature is removed
// (after being default enabled for a few milestones), removes these tests and
// integrate the feature-enabled tests into the broader tests.
class SharedStoragePrivateAggregationDisabledTest
    : public SharedStorageWorkletTest {
 public:
  SharedStoragePrivateAggregationDisabledTest() {
    private_aggregation_feature_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

TEST_F(SharedStoragePrivateAggregationDisabledTest,
       GlobalScopeObjectsAndFunctions_DuringAddModule) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
    var expectedObjects = [
      "console",
      "crypto"
    ];

    var expectedFunctions = [
      "SharedStorage",
      "Crypto",
      "CryptoKey",
      "SubtleCrypto",
      "TextEncoder",
      "TextDecoder",
      "register",
      "console.log"
    ];

    var expectedUndefinedVariables = [
      // PrivateAggregation related variables are undefined because the
      // corresponding base::Feature(s) are not enabled.
      "privateAggregation",
      "PrivateAggregation"
    ];

    for (let expectedObject of expectedObjects) {
      if (eval("typeof " + expectedObject) !== "object") {
        throw Error(expectedObject + " is not object type.")
      }
    }

    for (let expectedFunction of expectedFunctions) {
      if (eval("typeof " + expectedFunction) !== "function") {
        throw Error(expectedFunction + " is not function type.")
      }
    }

    for (let expectedUndefined of expectedUndefinedVariables) {
      if (eval("typeof " + expectedUndefined) !== "undefined") {
        throw Error(expectedUndefined + " is not undefined.")
      }
    }

    // Verify that trying to access `sharedStorage` would throw a custom error.
    try {
      sharedStorage;
    } catch (e) {
      console.log("Expected error:", e.message);
    }
  )");

  EXPECT_TRUE(add_module_result.success);
  EXPECT_EQ(add_module_result.error_message, "");

  EXPECT_EQ(test_client_->observed_console_log_messages_.size(), 1u);
  EXPECT_EQ(test_client_->observed_console_log_messages_[0],
            "Expected error: Failed to read the 'sharedStorage' property from "
            "'SharedStorageWorkletGlobalScope': sharedStorage cannot be "
            "accessed during addModule().");
}

TEST_F(SharedStoragePrivateAggregationDisabledTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleSuccess) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "console",
            "sharedStorage",
            "crypto"
          ];

          var expectedFunctions = [
            "SharedStorage",
            "Crypto",
            "CryptoKey",
            "SubtleCrypto",
            "TextEncoder",
            "TextDecoder",
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",

            // PrivateAggregation related variables are undefined because the
            // corresponding base::Feature(s) are not enabled.
            "privateAggregation",
            "PrivateAggregation"
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStoragePrivateAggregationDisabledTest,
       GlobalScopeObjectsAndFunctions_AfterAddModuleFailure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          var expectedObjects = [
            "console",
            "sharedStorage",
            "crypto"
          ];

          var expectedFunctions = [
            "SharedStorage",
            "Crypto",
            "CryptoKey",
            "SubtleCrypto",
            "TextEncoder",
            "TextDecoder",
            "register",
            "sharedStorage.set",
            "sharedStorage.append",
            "sharedStorage.delete",
            "sharedStorage.clear",
            "sharedStorage.get",
            "sharedStorage.length",
            "sharedStorage.keys",
            "sharedStorage.entries",
            "sharedStorage.remainingBudget"
          ];

          // Those are either not implemented yet, or should stay undefined.
          var expectedUndefinedVariables = [
            "sharedStorage.selectURL",
            "sharedStorage.run",
            "sharedStorage.worklet",
            "sharedStorage.context",

            // PrivateAggregation related variables are undefined because the
            // corresponding base::Feature(s) are not enabled.
            "privateAggregation",
            "PrivateAggregation"
          ];

          for (let expectedObject of expectedObjects) {
            if (eval("typeof " + expectedObject) !== "object") {
              throw Error(expectedObject + " is not object type.")
            }
          }

          for (let expectedFunction of expectedFunctions) {
            if (eval("typeof " + expectedFunction) !== "function") {
              throw Error(expectedFunction + " is not function type.")
            }
          }

          for (let expectedUndefined of expectedUndefinedVariables) {
            if (eval("typeof " + expectedUndefined) !== "undefined") {
              throw Error(expectedUndefined + " is not undefined.")
            }
          }
        }
      }

      register("test-operation", TestClass);

      // This should fail the addModule()
      a;
  )");

  EXPECT_FALSE(add_module_result.success);
  EXPECT_THAT(add_module_result.error_message,
              testing::HasSubstr("ReferenceError: a is not defined"));

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

class SharedStoragePrivateAggregationTest : public SharedStorageWorkletTest {
 public:
  SharedStoragePrivateAggregationTest() {
    private_aggregation_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kPrivateAggregationApi,
          {{"enabled_in_shared_storage", "true"}}}},
        /*disabled_features=*/{});
  }

  // `error_message` being `nullptr` indicates no error is expected.
  void ExecuteScriptAndValidateContribution(
      const std::string& script_body,
      absl::uint128 expected_bucket,
      int expected_value,
      mojom::blink::DebugModeDetailsPtr expected_debug_mode_details =
          mojom::blink::DebugModeDetails::New(),
      std::optional<uint64_t> filtering_id = std::nullopt,
      int filtering_id_max_bytes = 1,
      std::string* error_message = nullptr) {
    AddModuleResult add_module_result =
        AddModule(/*script_content=*/base::StrCat(
            {"class TestClass { async run() {", script_body,
             "}}; register(\"test-operation\", TestClass);"}));

    EXPECT_CALL(*mock_private_aggregation_host_, ContributeToHistogram)
        .WillOnce(testing::Invoke(
            [&](Vector<
                blink::mojom::blink::AggregatableReportHistogramContributionPtr>
                    contributions) {
              ASSERT_EQ(contributions.size(), 1u);
              EXPECT_EQ(contributions[0]->bucket, expected_bucket);
              EXPECT_EQ(contributions[0]->value, expected_value);
            }));
    if (expected_debug_mode_details->is_enabled) {
      EXPECT_CALL(*mock_private_aggregation_host_, EnableDebugMode)
          .WillOnce(testing::Invoke([&](mojom::blink::DebugKeyPtr debug_key) {
            EXPECT_TRUE(debug_key == expected_debug_mode_details->debug_key);
          }));
    }

    RunResult run_result = Run("test-operation", CreateSerializedUndefined(),
                               filtering_id_max_bytes);

    EXPECT_EQ(run_result.success, (error_message == nullptr));

    if (error_message != nullptr) {
      *error_message = run_result.error_message;
    }

    std::vector<mojom::WebFeature> expected_use_counters = {
        mojom::WebFeature::kPrivateAggregationApiAll,
        mojom::WebFeature::kPrivateAggregationApiSharedStorage};
    if (expected_debug_mode_details->is_enabled) {
      expected_use_counters.push_back(
          mojom::WebFeature::kPrivateAggregationApiEnableDebugMode);
    }
    if (filtering_id.has_value()) {
      expected_use_counters.push_back(
          mojom::WebFeature::kPrivateAggregationApiFilteringIds);
    }

    EXPECT_THAT(test_client_->observed_use_counters_,
                testing::UnorderedElementsAreArray(expected_use_counters));

    mock_private_aggregation_host_->FlushForTesting();
  }

  std::string ExecuteScriptReturningError(
      const std::string& script_body,
      std::vector<mojom::WebFeature> expect_use_counters = {},
      int filtering_id_max_bytes = 1) {
    AddModuleResult add_module_result =
        AddModule(/*script_content=*/base::StrCat(
            {"class TestClass { async run() {", script_body,
             "}}; register(\"test-operation\", TestClass);"}));

    CHECK_EQ(ShouldDefinePrivateAggregationInSharedStorage(),
             !!mock_private_aggregation_host_);

    if (mock_private_aggregation_host_) {
      EXPECT_CALL(*mock_private_aggregation_host_, ContributeToHistogram)
          .Times(0);
      EXPECT_CALL(*mock_private_aggregation_host_, EnableDebugMode).Times(0);
    }

    RunResult run_result = Run("test-operation", CreateSerializedUndefined(),
                               filtering_id_max_bytes);
    EXPECT_FALSE(run_result.success);

    EXPECT_THAT(test_client_->observed_use_counters_,
                testing::UnorderedElementsAreArray(expect_use_counters));

    if (mock_private_aggregation_host_) {
      mock_private_aggregation_host_->FlushForTesting();
    }

    return run_result.error_message;
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

TEST_F(SharedStoragePrivateAggregationTest,
       InterfaceAndObjectExposure_DuringAddModule) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
    // This will succeed.
    PrivateAggregation;

    // This will fail.
    privateAggregation;
  )");

  EXPECT_FALSE(add_module_result.success);
  EXPECT_THAT(add_module_result.error_message,
              testing::HasSubstr(
                  "privateAggregation cannot be accessed during addModule()"));
}

TEST_F(SharedStoragePrivateAggregationTest,
       InterfaceAndObjectExposure_AfterAddModuleSuccess) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          PrivateAggregation;
          privateAggregation;
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_TRUE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStoragePrivateAggregationTest,
       InterfaceAndObjectExposure_AfterAddModuleFailure) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          PrivateAggregation;
          privateAggregation;
        }
      }

      register("test-operation", TestClass);

      // This should fail the addModule()
      a;
  )");

  EXPECT_FALSE(add_module_result.success);

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());

  EXPECT_TRUE(run_result.success);
  EXPECT_EQ(run_result.error_message, "");
}

TEST_F(SharedStoragePrivateAggregationTest, BasicTest) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2});",
      /*expected_bucket=*/1, /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, ZeroBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 0n, value: 2});",
      /*expected_bucket=*/0, /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, ZeroValue) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 0});",
      /*expected_bucket=*/1, /*expected_value=*/0);
}

TEST_F(SharedStoragePrivateAggregationTest, LargeBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 18446744073709551616n, value: 2});",
      /*expected_bucket=*/absl::MakeUint128(/*high=*/1, /*low=*/0),
      /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, MaxBucket) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 340282366920938463463374607431768211455n, value: 2});",
      /*expected_bucket=*/absl::Uint128Max(), /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest, NonIntegerValue) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2.3});",
      /*expected_bucket=*/1, /*expected_value=*/2);
}

TEST_F(SharedStoragePrivateAggregationTest,
       PrivateAggregationPermissionsPolicyNotAllowed_Rejected) {
  private_aggregation_permissions_policy_allowed_ = false;

  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage});

  EXPECT_THAT(error_str, testing::HasSubstr(
                             "The \"private-aggregation\" Permissions Policy "
                             "denied the method on privateAggregation"));
}

TEST_F(SharedStoragePrivateAggregationTest, TooLargeBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram({bucket: "
      "340282366920938463463374607431768211456n, value: 2});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage});

  EXPECT_THAT(
      error_str,
      testing::HasSubstr(
          "contribution['bucket'] is negative or does not fit in 128 bits"));
}

TEST_F(SharedStoragePrivateAggregationTest, NegativeBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram({bucket: -1n, value: 2});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage});

  EXPECT_THAT(
      error_str,
      testing::HasSubstr(
          "contribution['bucket'] is negative or does not fit in 128 bits"));
}

TEST_F(SharedStoragePrivateAggregationTest, NonBigIntBucket_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram({bucket: 1, value: 2});",
      /*expect_use_counters=*/{});

  EXPECT_THAT(error_str, testing::HasSubstr("Cannot convert 1 to a BigInt"));
}

TEST_F(SharedStoragePrivateAggregationTest, NegativeValue_Rejected) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: -1});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage});

  EXPECT_THAT(error_str,
              testing::HasSubstr("contribution['value'] is negative"));
}

TEST_F(SharedStoragePrivateAggregationTest,
       InvalidEnableDebugModeArgument_Rejected) {
  // The debug key is not wrapped in a dictionary.
  std::string error_str =
      ExecuteScriptReturningError("privateAggregation.enableDebugMode(1234n);",
                                  /*expect_use_counters=*/{});

  EXPECT_THAT(error_str,
              testing::HasSubstr("The provided value is not of type "
                                 "'PrivateAggregationDebugModeOptions'"));
}

TEST_F(SharedStoragePrivateAggregationTest,
       EnableDebugModeCalledTwice_SecondCallFails) {
  std::string error_str;

  // Note that the first call still applies to future requests if the error is
  // caught. Here, we rethrow it to check its value.
  ExecuteScriptAndValidateContribution(
      R"(
        let error;
        try {
          privateAggregation.enableDebugMode({debugKey: 1234n});
          privateAggregation.enableDebugMode();
        } catch (e) {
          error = e;
        }
        privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
        throw error;
      )",
      /*expected_bucket=*/1,
      /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      mojom::blink::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/mojom::blink::DebugKey::New(1234u)),
      /*filtering_id=*/std::nullopt,
      /*filtering_id_max_bytes=*/1, &error_str);

  EXPECT_THAT(error_str,
              testing::HasSubstr("enableDebugMode may be called at most once"));
}

// Note that FLEDGE worklets have different behavior in this case.
TEST_F(SharedStoragePrivateAggregationTest,
       EnableDebugModeCalledAfterRequest_DoesntApply) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class ContributeToHistogram {
        async run() {
          privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
        }
      }

      class EnableDebugMode {
        async run() {
          privateAggregation.enableDebugMode({debugKey: 1234n});
        }
      }

      register("contribute-to-histogram", ContributeToHistogram);
      register("enable-debug-mode", EnableDebugMode);
  )");

  std::optional<mojo::ReceiverId> contribute_to_histogram_pipe_id;
  std::optional<mojo::ReceiverId> enable_debug_mode_pipe_id;
  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(*mock_private_aggregation_host_, ContributeToHistogram)
      .WillOnce(testing::Invoke(
          [&](Vector<
              blink::mojom::blink::AggregatableReportHistogramContributionPtr>
                  contributions) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);

            contribute_to_histogram_pipe_id =
                mock_private_aggregation_host_->receiver_set()
                    .current_receiver();
            closure.Run();
          }));
  EXPECT_CALL(*mock_private_aggregation_host_, EnableDebugMode)
      .WillOnce(
          testing::Invoke([&](blink::mojom::blink::DebugKeyPtr debug_key) {
            ASSERT_FALSE(debug_key.is_null());
            EXPECT_EQ(debug_key->value, 1234u);

            enable_debug_mode_pipe_id =
                mock_private_aggregation_host_->receiver_set()
                    .current_receiver();
            closure.Run();
          }));

  RunResult run_result =
      Run("contribute-to-histogram", CreateSerializedUndefined());
  EXPECT_TRUE(run_result.success);

  RunResult run_result2 = Run("enable-debug-mode", CreateSerializedUndefined());
  EXPECT_TRUE(run_result2.success);

  mock_private_aggregation_host_->FlushForTesting();
  run_loop.Run();

  // The calls should've come on two different pipes.
  EXPECT_TRUE(contribute_to_histogram_pipe_id.has_value());
  EXPECT_TRUE(enable_debug_mode_pipe_id.has_value());
  EXPECT_NE(contribute_to_histogram_pipe_id, enable_debug_mode_pipe_id);
}

TEST_F(SharedStoragePrivateAggregationTest, MultipleDebugModeRequests) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          privateAggregation.enableDebugMode({debugKey: 1234n});
          privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
          privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
        }
      }

      register("test-operation", TestClass);
  )");

  EXPECT_CALL(*mock_private_aggregation_host_, EnableDebugMode)
      .WillOnce(testing::Invoke([](mojom::blink::DebugKeyPtr debug_key) {
        EXPECT_EQ(debug_key, mojom::blink::DebugKey::New(1234u));
      }));

  EXPECT_CALL(*mock_private_aggregation_host_, ContributeToHistogram)
      .WillOnce(testing::Invoke(
          [](Vector<
              blink::mojom::blink::AggregatableReportHistogramContributionPtr>
                 contributions) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);
          }))
      .WillOnce(testing::Invoke(
          [](Vector<
              blink::mojom::blink::AggregatableReportHistogramContributionPtr>
                 contributions) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0]->bucket, 3);
            EXPECT_EQ(contributions[0]->value, 4);
          }));

  RunResult run_result = Run("test-operation", CreateSerializedUndefined());
  EXPECT_TRUE(run_result.success);

  mock_private_aggregation_host_->FlushForTesting();
}

// Regression test for crbug.com/1429895.
TEST_F(SharedStoragePrivateAggregationTest,
       GlobalScopeDeletedBeforeOperationCompletes_ContributionsStillFlushed) {
  AddModuleResult add_module_result = AddModule(/*script_content=*/R"(
      class TestClass {
        async run() {
          privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
          await new Promise(() => {});
        }
      }

      register("test-operation", TestClass);
  )");

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_private_aggregation_host_, EnableDebugMode).Times(0);
  EXPECT_CALL(*mock_private_aggregation_host_, ContributeToHistogram)
      .WillOnce(testing::Invoke(
          [&](Vector<
              blink::mojom::blink::AggregatableReportHistogramContributionPtr>
                  contributions) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0]->bucket, 1);
            EXPECT_EQ(contributions[0]->value, 2);

            run_loop.Quit();
          }));

  shared_storage_worklet_service_->RunOperation(
      "test-operation", CreateSerializedUndefined(),
      MaybeInitPAOperationDetails(), base::DoNothing());

  // Trigger the disconnect handler.
  shared_storage_worklet_service_.reset();

  // Callback called means the worklet has terminated successfully.
  EXPECT_TRUE(worklet_terminated_future_.Wait());

  run_loop.Run();
}

class SharedStoragePrivateAggregationFilteringIdTest
    : public SharedStoragePrivateAggregationTest {
 public:
  SharedStoragePrivateAggregationFilteringIdTest() = default;

 private:
  // The features are not necessarily synchronized in the unit test, so we
  // enable both.
  base::test::ScopedFeatureList scoped_base_feature_{
      features::kPrivateAggregationApiFilteringIds};
  ScopedPrivateAggregationApiFilteringIdsForTest scoped_rte_feature{
      /*enabled=*/true};
};

TEST_F(SharedStoragePrivateAggregationFilteringIdTest, BasicFilteringId) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 3n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/3);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       FilteringIdWithDebugMode) {
  ExecuteScriptAndValidateContribution(
      R"(privateAggregation.enableDebugMode();
         privateAggregation.contributeToHistogram(
             {bucket: 1n, value: 2, filteringId: 3n});)",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/
      mojom::blink::DebugModeDetails::New(/*is_enabled=*/true,
                                          /*debug_key=*/nullptr),
      /*filtering_id=*/3);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       NoFilteringIdSpecified_FilteringIdNull) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::nullopt);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       ExplicitDefaultFilteringId_FilteringIdNotNull) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 0n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/0);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       MaxFilteringIdForByteSize_Success) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 255n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/255);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       FilteringIdTooBigForByteSize_Error) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 256n});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage,
          mojom::WebFeature::kPrivateAggregationApiFilteringIds});

  EXPECT_THAT(error_str,
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       FilteringIdNegative_Error) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: -1n});",
      /*expect_use_counters=*/{
          mojom::WebFeature::kPrivateAggregationApiAll,
          mojom::WebFeature::kPrivateAggregationApiSharedStorage,
          mojom::WebFeature::kPrivateAggregationApiFilteringIds});

  EXPECT_THAT(error_str,
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       NoFilteringIdWithCustomByteSize) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::nullopt, /*filtering_id_max_bytes=*/3);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       FilteringIdWithCustomByteSize_Success) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 3n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/3, /*filtering_id_max_bytes=*/3);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       MaxFilteringIdWithCustomByteSize_Success) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 16777215n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/16777215, /*filtering_id_max_bytes=*/3);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       TooBigFilteringIdWithCustomByteSize_Error) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 16777216n});",
      /*expect_use_counters=*/
      {mojom::WebFeature::kPrivateAggregationApiAll,
       mojom::WebFeature::kPrivateAggregationApiSharedStorage,
       mojom::WebFeature::kPrivateAggregationApiFilteringIds},
      /*filtering_id_max_bytes=*/3);

  EXPECT_THAT(error_str,
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest, MaxPossibleFilteringId) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: (1n << 64n) - 1n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::numeric_limits<uint64_t>::max(),
      /*filtering_id_max_bytes=*/8);
}

TEST_F(SharedStoragePrivateAggregationFilteringIdTest,
       TooBigFilteringIdWithMaxByteSize_Error) {
  std::string error_str = ExecuteScriptReturningError(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: (1n << 64n)});",
      /*expect_use_counters=*/
      {mojom::WebFeature::kPrivateAggregationApiAll,
       mojom::WebFeature::kPrivateAggregationApiSharedStorage,
       mojom::WebFeature::kPrivateAggregationApiFilteringIds},
      /*filtering_id_max_bytes=*/8);

  EXPECT_THAT(error_str,
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
}

class SharedStoragePrivateAggregationFilteringIdDisabledTest
    : public SharedStoragePrivateAggregationTest {
 public:
  SharedStoragePrivateAggregationFilteringIdDisabledTest() {
    scoped_base_feature_.InitAndDisableFeature(
        features::kPrivateAggregationApiFilteringIds);
  }

 private:
  // The features are not necessarily synchronized in the unit test, so we
  // disable both.
  base::test::ScopedFeatureList scoped_base_feature_;
  ScopedPrivateAggregationApiFilteringIdsForTest scoped_rte_feature{
      /*enabled=*/false};
};

TEST_F(SharedStoragePrivateAggregationFilteringIdDisabledTest,
       ValidFilteringId_Ignored) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: 3n});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::nullopt);
}
TEST_F(SharedStoragePrivateAggregationFilteringIdDisabledTest,
       InvalidFilteringId_Ignored) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram("
      "{bucket: 1n, value: 2, filteringId: -1});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::nullopt);
}
TEST_F(SharedStoragePrivateAggregationFilteringIdDisabledTest,
       CustomFilteringIdMaxBytes_Ignored) {
  ExecuteScriptAndValidateContribution(
      "privateAggregation.contributeToHistogram({bucket: 1n, value: 2});",
      /*expected_bucket=*/1, /*expected_value=*/2,
      /*expected_debug_mode_details=*/mojom::blink::DebugModeDetails::New(),
      /*filtering_id=*/std::nullopt, /*filtering_id_max_bytes=*/3);
}

class SharedStorageWorkletThreadTest : public testing::Test {};

// Assert that each `SharedStorageWorkletThread` owns a dedicated
// `WorkerBackingThread`.
TEST_F(SharedStorageWorkletThreadTest, DedicatedBackingThread) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      blink::features::kSharedStorageWorkletSharedBackingThreadImplementation);

  test::TaskEnvironment task_environment;

  MockWorkerReportingProxy reporting_proxy1;
  MockWorkerReportingProxy reporting_proxy2;
  auto thread1 = SharedStorageWorkletThread::Create(reporting_proxy1);
  auto thread2 = SharedStorageWorkletThread::Create(reporting_proxy2);
  EXPECT_NE(&thread1->GetWorkerBackingThread(),
            &thread2->GetWorkerBackingThread());

  // Start and terminate the threads, so that the test can terminate gracefully.
  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;

  thread1->Start(MakeTestGlobalScopeCreationParams(), thread_startup_data,
                 std::make_unique<WorkerDevToolsParams>());
  thread2->Start(MakeTestGlobalScopeCreationParams(), thread_startup_data,
                 std::make_unique<WorkerDevToolsParams>());

  thread1->TerminateForTesting();
  thread1->WaitForShutdownForTesting();
  thread2->TerminateForTesting();
  thread2->WaitForShutdownForTesting();
}

// Assert that multiple `SharedStorageWorkletThread`s share a
// `WorkerBackingThread`.
//
// Note: Currently, this would trigger a crash due to a failure in installing
// the `v8/expose_gc` extension. Even though `--expose-gc` isn't set by default
// in production, we should still fix this.
//
// TODO(yaoxia): We're temporarily leaving this issue unfixed to facilitate our
// investigation into a crash that occurs in the wild (crbug.com/1501387). We'll
// re-enable this after investigation.
TEST_F(SharedStorageWorkletThreadTest, DISABLED_SharedBackingThread) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kSharedStorageWorkletSharedBackingThreadImplementation);

  test::TaskEnvironment task_environment;
  MockWorkerReportingProxy reporting_proxy1;
  MockWorkerReportingProxy reporting_proxy2;
  auto thread1 = SharedStorageWorkletThread::Create(reporting_proxy1);
  auto thread2 = SharedStorageWorkletThread::Create(reporting_proxy2);
  EXPECT_EQ(&thread1->GetWorkerBackingThread(),
            &thread2->GetWorkerBackingThread());

  // Start and terminate the threads, so that the test can terminate gracefully.
  thread1->Start(MakeTestGlobalScopeCreationParams(),
                 /*thread_startup_data=*/std::nullopt,
                 std::make_unique<WorkerDevToolsParams>());
  thread2->Start(MakeTestGlobalScopeCreationParams(),
                 /*thread_startup_data=*/std::nullopt,
                 std::make_unique<WorkerDevToolsParams>());

  thread1->TerminateForTesting();
  thread1->WaitForShutdownForTesting();
  thread2->TerminateForTesting();
  thread2->WaitForShutdownForTesting();
}

}  // namespace blink
