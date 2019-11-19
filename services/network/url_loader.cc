// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/static_cookie_policy.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/canonical_cookie.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/cross_origin_resource_policy.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/empty_url_loader_client.h"
#include "services/network/loader_util.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/origin_policy/origin_policy_constants.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/sec_header_helpers.h"
#include "services/network/throttling/scoped_throttling_token.h"

namespace network {

namespace {

// Cannot use 0, because this means "default" in
// mojo::core::Core::CreateDataPipe
constexpr size_t kBlockedBodyAllocationSize = 1;

// TODO: this duplicates some of PopulateResourceResponse in
// content/browser/loader/resource_loader.cc
void PopulateResourceResponse(net::URLRequest* request,
                              bool is_load_timing_enabled,
                              bool include_ssl_info,
                              ResourceResponse* response) {
  response->head.request_time = request->request_time();
  response->head.response_time = request->response_time();
  response->head.headers = request->response_headers();
  request->GetCharset(&response->head.charset);
  response->head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->head.mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->head.was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->head.was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->head.alpn_negotiated_protocol =
      response_info.alpn_negotiated_protocol;
  response->head.connection_info = response_info.connection_info;
  response->head.remote_endpoint = response_info.remote_endpoint;
  response->head.was_fetched_via_cache = request->was_cached();
  response->head.proxy_server = request->proxy_server();
  response->head.network_accessed = response_info.network_accessed;
  response->head.async_revalidation_requested =
      response_info.async_revalidation_requested;
  response->head.was_in_prefetch_cache =
      !(request->load_flags() & net::LOAD_PREFETCH) &&
      response_info.unused_since_prefetch;

  if (is_load_timing_enabled)
    request->GetLoadTimingInfo(&response->head.load_timing);

  if (request->ssl_info().cert.get()) {
    response->head.ct_policy_compliance =
        request->ssl_info().ct_policy_compliance;
    response->head.cert_status = request->ssl_info().cert_status;
    net::SSLVersion ssl_version = net::SSLConnectionStatusToVersion(
        request->ssl_info().connection_status);
    response->head.is_legacy_tls_version =
        ssl_version == net::SSLVersion::SSL_CONNECTION_VERSION_TLS1 ||
        ssl_version == net::SSLVersion::SSL_CONNECTION_VERSION_TLS1_1;

    if (include_ssl_info)
      response->head.ssl_info = request->ssl_info();
  }

  response->head.request_start = request->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  response->head.encoded_data_length = request->GetTotalReceivedBytes();
  response->head.auth_challenge_info = request->auth_challenge_info();
}

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElement& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(network::mojom::DataElementType::kBytes, element.type());
  }

  ~BytesElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElement& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(network::mojom::DataElementType::kFile, element.type());
  }

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

class RawFileElementReader : public net::UploadFileElementReader {
 public:
  RawFileElementReader(ResourceRequestBody* resource_request_body,
                       base::TaskRunner* task_runner,
                       const DataElement& element)
      : net::UploadFileElementReader(
            task_runner,
            // TODO(mmenke): Is duplicating this necessary?
            element.file().Duplicate(),
            element.path(),
            element.offset(),
            element.length(),
            element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(network::mojom::DataElementType::kRawFile, element.type());
  }

  ~RawFileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(RawFileElementReader);
};

std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1 &&
      body->elements()->begin()->type() ==
          network::mojom::DataElementType::kChunkedDataPipe) {
    return std::make_unique<ChunkedDataPipeUploadDataStream>(
        body, const_cast<DataElement&>(body->elements()->front())
                  .ReleaseChunkedDataPipeGetter());
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementType::kBytes:
        element_readers.push_back(
            std::make_unique<BytesElementReader>(body, element));
        break;
      case network::mojom::DataElementType::kFile:
        DCHECK(opened_file != opened_files.end());
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element, std::move(*opened_file++)));
        break;
      case network::mojom::DataElementType::kRawFile:
        element_readers.push_back(std::make_unique<RawFileElementReader>(
            body, file_task_runner, element));
        break;
      case network::mojom::DataElementType::kBlob: {
        CHECK(false) << "Network service always uses DATA_PIPE for blobs.";
        break;
      }
      case network::mojom::DataElementType::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body, element.CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementType::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED();
        break;
      }
      case network::mojom::DataElementType::kUnknown:
        NOTREACHED();
        break;
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

class SSLPrivateKeyInternal : public net::SSLPrivateKey {
 public:
  SSLPrivateKeyInternal(
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key)
      : provider_name_(provider_name),
        algorithm_preferences_(algorithm_preferences),
        ssl_private_key_(std::move(ssl_private_key)) {
    ssl_private_key_.set_disconnect_handler(
        base::BindOnce(&SSLPrivateKeyInternal::HandleSSLPrivateKeyError,
                       base::Unretained(this)));
  }

  // net::SSLPrivateKey:
  std::string GetProviderName() override { return provider_name_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return algorithm_preferences_;
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            net::SSLPrivateKey::SignCallback callback) override {
    std::vector<uint8_t> input_vector(input.begin(), input.end());
    if (!ssl_private_key_ || !ssl_private_key_.is_connected()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY,
                         input_vector));
      return;
    }

    ssl_private_key_->Sign(algorithm, input_vector,
                           base::BindOnce(&SSLPrivateKeyInternal::Callback,
                                          this, std::move(callback)));
  }

 private:
  ~SSLPrivateKeyInternal() override = default;

  void HandleSSLPrivateKeyError() { ssl_private_key_.reset(); }

  void Callback(net::SSLPrivateKey::SignCallback callback,
                int32_t net_error,
                const std::vector<uint8_t>& input) {
    DCHECK_LE(net_error, 0);
    DCHECK_NE(net_error, net::ERR_IO_PENDING);
    std::move(callback).Run(static_cast<net::Error>(net_error), input);
  }

  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<mojom::SSLPrivateKey> ssl_private_key_;

  DISALLOW_COPY_AND_ASSIGN(SSLPrivateKeyInternal);
};

bool ShouldNotifyAboutCookie(
    net::CanonicalCookie::CookieInclusionStatus status) {
  // Notify about cookies actually used, and those blocked by preferences ---
  // for purposes of cookie UI --- as well those carrying warnings pertaining to
  // SameSite features, in order to issue a deprecation warning for them.
  return status.IsInclude() || status.ShouldWarn() ||
         status.HasExclusionReason(net::CanonicalCookie::CookieInclusionStatus::
                                       EXCLUDE_USER_PREFERENCES);
}

}  // namespace

