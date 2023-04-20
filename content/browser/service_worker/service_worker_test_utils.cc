// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_test_utils.h"
#include "base/memory/raw_ref.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/test/policy_container_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

// The minimal DidCommitProvisionalLoadParams passing mojom validation.
mojom::DidCommitProvisionalLoadParamsPtr
MinimalDidCommitNavigationLoadParams() {
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->referrer = blink::mojom::Referrer::New();
  params->navigation_token = base::UnguessableToken::Create();
  return params;
}

class FakeNavigationClient : public mojom::NavigationClient {
 public:
  using ReceivedProviderInfoCallback = base::OnceCallback<void(
      blink::mojom::ServiceWorkerContainerInfoForClientPtr)>;

  explicit FakeNavigationClient(
      ReceivedProviderInfoCallback on_received_callback)
      : on_received_callback_(std::move(on_received_callback)) {}

  FakeNavigationClient(const FakeNavigationClient&) = delete;
  FakeNavigationClient& operator=(const FakeNavigationClient&) = delete;

  ~FakeNavigationClient() override = default;

 private:
  // mojom::NavigationClient implementation:
  void CommitNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          topics_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          keep_alive_loader_factory,
      const blink::DocumentToken& document_token,
      const base::UnguessableToken& devtools_navigation_token,
      const absl::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
      mojo::PendingRemote<blink::mojom::ResourceCache> resource_cache,
      mojom::CookieManagerInfoPtr cookie_manager_info,
      mojom::StorageInfoPtr storage_info,
      CommitNavigationCallback callback) override {
    std::move(on_received_callback_).Run(std::move(container_info));
    std::move(callback).Run(MinimalDidCommitNavigationLoadParams(), nullptr);
  }
  void CommitFailedNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      int extended_error_code,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      const blink::DocumentToken& document_token,
      blink::mojom::PolicyContainerPtr policy_container,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      CommitFailedNavigationCallback callback) override {
    std::move(callback).Run(MinimalDidCommitNavigationLoadParams(), nullptr);
  }

  ReceivedProviderInfoCallback on_received_callback_;
};

class ResourceWriter {
 public:
  ResourceWriter(
      const mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
      const GURL& script_url,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const std::string& body,
      const std::string& meta_data)
      : storage_(storage),
        script_url_(script_url),
        headers_(headers),
        body_(body),
        meta_data_(meta_data) {}

  void Start(WriteToDiskCacheCallback callback) {
    DCHECK(storage_->is_connected());
    callback_ = std::move(callback);
    (*storage_)->GetNewResourceId(base::BindOnce(
        &ResourceWriter::DidGetResourceId, base::Unretained(this)));
  }

  void StartWithResourceId(int64_t resource_id,
                           WriteToDiskCacheCallback callback) {
    DCHECK(storage_->is_connected());
    callback_ = std::move(callback);
    DidGetResourceId(resource_id);
  }

 private:
  void DidGetResourceId(int64_t resource_id) {
    DCHECK(storage_->is_connected());
    DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);

    resource_id_ = resource_id;
    (*storage_)->CreateResourceWriter(
        resource_id, body_writer_.BindNewPipeAndPassReceiver());
    (*storage_)->CreateResourceMetadataWriter(
        resource_id, metadata_writer_.BindNewPipeAndPassReceiver());

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n"));
    response_head->request_time = base::Time::Now();
    response_head->response_time = base::Time::Now();
    response_head->content_length = body_.size();
    for (const auto& header : headers_)
      response_head->headers->AddHeader(header.first, header.second);