URLLoader::URLLoader(
    net::URLRequestContext* url_request_context,
    mojom::NetworkServiceClient* network_service_client,
    mojom::NetworkContextClient* network_context_client,
    DeleteCallback delete_callback,
    mojo::PendingReceiver<mojom::URLLoader> url_loader_receiver,
    int32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const mojom::URLLoaderFactoryParams* factory_params,
    uint32_t request_id,
    int keepalive_request_size,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder,
    base::WeakPtr<NetworkUsageAccumulator> network_usage_accumulator,
    mojom::TrustedURLLoaderHeaderClient* url_loader_header_client,
    mojom::OriginPolicyManager* origin_policy_manager)
    : url_request_context_(url_request_context),
      network_service_client_(network_service_client),
      network_context_client_(network_context_client),
      delete_callback_(std::move(delete_callback)),
      options_(options),
      corb_detachable_(request.corb_detachable),
      resource_type_(request.resource_type),
      is_load_timing_enabled_(request.enable_load_timing),
      factory_params_(factory_params),
      render_frame_id_(request.render_frame_id),
      request_id_(request_id),
      keepalive_request_size_(keepalive_request_size),
      keepalive_(request.keepalive),
      do_not_prompt_for_login_(request.do_not_prompt_for_login),
      receiver_(this, std::move(url_loader_receiver)),
      url_loader_client_(std::move(url_loader_client)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      peer_closed_handle_watcher_(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                                  base::SequencedTaskRunnerHandle::Get()),
      want_raw_headers_(request.report_raw_headers),
      report_raw_headers_(false),
      devtools_request_id_(request.devtools_request_id),
      resource_scheduler_client_(std::move(resource_scheduler_client)),
      keepalive_statistics_recorder_(std::move(keepalive_statistics_recorder)),
      network_usage_accumulator_(std::move(network_usage_accumulator)),
      first_auth_attempt_(true),
      custom_proxy_pre_cache_headers_(request.custom_proxy_pre_cache_headers),
      custom_proxy_post_cache_headers_(request.custom_proxy_post_cache_headers),
      custom_proxy_use_alternate_proxy_list_(
          request.custom_proxy_use_alternate_proxy_list),
      fetch_window_id_(request.fetch_window_id),
      update_network_isolation_key_on_redirect_(
          request.trusted_params
              ? request.trusted_params->update_network_isolation_key_on_redirect
              : mojom::UpdateNetworkIsolationKeyOnRedirect::kDoNotUpdate),
      origin_policy_manager_(nullptr) {
  DCHECK(delete_callback_);
  DCHECK(factory_params_);
  if (url_loader_header_client &&
      (options_ & mojom::kURLLoadOptionUseHeaderClient)) {
    if (options_ & mojom::kURLLoadOptionAsCorsPreflight) {
      url_loader_header_client->OnLoaderForCorsPreflightCreated(
          request, header_client_.BindNewPipeAndPassReceiver());
    } else {
      url_loader_header_client->OnLoaderCreated(
          request_id_, header_client_.BindNewPipeAndPassReceiver());
    }
    // Make sure the loader dies if |header_client_| has an error, otherwise
    // requests can hang.
    header_client_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  }
  if (want_raw_headers_) {
    options_ |= mojom::kURLLoadOptionSendSSLInfoWithResponse |
                mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  }
  receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  url_request_ = url_request_context_->CreateRequest(
      GURL(request.url), request.priority, this, traffic_annotation);
  url_request_->set_method(request.method);
  url_request_->set_site_for_cookies(request.site_for_cookies);
  url_request_->set_attach_same_site_cookies(request.attach_same_site_cookies);
  url_request_->SetReferrer(request.referrer.GetAsReferrer().spec());
  url_request_->set_referrer_policy(request.referrer_policy);
  url_request_->set_upgrade_if_insecure(request.upgrade_if_insecure);

  if (factory_params_->network_isolation_key) {
    url_request_->set_network_isolation_key(
        factory_params_->network_isolation_key.value());
  } else if (request.trusted_params &&
             !request.trusted_params->network_isolation_key.IsEmpty()) {
    url_request_->set_network_isolation_key(
        request.trusted_params->network_isolation_key);
  }

  if (request.trusted_params) {
    url_request_->SetDisableSecureDns(
        request.trusted_params->disable_secure_dns);
  }

  // |cors_excempt_headers| must be merged here to avoid breaking CORS checks.
  // They are non-empty when the values are given by the UA code, therefore
  // they should be ignored by CORS checks.
  net::HttpRequestHeaders merged_headers = request.headers;
  merged_headers.MergeFrom(request.cors_exempt_headers);
  if (request.obey_origin_policy) {
    DCHECK(origin_policy_manager);
    origin_policy_manager_ = origin_policy_manager;
    merged_headers.SetHeader(net::HttpRequestHeaders::kSecOriginPolicy,
                             kDefaultOriginPolicyVersion);
  }
  // This should be ensured by the CorsURLLoaderFactory(), which is called
  // before URLLoaders are created.
  DCHECK(AreRequestHeadersSafe(merged_headers));
  url_request_->SetExtraRequestHeaders(merged_headers);

  url_request_->SetUserData(kUserDataKey,
                            std::make_unique<UnownedPointer>(this));

  is_nocors_corb_excluded_request_ =
      request.corb_excluded && request.mode == mojom::RequestMode::kNoCors &&
      CrossOriginReadBlocking::ShouldAllowForPlugin(
          factory_params_->process_id);
  request_mode_ = request.mode;

  throttling_token_ = network::ScopedThrottlingToken::MaybeCreate(
      url_request_->net_log().source().id, request.throttling_profile_id);

  url_request_->set_initiator(request.request_initiator);

  SetFetchMetadataHeaders(url_request_.get(), request_mode_, nullptr,
                          *factory_params_);

  if (request.update_first_party_url_on_redirect) {
    url_request_->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  url_request_->SetLoadFlags(request.load_flags);

  // net::LOAD_DO_NOT_* are in the process of being converted to
  // credentials_mode. See https://crbug.com/799935.
  // TODO(crbug.com/943939): Make this work with CredentialsMode::kSameOrigin.
  if (request.credentials_mode == mojom::CredentialsMode::kOmit) {
    const auto creds_mask = net::LOAD_DO_NOT_SAVE_COOKIES |
                            net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SEND_AUTH_DATA;
    DCHECK((request.load_flags & creds_mask) == 0 ||
           (request.load_flags & creds_mask) == creds_mask);
    url_request_->set_allow_credentials(false);
  }

  url_request_->SetRequestHeadersCallback(base::BindRepeating(
      &URLLoader::SetRawRequestHeadersAndNotify, base::Unretained(this)));

  if (want_raw_headers_) {
    url_request_->SetResponseHeadersCallback(
        base::Bind(&URLLoader::SetRawResponseHeaders, base::Unretained(this)));
  }

  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadStarted(factory_params_->process_id,
                                                  keepalive_request_size_);
  }

  // Resolve elements from request_body and prepare upload data.
  if (request.request_body.get()) {
    OpenFilesForUpload(request);
    return;
  }
  ScheduleStart();
}

// This class is used to manage the queue of pending file upload operations
// initiated by the URLLoader::OpenFilesForUpload().
class URLLoader::FileOpenerForUpload {
 public:
  typedef base::OnceCallback<void(int, std::vector<base::File>)>
      SetUpUploadCallback;

  FileOpenerForUpload(std::vector<base::FilePath> paths,
                      URLLoader* url_loader,
                      uint32_t process_id,
                      mojom::NetworkContextClient* const network_context_client,
                      SetUpUploadCallback set_up_upload_callback)
      : paths_(std::move(paths)),
        url_loader_(url_loader),
        process_id_(process_id),
        network_context_client_(network_context_client),
        set_up_upload_callback_(std::move(set_up_upload_callback)) {
    StartOpeningNextBatch();
  }

  ~FileOpenerForUpload() {
    if (!opened_files_.empty())
      PostCloseFiles(std::move(opened_files_));
  }

 private:
  static void OnFilesForUploadOpened(
      base::WeakPtr<FileOpenerForUpload> file_opener,
      size_t num_files_requested,
      int error_code,
      std::vector<base::File> opened_files) {
    if (!file_opener) {
      PostCloseFiles(std::move(opened_files));
      return;
    }

    if (error_code == net::OK && num_files_requested != opened_files.size())
      error_code = net::ERR_FAILED;

    if (error_code != net::OK) {
      PostCloseFiles(std::move(opened_files));
      file_opener->FilesForUploadOpenedDone(error_code);
      return;
    }

    for (base::File& file : opened_files)
      file_opener->opened_files_.push_back(std::move(file));

    if (file_opener->opened_files_.size() < file_opener->paths_.size()) {
      file_opener->StartOpeningNextBatch();
      return;
    }

    file_opener->FilesForUploadOpenedDone(net::OK);
  }

  // |opened_files| need to be closed on a blocking task runner, so move the
  // |opened_files| vector onto a sequence that can block so it gets destroyed
  // there.
  static void PostCloseFiles(std::vector<base::File> opened_files) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_BLOCKING},
        base::BindOnce(base::DoNothing::Once<std::vector<base::File>>(),
                       std::move(opened_files)));
  }

  void StartOpeningNextBatch() {
    size_t num_files_to_request = std::min(paths_.size() - opened_files_.size(),
                                           kMaxFileUploadRequestsPerBatch);
    std::vector<base::FilePath> batch_paths(
        paths_.begin() + opened_files_.size(),
        paths_.begin() + opened_files_.size() + num_files_to_request);

    network_context_client_->OnFileUploadRequested(
        process_id_, /*async=*/true, batch_paths,
        base::BindOnce(&FileOpenerForUpload::OnFilesForUploadOpened,
                       weak_ptr_factory_.GetWeakPtr(), num_files_to_request));
  }

  void FilesForUploadOpenedDone(int error_code) {
    url_loader_->url_request_->LogUnblocked();

    if (error_code == net::OK)
      std::move(set_up_upload_callback_).Run(net::OK, std::move(opened_files_));
    else
      std::move(set_up_upload_callback_).Run(error_code, {});
  }

  // The paths of files for upload
  const std::vector<base::FilePath> paths_;
  URLLoader* const url_loader_;
  const uint32_t process_id_;
  mojom::NetworkContextClient* const network_context_client_;
  SetUpUploadCallback set_up_upload_callback_;
  // The files opened so far.
  std::vector<base::File> opened_files_;

  base::WeakPtrFactory<FileOpenerForUpload> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileOpenerForUpload);
};