    body_writer_->WriteResponseHead(
        std::move(response_head),
        base::BindOnce(&ResourceWriter::DidWriteResponseHead,
                       base::Unretained(this)));
  }

  void DidWriteResponseHead(int result) {
    DCHECK_GE(result, 0);
    mojo_base::BigBuffer buffer(base::as_bytes(base::make_span(body_)));
    body_writer_->WriteData(
        std::move(buffer),
        base::BindOnce(&ResourceWriter::DidWriteData, base::Unretained(this)));
  }

  void DidWriteData(int result) {
    DCHECK_EQ(result, static_cast<int>(body_.size()));
    mojo_base::BigBuffer buffer(base::as_bytes(base::make_span(meta_data_)));
    metadata_writer_->WriteMetadata(
        std::move(buffer), base::BindOnce(&ResourceWriter::DidWriteMetadata,
                                          base::Unretained(this)));
  }

  void DidWriteMetadata(int result) {
    DCHECK_EQ(result, static_cast<int>(meta_data_.size()));
    std::move(callback_).Run(storage::mojom::ServiceWorkerResourceRecord::New(
        resource_id_, script_url_, body_.size(), /*sha256_checksum=*/""));
  }

  const raw_ref<const mojo::Remote<storage::mojom::ServiceWorkerStorageControl>>
      storage_;
  const GURL script_url_;
  const std::vector<std::pair<std::string, std::string>> headers_;
  const std::string body_;
  const std::string meta_data_;
  WriteToDiskCacheCallback callback_;

  int64_t resource_id_ = blink::mojom::kInvalidServiceWorkerResourceId;
  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> body_writer_;
  mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>
      metadata_writer_;
};

void OnWriteToDiskCacheFinished(
    std::unique_ptr<ResourceWriter> self_owned_writer,
    WriteToDiskCacheCallback callback,
    storage::mojom::ServiceWorkerResourceRecordPtr record) {
  std::move(callback).Run(std::move(record));
}

}  // namespace

ServiceWorkerRemoteContainerEndpoint::ServiceWorkerRemoteContainerEndpoint() =
    default;
ServiceWorkerRemoteContainerEndpoint::ServiceWorkerRemoteContainerEndpoint(
    ServiceWorkerRemoteContainerEndpoint&& other)
    : navigation_client_(std::move(other.navigation_client_)),
      host_remote_(std::move(other.host_remote_)),
      client_receiver_(std::move(other.client_receiver_)) {}

ServiceWorkerRemoteContainerEndpoint::~ServiceWorkerRemoteContainerEndpoint() =
    default;

void ServiceWorkerRemoteContainerEndpoint::BindForWindow(
    blink::mojom::ServiceWorkerContainerInfoForClientPtr info) {
  // We establish a message pipe for connecting |navigation_client_| to a fake
  // navigation client, then simulate sending the navigation commit IPC which
  // carries a service worker container info over it, then the container info
  // received there gets its |host_remote| and |client_receiver| associated
  // with a message pipe so that their users later can make Mojo calls without
  // crash.
  blink::mojom::ServiceWorkerContainerInfoForClientPtr received_info;
  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeNavigationClient>(base::BindOnce(
          [](base::OnceClosure quit_closure,
             blink::mojom::ServiceWorkerContainerInfoForClientPtr* out_info,
             blink::mojom::ServiceWorkerContainerInfoForClientPtr info) {
            *out_info = std::move(info);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), &received_info)),
      navigation_client_.BindNewPipeAndPassReceiver());

  navigation_client_->CommitNavigation(
      blink::CreateCommonNavigationParams(),
      blink::CreateCommitNavigationParams(),
      network::mojom::URLResponseHead::New(),
      mojo::ScopedDataPipeConsumerHandle(),
      /*url_loader_client_endpoints=*/nullptr,
      /*subresource_loader_factories=*/nullptr,
      /*subresource_overrides=*/absl::nullopt,
      /*controller_service_worker_info=*/nullptr, std::move(info),
      /*prefetch_loader_factory=*/mojo::NullRemote(),
      /*topics_loader_factory=*/mojo::NullRemote(),
      /*keep_alive_loader_factory=*/mojo::NullRemote(), blink::DocumentToken(),
      base::UnguessableToken::Create(),
      std::vector<blink::ParsedPermissionsPolicyDeclaration>(),
      CreateStubPolicyContainer(), /*code_cache_host=*/mojo::NullRemote(),
      /*resource_cache=*/mojo::NullRemote(), /*cookie_manager_info=*/nullptr,
      /*storage_info=*/nullptr,
      base::BindOnce(
          [](mojom::DidCommitProvisionalLoadParamsPtr validated_params,
             mojom::DidCommitProvisionalLoadInterfaceParamsPtr
                 interface_params) {}));
  loop.Run();

  client_receiver_ = std::move(received_info->client_receiver);
  host_remote_.Bind(std::move(received_info->host_remote));
}

void ServiceWorkerRemoteContainerEndpoint::BindForServiceWorker(
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr info) {
  host_remote_.Bind(std::move(info->host_remote));
}

ServiceWorkerContainerHostAndInfo::ServiceWorkerContainerHostAndInfo(
    base::WeakPtr<ServiceWorkerContainerHost> host,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr info)
    : host(std::move(host)), info(std::move(info)) {}

ServiceWorkerContainerHostAndInfo::~ServiceWorkerContainerHostAndInfo() =
    default;

base::WeakPtr<ServiceWorkerContainerHost> CreateContainerHostForWindow(
    const GlobalRenderFrameHostId& render_frame_host_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteContainerEndpoint* output_endpoint) {
  std::unique_ptr<ServiceWorkerContainerHostAndInfo> host_and_info =
      CreateContainerHostAndInfoForWindow(context, is_parent_frame_secure);
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      std::move(host_and_info->host);
  output_endpoint->BindForWindow(std::move(host_and_info->info));

  // Establish a dummy connection to allow sending messages without errors.
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      reporter;
  auto dummy = reporter.InitWithNewPipeAndPassReceiver();

  // In production code this is called from NavigationRequest in the browser
  // process right before navigation commit.
  container_host->OnBeginNavigationCommit(
      render_frame_host_id, PolicyContainerPolicies(), std::move(reporter),
      ukm::kInvalidSourceId);
  return container_host;
}

std::unique_ptr<ServiceWorkerContainerHostAndInfo>
CreateContainerHostAndInfoForWindow(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool are_ancestors_secure) {
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  auto info = blink::mojom::ServiceWorkerContainerInfoForClient::New();
  info->client_receiver = client_remote.InitWithNewEndpointAndPassReceiver();
  host_receiver = info->host_remote.InitWithNewEndpointAndPassReceiver();
  return std::make_unique<ServiceWorkerContainerHostAndInfo>(
      context->CreateContainerHostForWindow(
          std::move(host_receiver), are_ancestors_secure,
          std::move(client_remote), /*frame_tree_node_id=*/1),
      std::move(info));
}

base::OnceCallback<void(blink::ServiceWorkerStatusCode)>
ReceiveServiceWorkerStatus(absl::optional<blink::ServiceWorkerStatusCode>* out,
                           base::OnceClosure quit_closure) {
  return base::BindOnce(
      [](base::OnceClosure quit_closure,
         absl::optional<blink::ServiceWorkerStatusCode>* out,
         blink::ServiceWorkerStatusCode result) {
        *out = result;
        std::move(quit_closure).Run();
      },
      std::move(quit_closure), out);
}

blink::ServiceWorkerStatusCode WarmUpServiceWorker(
    ServiceWorkerVersion* version) {
  blink::ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  version->StartWorker(ServiceWorkerMetrics::EventType::WARM_UP,
                       base::BindLambdaForTesting(
                           [&](blink::ServiceWorkerStatusCode result_status) {
                             status = result_status;
                             run_loop.Quit();
                           }));
  run_loop.Run();
  return status;
}

blink::ServiceWorkerStatusCode StartServiceWorker(
    ServiceWorkerVersion* version) {
  blink::ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  version->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                       base::BindLambdaForTesting(
                           [&](blink::ServiceWorkerStatusCode result_status) {
                             status = result_status;
                             run_loop.Quit();
                           }));
  run_loop.Run();
  return status;
}

void StopServiceWorker(ServiceWorkerVersion* version) {
  base::RunLoop run_loop;
  version->StopWorker(run_loop.QuitClosure());
  run_loop.Run();
}

std::unique_ptr<ServiceWorkerHost> CreateServiceWorkerHost(
    int process_id,
    bool is_parent_frame_secure,
    ServiceWorkerVersion* hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteContainerEndpoint* output_endpoint) {
  auto provider_info =
      blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
  auto host = std::make_unique<ServiceWorkerHost>(
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver(),
      hosted_version, std::move(context));

  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      pending_interface_provider;

  host->CompleteStartWorkerPreparation(
      process_id,
      provider_info->browser_interface_broker.InitWithNewPipeAndPassReceiver(),
      pending_interface_provider.InitWithNewPipeAndPassRemote());
  output_endpoint->BindForServiceWorker(std::move(provider_info));
  return host;
}