void URLLoader::OpenFilesForUpload(const ResourceRequest& request) {
  std::vector<base::FilePath> paths;
  for (const auto& element : *request.request_body.get()->elements()) {
    if (element.type() == mojom::DataElementType::kFile)
      paths.push_back(element.path());
  }
  if (paths.empty()) {
    SetUpUpload(request, net::OK, std::vector<base::File>());
    return;
  }
  if (!network_context_client_) {
    DLOG(ERROR) << "URLLoader couldn't upload a file because no "
                   "NetworkContextClient is set.";
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::NotifyCompleted, base::Unretained(this),
                       net::ERR_ACCESS_DENIED));
    return;
  }
  url_request_->LogBlockedBy("Opening Files");
  file_opener_for_upload_ = std::make_unique<FileOpenerForUpload>(
      std::move(paths), this, factory_params_->process_id,
      network_context_client_,
      base::BindOnce(&URLLoader::SetUpUpload, base::Unretained(this), request));
}

void URLLoader::SetUpUpload(const ResourceRequest& request,
                            int error_code,
                            std::vector<base::File> opened_files) {
  if (error_code != net::OK) {
    DCHECK(opened_files.empty());
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  base::Unretained(this), error_code));
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::USER_VISIBLE});
  url_request_->set_upload(CreateUploadDataStream(
      request.request_body.get(), opened_files, task_runner.get()));

  if (request.enable_upload_progress) {
    upload_progress_tracker_ = std::make_unique<UploadProgressTracker>(
        FROM_HERE,
        base::BindRepeating(&URLLoader::SendUploadProgress,
                            base::Unretained(this)),
        url_request_.get());
  }
  ScheduleStart();
}

void URLLoader::ScheduleStart() {
  bool defer = false;
  if (resource_scheduler_client_) {
    resource_scheduler_request_handle_ =
        resource_scheduler_client_->ScheduleRequest(
            !(options_ & mojom::kURLLoadOptionSynchronous), url_request_.get());
    resource_scheduler_request_handle_->set_resume_callback(
        base::BindRepeating(&URLLoader::ResumeStart, base::Unretained(this)));
    resource_scheduler_request_handle_->WillStartRequest(&defer);
  }
  if (defer)
    url_request_->LogBlockedBy("ResourceScheduler");
  else
    url_request_->Start();
}

URLLoader::~URLLoader() {
  RecordBodyReadFromNetBeforePausedIfNeeded();
  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadFinished(factory_params_->process_id,
                                                   keepalive_request_size_);
  }
}