scoped_refptr<ServiceWorkerRegistration> CreateNewServiceWorkerRegistration(
    ServiceWorkerRegistry* registry,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  // Using nestable run loop because:
  // * The CreateNewRegistration() internally uses a mojo remote and the
  //   receiver of the remote lives in the same process/sequence in tests.
  // * When the receiver lives in the same process, a nested task is posted so
  //   that a mojo message sent to the receiver can be dispatched.
  // * Default run loop doesn't execute nested tasks. Tests will hang when
  //   default run loop is used.
  // TODO(bashi): Figure out a way to avoid using nested loop as it's
  // problematic.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  registry->CreateNewRegistration(
      options, key, blink::mojom::AncestorFrameType::kNormalFrame,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerRegistration> new_registration) {
            registration = std::move(new_registration);
            run_loop.Quit();
          }));
  run_loop.Run();
  DCHECK(registration);
  return registration;
}

scoped_refptr<ServiceWorkerVersion> CreateNewServiceWorkerVersion(
    ServiceWorkerRegistry* registry,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type) {
  scoped_refptr<ServiceWorkerVersion> version;
  // See comments in CreateNewServiceWorkerRegistration() why nestable tasks
  // allowed.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  registry->CreateNewVersion(
      std::move(registration), script_url, script_type,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerVersion> new_version) {
            version = std::move(new_version);
            run_loop.Quit();
          }));
  run_loop.Run();
  DCHECK(version);
  return version;
}

scoped_refptr<ServiceWorkerRegistration>
CreateServiceWorkerRegistrationAndVersion(ServiceWorkerContextCore* context,
                                          const GURL& scope,
                                          const GURL& script,
                                          const blink::StorageKey& key,
                                          int64_t resource_id) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateNewServiceWorkerRegistration(context->registry(), options, key);
  scoped_refptr<ServiceWorkerVersion> version =
      CreateNewServiceWorkerVersion(context->registry(), registration.get(),
                                    script, blink::mojom::ScriptType::kClassic);
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      resource_id, script,
      /*size_bytes=*/100, /*sha256_checksum=*/""));
  version->script_cache_map()->SetResources(records);
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetWaitingVersion(version);
  return registration;
}

storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheWithIdSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    int64_t resource_id,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data) {
  storage::mojom::ServiceWorkerResourceRecordPtr record;
  ResourceWriter writer(storage, script_url, headers, body, meta_data);
  base::RunLoop loop;
  writer.StartWithResourceId(
      resource_id,
      base::BindLambdaForTesting(
          [&](storage::mojom::ServiceWorkerResourceRecordPtr result) {
            record = std::move(result);
            loop.Quit();
          }));
  loop.Run();
  return record;
}

storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data) {
  storage::mojom::ServiceWorkerResourceRecordPtr record;
  ResourceWriter writer(storage, script_url, headers, body, meta_data);
  base::RunLoop loop;
  writer.Start(base::BindLambdaForTesting(
      [&](storage::mojom::ServiceWorkerResourceRecordPtr result) {
        record = std::move(result);
        loop.Quit();
      }));
  loop.Run();
  return record;
}

void WriteToDiskCacheAsync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    WriteToDiskCacheCallback callback) {
  auto writer = std::make_unique<ResourceWriter>(storage, script_url, headers,
                                                 body, meta_data);
  auto* raw_writer = writer.get();
  raw_writer->Start(base::BindOnce(&OnWriteToDiskCacheFinished,
                                   std::move(writer), std::move(callback)));
}

int64_t GetNewResourceIdSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage) {
  base::RunLoop run_loop;
  int64_t resource_id;
  storage->GetNewResourceId(
      base::BindLambdaForTesting([&](int64_t new_resource_id) {
        DCHECK_NE(new_resource_id,
                  blink::mojom::kInvalidServiceWorkerResourceId);
        resource_id = new_resource_id;
        run_loop.Quit();
      }));
  run_loop.Run();
  return resource_id;
}

MockServiceWorkerResourceReader::MockServiceWorkerResourceReader() = default;

MockServiceWorkerResourceReader::~MockServiceWorkerResourceReader() = default;

mojo::PendingRemote<storage::mojom::ServiceWorkerResourceReader>
MockServiceWorkerResourceReader::BindNewPipeAndPassRemote(
    base::OnceClosure disconnect_handler) {
  auto remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
  return remote;
}

void MockServiceWorkerResourceReader::ReadResponseHead(
    storage::mojom::ServiceWorkerResourceReader::ReadResponseHeadCallback
        callback) {
  pending_read_response_head_callback_ = std::move(callback);
}

void MockServiceWorkerResourceReader::PrepareReadData(
    int64_t,
    PrepareReadDataCallback callback) {
  DCHECK(!body_.is_valid());
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = expected_max_data_bytes_;
  mojo::CreateDataPipe(&options, body_, consumer);
  std::move(callback).Run(std::move(std::move(consumer)));
}

void MockServiceWorkerResourceReader::ReadData(ReadDataCallback callback) {
  // Calling `callback` anyway just to satisfy mojo constraint, but the timing
  // and the argument are incorrect (e.g. `callback` should be called after all
  // reads are completed). So far this is OK because no one in tests checks the
  // response here.
  std::move(callback).Run(0);
}

void MockServiceWorkerResourceReader::ExpectReadResponseHead(size_t len,
                                                             int result) {
  expected_reads_.push(ExpectedRead(len, result));
}

void MockServiceWorkerResourceReader::ExpectReadResponseHeadOk(size_t len) {
  expected_reads_.push(ExpectedRead(len, len));
}

void MockServiceWorkerResourceReader::ExpectReadData(const char* data,
                                                     size_t len,
                                                     int result) {
  expected_max_data_bytes_ = std::max(expected_max_data_bytes_, len);
  expected_reads_.push(ExpectedRead(data, len, result));
}

void MockServiceWorkerResourceReader::ExpectReadDataOk(
    const std::string& data) {
  expected_reads_.push(ExpectedRead(data.data(), data.size(), data.size()));
}

void MockServiceWorkerResourceReader::ExpectReadOk(
    const std::vector<std::string>& stored_data,
    const size_t bytes_stored) {
  ExpectReadResponseHeadOk(bytes_stored);
  for (const auto& data : stored_data)
    ExpectReadDataOk(data);
}

void MockServiceWorkerResourceReader::CompletePendingRead() {
  DCHECK(!expected_reads_.empty());
  ExpectedRead expected = expected_reads_.front();
  expected_reads_.pop();

  // Make sure that all messages are received at this point.
  receiver_.FlushForTesting();

  if (expected.is_head) {
    DCHECK(pending_read_response_head_callback_);
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.0 200 OK\0\0");
    response_head->content_length = expected.len;
    std::move(pending_read_response_head_callback_)
        .Run(expected.result, std::move(response_head),
             /*metadata=*/absl::nullopt);
  } else {
    if (expected.len == 0) {
      body_.reset();
    } else {
      DCHECK(body_.is_valid());
      EXPECT_TRUE(mojo::BlockingCopyFromString(
          std::string(expected.data, expected.len), body_));
    }
  }

  // Wait until the body is received by the user of the response reader.
  base::RunLoop().RunUntilIdle();
}

MockServiceWorkerResourceWriter::MockServiceWorkerResourceWriter() = default;

MockServiceWorkerResourceWriter::~MockServiceWorkerResourceWriter() = default;

mojo::PendingRemote<storage::mojom::ServiceWorkerResourceWriter>
MockServiceWorkerResourceWriter::BindNewPipeAndPassRemote(
    base::OnceClosure disconnect_handler) {
  auto remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
  return remote;
}

void MockServiceWorkerResourceWriter::WriteResponseHead(
    network::mojom::URLResponseHeadPtr response_head,
    WriteResponseHeadCallback callback) {
  DCHECK(!expected_writes_.empty());
  ExpectedWrite write = expected_writes_.front();
  EXPECT_TRUE(write.is_head);
  if (write.result > 0) {
    EXPECT_EQ(write.length, static_cast<size_t>(response_head->content_length));
    head_written_ += response_head->content_length;
  }
  pending_callback_ = std::move(callback);
}

void MockServiceWorkerResourceWriter::WriteData(mojo_base::BigBuffer data,
                                                WriteDataCallback callback) {
  DCHECK(!expected_writes_.empty());
  ExpectedWrite write = expected_writes_.front();
  EXPECT_FALSE(write.is_head);
  if (write.result > 0) {
    EXPECT_EQ(write.length, data.size());
    data_written_ += data.size();
  }
  pending_callback_ = std::move(callback);
}