// static
const void* const URLLoader::kUserDataKey = &URLLoader::kUserDataKey;

void URLLoader::FollowRedirect(const std::vector<std::string>& removed_headers,
                               const net::HttpRequestHeaders& modified_headers,
                               const base::Optional<GURL>& new_url) {
  if (!deferred_redirect_url_) {
    NOTREACHED();
    return;
  }

  // Removing headers can't make the set of pre-existing headers unsafe, but
  // adding headers can.
  if (!AreRequestHeadersSafe(modified_headers)) {
    NotifyCompleted(net::ERR_INVALID_ARGUMENT);
    // |this| may have been deleted.
    return;
  }

  if (!modified_headers.IsEmpty())
    LogConcerningRequestHeaders(modified_headers,
                                true /* added_during_redirect */);

  // See if network isolation key needs to be updated.
  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information
  if (url_request_->network_isolation_key().IsFullyPopulated() &&
      update_network_isolation_key_on_redirect_ !=
          mojom::UpdateNetworkIsolationKeyOnRedirect::kDoNotUpdate) {
    const GURL& url = new_url ? new_url.value() : *deferred_redirect_url_;
    const url::Origin& new_origin = url::Origin::Create(url);

    if (update_network_isolation_key_on_redirect_ ==
        mojom::UpdateNetworkIsolationKeyOnRedirect::
            kUpdateTopFrameAndFrameOrigin) {
      url_request_->set_network_isolation_key(net::NetworkIsolationKey(
          new_origin /* top frame origin */, new_origin /* frame origin */));
    } else if (update_network_isolation_key_on_redirect_ ==
               mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateFrameOrigin) {
      base::Optional<url::Origin> top_frame_origin =
          url_request_->network_isolation_key().GetTopFrameOrigin();
      DCHECK(top_frame_origin);
      url_request_->set_network_isolation_key(
          net::NetworkIsolationKey(top_frame_origin.value(), new_origin));
    }
  }

  deferred_redirect_url_.reset();
  new_redirect_url_ = new_url;

  url_request_->FollowDeferredRedirect(removed_headers, modified_headers);
  new_redirect_url_.reset();
}

void URLLoader::SetPriority(net::RequestPriority priority,
                            int32_t intra_priority_value) {
  if (url_request_ && resource_scheduler_client_) {
    resource_scheduler_client_->ReprioritizeRequest(
        url_request_.get(), priority, intra_priority_value);
  }
}

void URLLoader::PauseReadingBodyFromNet() {
  DVLOG(1) << "URLLoader pauses fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");

  // Please note that we pause reading body in all cases. Even if the URL
  // request indicates that the response was cached, there could still be
  // network activity involved. For example, the response was only partially
  // cached.
  //
  // On the other hand, we only report BodyReadFromNetBeforePaused histogram
  // when we are sure that the response body hasn't been read from cache. This
  // avoids polluting the histogram data with data points from cached responses.
  should_pause_reading_body_ = true;

  // If the data pipe has been set up and the request is in IO pending state,
  // there is a pending read for the response body.
  if (HasDataPipe() && url_request_->status().is_io_pending()) {
    update_body_read_before_paused_ = true;
  } else {
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  }
}

void URLLoader::ResumeReadingBodyFromNet() {
  DVLOG(1) << "URLLoader resumes fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  should_pause_reading_body_ = false;

  if (paused_reading_body_) {
    paused_reading_body_ = false;
    ReadMore();
  }
}

void URLLoader::OnReceivedRedirect(net::URLRequest* url_request,
                                   const net::RedirectInfo& redirect_info,
                                   bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  DCHECK(!deferred_redirect_url_);
  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  PopulateResourceResponse(
      url_request_.get(), is_load_timing_enabled_,
      options_ & mojom::kURLLoadOptionSendSSLInfoWithResponse, response.get());
  if (report_raw_headers_) {
    response->head.raw_request_response_info = BuildRawRequestResponseInfo(
        *url_request_, raw_request_headers_, raw_response_headers_.get());
    raw_request_headers_ = net::HttpRawRequestHeaders();
    raw_response_headers_ = nullptr;
  }

  ReportFlaggedResponseCookies();

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  if (CrossOriginResourcePolicy::kBlock ==
      CrossOriginResourcePolicy::Verify(
          url_request_->url(), url_request_->initiator(), response->head,
          request_mode_, factory_params_->request_initiator_site_lock,
          factory_params_->cross_origin_embedder_policy)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false);
    DeleteSelf();
    return;
  }

  // We may need to clear out old Sec- prefixed request headers. We'll attempt
  // to do this before we re-add any.
  MaybeRemoveSecHeaders(url_request_.get(), redirect_info.new_url);
  SetFetchMetadataHeaders(url_request_.get(), request_mode_,
                          &redirect_info.new_url, *factory_params_);

  url_loader_client_->OnReceiveRedirect(redirect_info, response->head);
}

void URLLoader::OnAuthRequired(net::URLRequest* url_request,
                               const net::AuthChallengeInfo& auth_info) {
  if (!network_context_client_) {
    OnAuthCredentials(base::nullopt);
    return;
  }

  if (do_not_prompt_for_login_) {
    OnAuthCredentials(base::nullopt);
    return;
  }

  DCHECK(!auth_challenge_responder_receiver_.is_bound());

  ResourceResponseHead head;
  if (url_request->response_headers())
    head.headers = url_request->response_headers();
  head.auth_challenge_info = auth_info;
  network_context_client_->OnAuthRequired(
      fetch_window_id_, factory_params_->process_id, render_frame_id_,
      request_id_, url_request_->url(), first_auth_attempt_, auth_info, head,
      auth_challenge_responder_receiver_.BindNewPipeAndPassRemote());

  auth_challenge_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::DeleteSelf, base::Unretained(this)));

  first_auth_attempt_ = false;
}

void URLLoader::OnCertificateRequested(net::URLRequest* unused,
                                       net::SSLCertRequestInfo* cert_info) {
  DCHECK(!client_cert_responder_receiver_.is_bound());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreUrlFetcherCertRequests) &&
      factory_params_->process_id == 0 &&
      render_frame_id_ == MSG_ROUTING_NONE) {
    ContinueWithoutCertificate();
    return;
  }

  if (!network_context_client_) {
    ContinueWithoutCertificate();
    return;
  }

  // Set up mojo endpoints for ClientCertificateResponder and bind to the
  // Receiver. This enables us to receive messages regarding the client
  // certificate selection.
  if (fetch_window_id_) {
    network_context_client_->OnCertificateRequested(
        fetch_window_id_, -1 /* process_id */, -1 /* routing_id */, request_id_,
        cert_info, client_cert_responder_receiver_.BindNewPipeAndPassRemote());
  } else {
    network_context_client_->OnCertificateRequested(
        base::nullopt /* window_id */, factory_params_->process_id,
        render_frame_id_, request_id_, cert_info,
        client_cert_responder_receiver_.BindNewPipeAndPassRemote());
  }
  client_cert_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::CancelRequest, base::Unretained(this)));
}