void MockServiceWorkerResourceWriter::ExpectWriteResponseHeadOk(size_t length) {
  ExpectWriteResponseHead(length, length);
}

void MockServiceWorkerResourceWriter::ExpectWriteDataOk(size_t length) {
  ExpectWriteData(length, length);
}

void MockServiceWorkerResourceWriter::ExpectWriteResponseHead(size_t length,
                                                              int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  ExpectedWrite expected(true, length, result);
  expected_writes_.push(expected);
}

void MockServiceWorkerResourceWriter::ExpectWriteData(size_t length,
                                                      int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  ExpectedWrite expected(false, length, result);
  expected_writes_.push(expected);
}

void MockServiceWorkerResourceWriter::CompletePendingWrite() {
  // Make sure that all messages are received at this point.
  receiver_.FlushForTesting();

  DCHECK(!expected_writes_.empty());
  DCHECK(pending_callback_);
  ExpectedWrite write = expected_writes_.front();
  expected_writes_.pop();
  std::move(pending_callback_).Run(write.result);
  // Wait until |pending_callback_| finishes.
  base::RunLoop().RunUntilIdle();
}

ServiceWorkerUpdateCheckTestUtils::ServiceWorkerUpdateCheckTestUtils() =
    default;
ServiceWorkerUpdateCheckTestUtils::~ServiceWorkerUpdateCheckTestUtils() =
    default;

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerUpdateCheckTestUtils::CreatePausedCacheWriter(
    EmbeddedWorkerTestHelper* worker_test_helper,
    size_t bytes_compared,
    const std::string& new_headers,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_size,
    int64_t old_resource_id,
    int64_t new_resource_id) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader;
  worker_test_helper->context()
      ->registry()
      ->GetRemoteStorageControl()
      ->CreateResourceReader(old_resource_id,
                             compare_reader.BindNewPipeAndPassReceiver());

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader;
  worker_test_helper->context()
      ->registry()
      ->GetRemoteStorageControl()
      ->CreateResourceReader(old_resource_id,
                             copy_reader.BindNewPipeAndPassReceiver());

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  worker_test_helper->context()
      ->registry()
      ->GetRemoteStorageControl()
      ->CreateResourceWriter(new_resource_id,
                             writer.BindNewPipeAndPassReceiver());

  auto cache_writer = ServiceWorkerCacheWriter::CreateForComparison(
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      new_resource_id, /*pause_when_not_identical=*/true,
      ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch);
  cache_writer->response_head_to_write_ =
      network::mojom::URLResponseHead::New();
  cache_writer->response_head_to_write_->request_time = base::Time::Now();
  cache_writer->response_head_to_write_->response_time = base::Time::Now();
  cache_writer->response_head_to_write_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(new_headers);
  cache_writer->bytes_compared_ = bytes_compared;
  cache_writer->data_to_write_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      pending_network_buffer ? pending_network_buffer->buffer() : nullptr);
  cache_writer->len_to_write_ = consumed_size;
  cache_writer->bytes_written_ = 0;
  cache_writer->io_pending_ = true;
  cache_writer->state_ = ServiceWorkerCacheWriter::State::STATE_PAUSING;
  return cache_writer;
}

std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
ServiceWorkerUpdateCheckTestUtils::CreateUpdateCheckerPausedState(
    std::unique_ptr<ServiceWorkerCacheWriter> cache_writer,
    ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
    ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_size) {
  mojo::Remote<network::mojom::URLLoaderClient> network_loader_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      network_loader_client_receiver =
          network_loader_client.BindNewPipeAndPassReceiver();
  return std::make_unique<ServiceWorkerSingleScriptUpdateChecker::PausedState>(
      std::move(cache_writer), /*network_loader=*/nullptr,
      std::move(network_loader_client),
      std::move(network_loader_client_receiver),
      std::move(pending_network_buffer), consumed_size, network_loader_state,
      body_writer_state);
}

void ServiceWorkerUpdateCheckTestUtils::SetComparedScriptInfoForVersion(
    const GURL& script_url,
    int64_t resource_id,
    ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state,
    ServiceWorkerVersion* version) {
  std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo> info_map;
  info_map.emplace(script_url,
                   ServiceWorkerUpdateChecker::ComparedScriptInfo(
                       resource_id, compare_result, std::move(paused_state),
                       /*failure_info=*/nullptr));
  version->PrepareForUpdate(
      std::move(info_map),
      (compare_result ==
       ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent)
          ? script_url
          : GURL(),
      base::MakeRefCounted<PolicyContainerHost>());
}