void URLLoader::OnSSLCertificateError(net::URLRequest* request,
                                      int net_error,
                                      const net::SSLInfo& ssl_info,
                                      bool fatal) {
  if (!network_context_client_) {
    OnSSLCertificateErrorResponse(ssl_info, net::ERR_INSECURE_RESPONSE);
    return;
  }
  network_context_client_->OnSSLCertificateError(
      factory_params_->process_id, render_frame_id_, url_request_->url(),
      net_error, ssl_info, fatal,
      base::Bind(&URLLoader::OnSSLCertificateErrorResponse,
                 weak_ptr_factory_.GetWeakPtr(), ssl_info));
}

void URLLoader::OnResponseStarted(net::URLRequest* url_request, int net_error) {
  DCHECK(url_request == url_request_.get());

  ReportFlaggedResponseCookies();

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeDefaultAllocationSize;
  MojoResult result =
      mojo::CreateDataPipe(&options, &response_body_stream_, &consumer_handle_);
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  DCHECK(response_body_stream_.is_valid());
  DCHECK(consumer_handle_.is_valid());

  // Do not account header bytes when reporting received body bytes to client.
  reported_total_encoded_bytes_ = url_request_->GetTotalReceivedBytes();

  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  response_ = new ResourceResponse();
  PopulateResourceResponse(
      url_request_.get(), is_load_timing_enabled_,
      options_ & mojom::kURLLoadOptionSendSSLInfoWithResponse, response_.get());
  if (report_raw_headers_) {
    response_->head.raw_request_response_info = BuildRawRequestResponseInfo(
        *url_request_, raw_request_headers_, raw_response_headers_.get());
    raw_request_headers_ = net::HttpRawRequestHeaders();
    raw_response_headers_ = nullptr;
  }

  peer_closed_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::Bind(&URLLoader::OnResponseBodyStreamConsumerClosed,
                 base::Unretained(this)));
  peer_closed_handle_watcher_.ArmOrNotify();

  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::Bind(&URLLoader::OnResponseBodyStreamReady,
                 base::Unretained(this)));

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  if (CrossOriginResourcePolicy::kBlock ==
      CrossOriginResourcePolicy::Verify(
          url_request_->url(), url_request_->initiator(), response_->head,
          request_mode_, factory_params_->request_initiator_site_lock,
          factory_params_->cross_origin_embedder_policy)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false);
    DeleteSelf();
    return;
  }

  // Figure out if we need to sniff (for MIME type detection or for Cross-Origin
  // Read Blocking / CORB).
  if (factory_params_->is_corb_enabled && !is_nocors_corb_excluded_request_) {
    CrossOriginReadBlocking::LogAction(
        CrossOriginReadBlocking::Action::kResponseStarted);

    corb_analyzer_ =
        std::make_unique<CrossOriginReadBlocking::ResponseAnalyzer>(
            url_request_->url(), url_request_->initiator(), response_->head,
            factory_params_->request_initiator_site_lock, request_mode_);
    is_more_corb_sniffing_needed_ = corb_analyzer_->needs_sniffing();
    if (corb_analyzer_->ShouldBlock()) {
      DCHECK(!is_more_corb_sniffing_needed_);
      corb_analyzer_->LogBlockedResponse();
      if (BlockResponseForCorb() == kWillCancelRequest)
        return;
    } else if (corb_analyzer_->ShouldAllow()) {
      DCHECK(!is_more_corb_sniffing_needed_);
      corb_analyzer_->LogAllowedResponse();
    }
  }
  if ((options_ & mojom::kURLLoadOptionSniffMimeType)) {
    if (ShouldSniffContent(url_request_.get(), response_.get())) {
      is_more_mime_sniffing_needed_ = true;
    } else if (response_->head.mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us
      // a mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->head.mime_type.assign("text/plain");
    }
  }

  if (base::FeatureList::IsEnabled(
          network::features::kOutOfBlinkFrameAncestors)) {
    // Parse the Content-Security-Policy headers.
    ContentSecurityPolicy policy;
    if (policy.Parse(url_request_->url(), *url_request_->response_headers()))
      response_->head.content_security_policy = std::move(policy);
  }

  // If necessary, retrieve the associated origin policy, before sending the
  // response to the client.
  if (origin_policy_manager_ && url_request_->response_headers()) {
    std::string sec_origin_policy_header;
    url_request_->response_headers()->GetNormalizedHeader(
        net::HttpRequestHeaders::kSecOriginPolicy, &sec_origin_policy_header);

    OriginPolicyManager::RetrieveOriginPolicyCallback
        origin_policy_manager_done =
            base::BindOnce(&URLLoader::OnOriginPolicyManagerRetrieveDone,
                           weak_ptr_factory_.GetWeakPtr());
    // Passing an empty string if no header is present is intentional as the
    // origin policy manager needs to also handle an empty header.
    origin_policy_manager_->RetrieveOriginPolicy(
        url::Origin::Create(url_request_->url()), sec_origin_policy_header,
        std::move(origin_policy_manager_done));

    // The callback will continue by calling
    // `StartReading()` after retrieving the origin
    // policy.
    return;
  }

  StartReading();
}

void URLLoader::ReadMore() {
  // Once the MIME type is sniffed, all data is sent as soon as it is read from
  // the network.
  DCHECK(consumer_handle_.is_valid() || !pending_write_);

  if (should_pause_reading_body_) {
    paused_reading_body_ = true;
    return;
  }

  if (!pending_write_.get()) {
    // TODO: we should use the abstractions in MojoAsyncResourceHandler.
    DCHECK_EQ(0u, pending_write_buffer_offset_);
    MojoResult result = NetToMojoPendingBuffer::BeginWrite(
        &response_body_stream_, &pending_write_, &pending_write_buffer_size_);
    if (result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT) {
      // The response body stream is in a bad state. Bail.
      NotifyCompleted(net::ERR_FAILED);
      return;
    }

    DCHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()),
              pending_write_buffer_size_);
    if (consumer_handle_.is_valid()) {
      DCHECK_GE(pending_write_buffer_size_,
                static_cast<uint32_t>(net::kMaxBytesToSniff));
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // The pipe is full. We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return;
    }
  }

  auto buf = base::MakeRefCounted<NetToMojoIOBuffer>(
      pending_write_.get(), pending_write_buffer_offset_);
  int bytes_read = url_request_->Read(
      buf.get(), static_cast<int>(pending_write_buffer_size_ -
                                  pending_write_buffer_offset_));
  if (bytes_read != net::ERR_IO_PENDING) {
    DidRead(bytes_read, true);
    // |this| may have been deleted.
  }
}