void ServiceWorkerUpdateCheckTestUtils::
    CreateAndSetComparedScriptInfoForVersion(
        const GURL& script_url,
        size_t bytes_compared,
        const std::string& new_headers,
        const std::string& diff_data_block,
        int64_t old_resource_id,
        int64_t new_resource_id,
        EmbeddedWorkerTestHelper* worker_test_helper,
        ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
        ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
        ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
        ServiceWorkerVersion* version,
        mojo::ScopedDataPipeProducerHandle* out_body_handle) {
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  if (!diff_data_block.empty()) {
    mojo::ScopedDataPipeConsumerHandle network_consumer;
    // Create a data pipe which has the new block sent from the network.
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, *out_body_handle,
                                                   network_consumer));
    uint32_t written_size = diff_data_block.size();
    ASSERT_EQ(MOJO_RESULT_OK,
              (*out_body_handle)
                  ->WriteData(diff_data_block.c_str(), &written_size,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
    ASSERT_EQ(diff_data_block.size(), written_size);
    base::RunLoop().RunUntilIdle();

    // Read the data to make a pending buffer.
    ASSERT_EQ(MOJO_RESULT_OK,
              network::MojoToNetPendingBuffer::BeginRead(
                  &network_consumer, &pending_buffer, &bytes_available));
    ASSERT_EQ(diff_data_block.size(), bytes_available);
  }

  auto cache_writer = CreatePausedCacheWriter(
      worker_test_helper, bytes_compared, new_headers, pending_buffer,
      bytes_available, old_resource_id, new_resource_id);
  auto paused_state = CreateUpdateCheckerPausedState(
      std::move(cache_writer), network_loader_state, body_writer_state,
      pending_buffer, bytes_available);
  SetComparedScriptInfoForVersion(script_url, old_resource_id, compare_result,
                                  std::move(paused_state), version);
}

bool ServiceWorkerUpdateCheckTestUtils::VerifyStoredResponse(
    int64_t resource_id,
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const std::string& expected_body) {
  DCHECK(storage);
  if (resource_id == blink::mojom::kInvalidServiceWorkerResourceId)
    return false;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(resource_id,
                                reader.BindNewPipeAndPassReceiver());

  // Verify the response status.
  size_t response_data_size = 0;
  {
    int rv;
    std::string status_text;
    base::RunLoop loop;
    reader->ReadResponseHead(base::BindLambdaForTesting(
        [&](int status, network::mojom::URLResponseHeadPtr response_head,
            absl::optional<mojo_base::BigBuffer> metadata) {
          rv = status;
          status_text = response_head->headers->GetStatusText();
          response_data_size = response_head->content_length;
          loop.Quit();
        }));
    loop.Run();

    if (rv < 0)
      return false;
    EXPECT_LT(0, rv);
    EXPECT_EQ("OK", status_text);
  }

  // Verify the response body.
  {
    mojo::ScopedDataPipeConsumerHandle data_consumer;
    base::RunLoop loop;
    reader->PrepareReadData(response_data_size,
                            base::BindLambdaForTesting(
                                [&](mojo::ScopedDataPipeConsumerHandle pipe) {
                                  data_consumer = std::move(pipe);
                                  loop.Quit();
                                }));
    loop.Run();

    int32_t rv;
    base::RunLoop loop2;
    reader->ReadData(base::BindLambdaForTesting([&](int32_t status) {
      rv = status;
      loop2.Quit();
    }));

    std::string body = ReadDataPipe(std::move(data_consumer));
    loop2.Run();

    if (rv < 0)
      return false;
    EXPECT_EQ(static_cast<int>(expected_body.size()), rv);
    EXPECT_EQ(expected_body, body);
  }
  return true;
}

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    uint32_t num_bytes;
    const void* buffer = nullptr;
    MojoResult rv =
        handle.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer);
        EXPECT_GT(num_bytes, 0u);
        uint32_t before_size = result->size();
        result->append(static_cast<const char*>(buffer), num_bytes);
        uint32_t read_size = result->size() - before_size;
        EXPECT_EQ(num_bytes, read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED();
  return;
}

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace content