void URLLoader::DidRead(int num_bytes, bool completed_synchronously) {
  size_t new_data_offset = pending_write_buffer_offset_;
  if (num_bytes > 0) {
    pending_write_buffer_offset_ += num_bytes;

    // Only notify client of download progress if we're done sniffing and
    // started sending response.
    if (!consumer_handle_.is_valid()) {
      int64_t total_encoded_bytes = url_request_->GetTotalReceivedBytes();
      int64_t delta = total_encoded_bytes - reported_total_encoded_bytes_;
      DCHECK_LE(0, delta);
      if (delta)
        url_loader_client_->OnTransferSizeUpdated(delta);
      reported_total_encoded_bytes_ = total_encoded_bytes;
    }
  }
  if (update_body_read_before_paused_) {
    update_body_read_before_paused_ = false;
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  }

  bool complete_read = true;
  if (consumer_handle_.is_valid()) {
    // |pending_write_| may be null if the job self-aborts due to a suspend;
    // this will have |consumer_handle_| valid when the loader is paused.
    if (pending_write_) {
      // Limit sniffing to the first net::kMaxBytesToSniff.
      size_t data_length = pending_write_buffer_offset_;
      if (data_length > net::kMaxBytesToSniff)
        data_length = net::kMaxBytesToSniff;

      base::StringPiece data(pending_write_->buffer(), data_length);

      if (is_more_mime_sniffing_needed_) {
        const std::string& type_hint = response_->head.mime_type;
        std::string new_type;
        is_more_mime_sniffing_needed_ = !net::SniffMimeType(
            data.data(), data.size(), url_request_->url(), type_hint,
            net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
        // SniffMimeType() returns false if there is not enough data to
        // determine the mime type. However, even if it returns false, it
        // returns a new type that is probably better than the current one.
        response_->head.mime_type.assign(new_type);
        response_->head.did_mime_sniff = true;
      }

      if (is_more_corb_sniffing_needed_) {
        corb_analyzer_->SniffResponseBody(data, new_data_offset);
        if (corb_analyzer_->ShouldBlock()) {
          corb_analyzer_->LogBlockedResponse();
          is_more_corb_sniffing_needed_ = false;
          if (BlockResponseForCorb() == kWillCancelRequest)
            return;
        } else if (corb_analyzer_->ShouldAllow()) {
          corb_analyzer_->LogAllowedResponse();
          is_more_corb_sniffing_needed_ = false;
        }
      }
    }

    if (num_bytes <= 0 ||
        pending_write_buffer_offset_ >= net::kMaxBytesToSniff) {
      is_more_mime_sniffing_needed_ = false;

      if (is_more_corb_sniffing_needed_) {
        corb_analyzer_->LogAllowedResponse();
        is_more_corb_sniffing_needed_ = false;
      }
    }

    if (!is_more_mime_sniffing_needed_ && !is_more_corb_sniffing_needed_) {
      SendResponseToClient();
    } else {
      complete_read = false;
    }
  }

  if (!url_request_->status().is_success() || num_bytes == 0) {
    // There may be no |pending_write_| if a URLRequestJob cancelled itself in
    // URLRequestJob::OnSuspend() after receiving headers, while there was no
    // pending read.
    // TODO(mmenke): That case is rather unfortunate - something should be done
    // at the socket layer instead, both to make for a better API (Only complete
    // reads when there's a pending read), and to cover all TCP socket uses,
    // since the concern is the effect that entering suspend mode has on
    // sockets. See https://crbug.com/651120.
    if (pending_write_)
      CompletePendingWrite(url_request_->status().is_success());
    NotifyCompleted(url_request_->status().ToNetError());
    // |this| will have been deleted.
    return;
  }

  if (complete_read) {
    CompletePendingWrite(true /* success */);
  }
  if (completed_synchronously) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoader::OnReadCompleted(net::URLRequest* url_request, int bytes_read) {
  DCHECK(url_request == url_request_.get());

  DidRead(bytes_read, false);
  // |this| may have been deleted.
}

int URLLoader::OnBeforeStartTransaction(net::CompletionOnceCallback callback,
                                        net::HttpRequestHeaders* headers) {
  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        *headers, base::BindOnce(&URLLoader::OnBeforeSendHeadersComplete,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback), headers));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int URLLoader::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& endpoint,
    base::Optional<GURL>* preserve_fragment_on_redirect_url) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(), endpoint,
        base::BindOnce(&URLLoader::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers,
                       preserve_fragment_on_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

net::LoadState URLLoader::GetLoadStateForTesting() const {
  return url_request_->GetLoadState().state;
}

uint32_t URLLoader::GetRenderFrameId() const {
  return render_frame_id_;
}

uint32_t URLLoader::GetProcessId() const {
  return factory_params_->process_id;
}

void URLLoader::SetAllowReportingRawHeaders(bool allow) {
  report_raw_headers_ = want_raw_headers_ && allow;
}

uint32_t URLLoader::GetResourceType() const {
  return resource_type_;
}

bool URLLoader::AllowCookies(const GURL& url,
                             const GURL& site_for_cookies) const {
  net::StaticCookiePolicy::Type policy =
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  if (options_ & mojom::kURLLoadOptionBlockAllCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (options_ & mojom::kURLLoadOptionBlockThirdPartyCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  } else {
    return true;
  }
  return net::StaticCookiePolicy(policy).CanAccessCookies(
             url, site_for_cookies) == net::OK;
}

// static
URLLoader* URLLoader::ForRequest(const net::URLRequest& request) {
  auto* pointer =
      static_cast<UnownedPointer*>(request.GetUserData(kUserDataKey));
  if (!pointer)
    return nullptr;
  return pointer->get();
}

void URLLoader::OnAuthCredentials(
    const base::Optional<net::AuthCredentials>& credentials) {
  auth_challenge_responder_receiver_.reset();

  if (!credentials.has_value()) {
    url_request_->CancelAuth();
  } else {
    // CancelAuth will proceed to the body, so cookies only need to be reported
    // here.
    ReportFlaggedResponseCookies();
    url_request_->SetAuth(credentials.value());
  }
}

void URLLoader::ContinueWithCertificate(
    const scoped_refptr<net::X509Certificate>& x509_certificate,
    const std::string& provider_name,
    const std::vector<uint16_t>& algorithm_preferences,
    mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) {
  client_cert_responder_receiver_.reset();
  auto key = base::MakeRefCounted<SSLPrivateKeyInternal>(
      provider_name, algorithm_preferences, std::move(ssl_private_key));
  url_request_->ContinueWithCertificate(std::move(x509_certificate),
                                        std::move(key));
}

void URLLoader::ContinueWithoutCertificate() {
  client_cert_responder_receiver_.reset();
  url_request_->ContinueWithCertificate(nullptr, nullptr);
}

void URLLoader::CancelRequest() {
  client_cert_responder_receiver_.reset();
  url_request_->CancelWithError(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void URLLoader::NotifyCompleted(int error_code) {
  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  if (network_usage_accumulator_) {
    network_usage_accumulator_->OnBytesTransferred(
        factory_params_->process_id, render_frame_id_,
        url_request_->GetTotalReceivedBytes(),
        url_request_->GetTotalSentBytes());
  }
  if (network_service_client_ && (url_request_->GetTotalReceivedBytes() > 0 ||
                                  url_request_->GetTotalSentBytes() > 0)) {
    network_service_client_->OnDataUseUpdate(
        url_request_->traffic_annotation().unique_id_hash_code,
        url_request_->GetTotalReceivedBytes(),
        url_request_->GetTotalSentBytes());
  }

  if (url_loader_client_) {
    if (consumer_handle_.is_valid())
      SendResponseToClient();

    URLLoaderCompletionStatus status;
    status.error_code = error_code;
    if (error_code == net::ERR_QUIC_PROTOCOL_ERROR) {
      net::NetErrorDetails details;
      url_request_->PopulateNetErrorDetails(&details);
      status.extended_error_code = details.quic_connection_error;
    }
    status.exists_in_cache = url_request_->response_info().was_cached;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = url_request_->GetTotalReceivedBytes();
    status.encoded_body_length = url_request_->GetRawBodyBytes();
    status.decoded_body_length = total_written_bytes_;
    status.proxy_server = url_request_->proxy_server();

    if ((options_ & mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
        net::IsCertStatusError(url_request_->ssl_info().cert_status)) {
      status.ssl_info = url_request_->ssl_info();
    }

    url_loader_client_->OnComplete(status);
  }

  DeleteSelf();
}

void URLLoader::OnMojoDisconnect() {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamConsumerClosed(MojoResult result) {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    return;
  }

  ReadMore();
}

void URLLoader::DeleteSelf() {
  std::move(delete_callback_).Run(this);
}

void URLLoader::SendResponseToClient() {
  url_loader_client_->OnReceiveResponse(response_->head);

  net::IOBufferWithSize* metadata =
      url_request_->response_info().metadata.get();
  if (metadata) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(metadata->data());

    url_loader_client_->OnReceiveCachedMetadata(
        std::vector<uint8_t>(data, data + metadata->size()));
  }

  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
  response_ = nullptr;
}

void URLLoader::CompletePendingWrite(bool success) {
  if (success) {
    // The write can only be completed immediately in case of a success, since
    // doing so invalidates memory of any attached NetToMojoIOBuffer's; but in
    // case of an abort, particularly one caused by a suspend, the failure may
    // be delivered to URLLoader while the disk_cache layer is still hanging on
    // to the now-invalid IOBuffer in some worker thread trying to commit it to
    // disk.  In case of an error, this will have to wait till everything is
    // destroyed.
    response_body_stream_ =
        pending_write_->Complete(pending_write_buffer_offset_);
  }
  total_written_bytes_ += pending_write_buffer_offset_;
  pending_write_ = nullptr;
  pending_write_buffer_offset_ = 0;
}

void URLLoader::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

void URLLoader::SetRawRequestHeadersAndNotify(
    net::HttpRawRequestHeaders headers) {
  if (network_service_client_ && devtools_request_id()) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;
    header_array.reserve(headers.headers().size());

    for (const auto& header : headers.headers()) {
      network::mojom::HttpRawHeaderPairPtr pair =
          network::mojom::HttpRawHeaderPair::New();
      pair->key = header.first;
      pair->value = header.second;
      header_array.push_back(std::move(pair));
    }

    network_service_client_->OnRawRequest(
        GetProcessId(), GetRenderFrameId(), devtools_request_id().value(),
        url_request_->maybe_sent_cookies(), std::move(header_array));
  }

  if (network_context_client_) {
    net::CookieStatusList reported_cookies;
    for (const auto& cookie_and_status : url_request_->maybe_sent_cookies()) {
      if (ShouldNotifyAboutCookie(cookie_and_status.status)) {
        reported_cookies.push_back(
            {cookie_and_status.cookie, cookie_and_status.status});
      }
    }

    if (!reported_cookies.empty()) {
      network_context_client_->OnCookiesRead(
          /* is_service_worker = */ false, GetProcessId(), GetRenderFrameId(),
          url_request_->url(), url_request_->site_for_cookies(),
          reported_cookies);
    }
  }

  if (want_raw_headers_)
    raw_request_headers_.Assign(std::move(headers));
}

void URLLoader::SendUploadProgress(const net::UploadProgress& progress) {
  url_loader_client_->OnUploadProgress(
      progress.position(), progress.size(),
      base::BindOnce(&URLLoader::OnUploadProgressACK,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnUploadProgressACK() {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

void URLLoader::OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                              int net_error) {
  if (net_error == net::OK) {
    url_request_->ContinueDespiteLastError();
    return;
  }

  url_request_->CancelWithSSLError(net_error, ssl_info);
}

bool URLLoader::HasDataPipe() const {
  return pending_write_ || response_body_stream_.is_valid();
}

void URLLoader::RecordBodyReadFromNetBeforePausedIfNeeded() {
  if (update_body_read_before_paused_)
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  if (body_read_before_paused_ != -1) {
    if (!url_request_->was_cached()) {
      UMA_HISTOGRAM_COUNTS_1M("Network.URLLoader.BodyReadFromNetBeforePaused",
                              body_read_before_paused_);
    } else {
      DVLOG(1) << "The request has been paused, but "
               << "Network.URLLoader.BodyReadFromNetBeforePaused is not "
               << "reported because the response body may be from cache. "
               << "body_read_before_paused_: " << body_read_before_paused_;
    }
  }
}

void URLLoader::ResumeStart() {
  url_request_->LogUnblocked();
  url_request_->Start();
}

void URLLoader::OnBeforeSendHeadersComplete(
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* out_headers,
    int result,
    const base::Optional<net::HttpRequestHeaders>& headers) {
  if (headers)
    *out_headers = headers.value();
  std::move(callback).Run(result);
}

void URLLoader::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    base::Optional<GURL>* out_preserve_fragment_on_redirect_url,
    int result,
    const base::Optional<std::string>& headers,
    const base::Optional<GURL>& preserve_fragment_on_redirect_url) {
  if (headers) {
    *out_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(headers.value());
  }
  *out_preserve_fragment_on_redirect_url = preserve_fragment_on_redirect_url;
  std::move(callback).Run(result);
}

void URLLoader::CompleteBlockedResponse(int error_code,
                                        bool should_report_corb_blocking) {
  // The response headers and body shouldn't yet be sent to the URLLoaderClient.
  DCHECK(response_);
  DCHECK(consumer_handle_.is_valid());

  // Tell the URLLoaderClient that the response has been completed.
  URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = 0;
  status.encoded_body_length = 0;
  status.decoded_body_length = 0;
  status.should_report_corb_blocking = should_report_corb_blocking;
  url_loader_client_->OnComplete(status);

  // Reset the connection to the URLLoaderClient.  This helps ensure that we
  // won't accidentally leak any data to the renderer from this point on.
  url_loader_client_.reset();
}

URLLoader::BlockResponseForCorbResult URLLoader::BlockResponseForCorb() {
  // The response headers and body shouldn't yet be sent to the URLLoaderClient.
  DCHECK(response_);
  DCHECK(consumer_handle_.is_valid());

  // Send stripped headers to the real URLLoaderClient.
  CrossOriginReadBlocking::SanitizeBlockedResponse(&response_->head);
  url_loader_client_->OnReceiveResponse(response_->head);

  // Send empty body to the real URLLoaderClient.
  mojo::DataPipe empty_data_pipe(kBlockedBodyAllocationSize);
  empty_data_pipe.producer_handle.reset();
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(empty_data_pipe.consumer_handle));

  // Tell the real URLLoaderClient that the response has been completed.
  bool should_report_corb_blocking =
      corb_analyzer_->ShouldReportBlockedResponse();
  if (corb_detachable_) {
    // TODO(lukasza): https://crbug.com/827633#c5: Consider passing net::ERR_OK
    // instead.  net::ERR_ABORTED was chosen for consistency with the old CORB
    // implementation that used to go through DetachableResourceHandler.
    CompleteBlockedResponse(net::ERR_ABORTED, should_report_corb_blocking);
  } else {
    // CORB responses are reported as a success.
    CompleteBlockedResponse(net::OK, should_report_corb_blocking);
  }

  // If the factory is asking to complete requests of this type, then we need to
  // continue processing the response to make sure the network cache is
  // populated.  Otherwise we can cancel the request.
  if (corb_detachable_) {
    // Discard any remaining callbacks or data by rerouting the pipes to
    // EmptyURLLoaderClient (deleting |self_ptr| when the URL request
    // completes).
    mojom::URLLoaderPtr self_ptr;
    receiver_.reset();
    receiver_.Bind(mojo::MakeRequest(&self_ptr));
    receiver_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
    EmptyURLLoaderClient::DrainURLRequest(
        url_loader_client_.BindNewPipeAndPassReceiver(), std::move(self_ptr));

    // Ask the caller to continue processing the request.
    return kContinueRequest;
  }
  // Delete self and cancel the request - the caller doesn't need to continue.
  //
  // DeleteSelf is posted asynchronously, to make sure that the callers (e.g.
  // URLLoader::OnResponseStarted and/or URLLoader::DidRead instance methods)
  // can still safely dereference |this|.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&URLLoader::DeleteSelf, weak_ptr_factory_.GetWeakPtr()));
  return kWillCancelRequest;
}

void URLLoader::ReportFlaggedResponseCookies() {
  if (network_service_client_ && devtools_request_id() &&
      url_request_->response_headers()) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;

    size_t iterator = 0;
    std::string name, value;
    while (url_request_->response_headers()->EnumerateHeaderLines(
        &iterator, &name, &value)) {
      network::mojom::HttpRawHeaderPairPtr pair =
          network::mojom::HttpRawHeaderPair::New();
      pair->key = name;
      pair->value = value;
      header_array.push_back(std::move(pair));
    }

    // Only send the "raw" header text when the headers were actually send in
    // text form (i.e. not QUIC or SPDY)
    base::Optional<std::string> raw_response_headers;

    const net::HttpResponseInfo& response_info = url_request_->response_info();

    if (!response_info.DidUseQuic() && !response_info.was_fetched_via_spdy) {
      raw_response_headers =
          base::make_optional(net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              url_request_->response_headers()->raw_headers()));
    }

    network_service_client_->OnRawResponse(
        GetProcessId(), GetRenderFrameId(), devtools_request_id().value(),
        url_request_->maybe_stored_cookies(), std::move(header_array),
        raw_response_headers);
  }

  if (network_context_client_) {
    net::CookieStatusList reported_cookies;
    for (const auto& cookie_line_and_status :
         url_request_->maybe_stored_cookies()) {
      if (ShouldNotifyAboutCookie(cookie_line_and_status.status) &&
          cookie_line_and_status.cookie) {
        reported_cookies.push_back({cookie_line_and_status.cookie.value(),
                                    cookie_line_and_status.status});
      }
    }

    if (!reported_cookies.empty()) {
      network_context_client_->OnCookiesChanged(
          /* is_service_worker = */ false, GetProcessId(), GetRenderFrameId(),
          url_request_->url(), url_request_->site_for_cookies(),
          reported_cookies);
    }
  }
}

void URLLoader::StartReading() {
  if (!is_more_mime_sniffing_needed_ && !is_more_corb_sniffing_needed_) {
    // Treat feed types as text/plain.
    if (response_->head.mime_type == "application/rss+xml" ||
        response_->head.mime_type == "application/atom+xml") {
      response_->head.mime_type.assign("text/plain");
    }
    SendResponseToClient();
  }

  // Start reading...
  ReadMore();
}

void URLLoader::OnOriginPolicyManagerRetrieveDone(
    const OriginPolicy& origin_policy) {
  response_->head.origin_policy = origin_policy;

  StartReading();
}

}  // namespace network
