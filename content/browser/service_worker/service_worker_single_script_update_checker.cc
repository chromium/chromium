// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kUpdateCheckTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("service_worker_update_checker",
                                        R"(
    semantics {
      sender: "ServiceWorker System"
      description:
        "This request is issued by an update check to fetch the content of "
        "the new scripts."
      trigger:
        "ServiceWorker's update logic, which is triggered by a navigation to a "
        "site controlled by a service worker."
      data:
        "No body. 'Service-Worker: script' header is attached when it's the "
        "main worker script. Requests may include cookies and credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "Users can control this feature via the 'Cookies' setting under "
        "'Privacy, Content settings'. If cookies are disabled for a single "
        "site, serviceworkers are disabled for the site only. If they are "
        "totally disabled, all serviceworker requests will be stopped."
      chrome_policy {
        CookiesBlockedForUrls {
          CookiesBlockedForUrls: { entries: '*' }
        }
      }
      chrome_policy {
        CookiesAllowedForUrls {
          CookiesAllowedForUrls { }
        }
      }
      chrome_policy {
        DefaultCookiesSetting {
          DefaultCookiesSetting: 2
        }
      }
    }
    comments:
      "Chrome would be unable to update service workers without this type of "
      "request. Using either CookiesBlockedForUrls or CookiesAllowedForUrls "
      "policies (or a combination of both) limits the scope of these requests."
    )");

}  // namespace

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerSingleScriptUpdateChecker::WrappedIOBuffer
    : public net::WrappedIOBuffer {
 public:
  WrappedIOBuffer(const char* data, size_t size)
      : net::WrappedIOBuffer(base::make_span(data, size)) {}

 private:
  ~WrappedIOBuffer() override = default;

  // This is to make sure that the vtable is not merged with other classes.
  virtual void dummy() { NOTREACHED_IN_MIGRATION(); }
};

ServiceWorkerSingleScriptUpdateChecker::ServiceWorkerSingleScriptUpdateChecker(
    const GURL& script_url,
    bool is_main_script,
    const GURL& main_script_url,
    const GURL& scope,
    bool force_bypass_cache,
    blink::mojom::ScriptType worker_script_type,
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
    const blink::mojom::FetchClientSettingsObjectPtr&
        fetch_client_settings_object,
    base::TimeDelta time_since_last_check,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
    int64_t writer_resource_id,
    ScriptChecksumUpdateOption script_checksum_update_option,
    const blink::StorageKey& storage_key,
    ResultCallback callback)
    : script_url_(script_url),
      is_main_script_(is_main_script),
      scope_(scope),
      force_bypass_cache_(force_bypass_cache),
      update_via_cache_(update_via_cache),
      time_since_last_check_(time_since_last_check),
      script_checksum_update_option_(script_checksum_update_option),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunner::GetCurrentDefault()),
      callback_(std::move(callback)) {
  DCHECK(browser_context);

  TRACE_EVENT_WITH_FLOW2("ServiceWorker",
                         "ServiceWorkerSingleScriptUpdateChecker::"
                         "ServiceWorkerSingleScriptUpdateChecker",
                         this, TRACE_EVENT_FLAG_FLOW_OUT, "script_url",
                         script_url.spec(), "main_script_url",
                         main_script_url.spec());

  network::ResourceRequest resource_request =
      service_worker_loader_helpers::CreateRequestForServiceWorkerScript(
          script_url, storage_key, is_main_script_, worker_script_type,
          *fetch_client_settings_object, *browser_context);

  uint32_t options = network::mojom::kURLLoadOptionNone;
  if (is_main_script_) {
    // Request SSLInfo. It will be persisted in service worker storage and
    // may be used by ServiceWorkerMainResourceLoader for navigations handled
    // by this service worker.
    options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;
  }

  // Upgrade the request to an a priori authenticated URL, if appropriate.
  // https://w3c.github.io/webappsec-upgrade-insecure-requests/#upgrade-request
  // TODO(crbug.com/40637521): Set |ResourceRequest::upgrade_if_insecure_|
  // appropriately.

  if (service_worker_loader_helpers::ShouldValidateBrowserCacheForScript(
          is_main_script_, force_bypass_cache_, update_via_cache_,
          time_since_last_check_)) {
    resource_request.load_flags |= net::LOAD_VALIDATE_CACHE;
  }

  ServiceWorkerCacheWriter::ChecksumUpdateTiming checksum_update_timing;
  switch (script_checksum_update_option_) {
    case ScriptChecksumUpdateOption::kForceUpdate:
      checksum_update_timing =
          ServiceWorkerCacheWriter::ChecksumUpdateTiming::kAlways;
      break;
    case ScriptChecksumUpdateOption::kDefault:
      checksum_update_timing =
          ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch;
      break;
  }

  cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      writer_resource_id, /*pause_when_not_identical=*/true,
      checksum_update_timing);

  // Service worker update checking doesn't have a relevant frame and tab, so
  // that `web_contents_getter` returns nullptr and the frame id is set to
  // an invalid FrameTreeNodeId.
  base::RepeatingCallback<WebContents*()> web_contents_getter =
      base::BindRepeating([]() -> WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      CreateContentBrowserURLLoaderThrottles(
          resource_request, browser_context, std::move(web_contents_getter),
          /*navigation_ui_data=*/nullptr, FrameTreeNodeId(),
          /*navigation_id=*/std::nullopt);

  network_client_remote_.Bind(
      network_client_receiver_.BindNewPipeAndPassRemote());
  network_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      network::SharedURLLoaderFactory::Create(loader_factory->Clone()),
      std::move(throttles), GlobalRequestID::MakeBrowserInitiated().request_id,
      options, &resource_request, network_client_remote_.get(),
      kUpdateCheckTrafficAnnotation,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  DCHECK_EQ(network_loader_state_,
            ServiceWorkerUpdatedScriptLoader::LoaderState::kNotStarted);
  network_loader_state_ =
      ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingHeader;
}

ServiceWorkerSingleScriptUpdateChecker::
    ~ServiceWorkerSingleScriptUpdateChecker() = default;

// URLLoaderClient override ----------------------------------------------------

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle consumer,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerSingleScriptUpdateChecker::OnReceiveResponse", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK_EQ(network_loader_state_,
            ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingHeader);

  blink::ServiceWorkerStatusCode service_worker_status;
  network::URLLoaderCompletionStatus completion_status;
  std::string error_message;
  if (!service_worker_loader_helpers::CheckResponseHead(
          *response_head, &service_worker_status, &completion_status,
          &error_message)) {
    DCHECK_NE(net::OK, completion_status.error_code);
    Fail(service_worker_status, error_message, completion_status);
    return;
  }

  // Check the path restriction defined in the spec:
  // https://w3c.github.io/ServiceWorker/#service-worker-script-response
  // Only main script needs the following check.
  if (is_main_script_) {
    std::string service_worker_allowed;
    bool has_header = response_head->headers->EnumerateHeader(
        nullptr, ServiceWorkerConsts::kServiceWorkerAllowed,
        &service_worker_allowed);
    if (!service_worker_loader_helpers::IsPathRestrictionSatisfied(
            scope_, script_url_, has_header ? &service_worker_allowed : nullptr,
            &error_message)) {
      Fail(blink::ServiceWorkerStatusCode::kErrorSecurity, error_message,
           network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE));
      return;
    }

    if (!GetContentClient()
             ->browser()
             ->ShouldServiceWorkerInheritPolicyContainerFromCreator(
                 script_url_)) {
      policy_container_host_ = base::MakeRefCounted<PolicyContainerHost>(
          // TODO(crbug.com/40867256): Ensure parsed headers are
          // available
          response_head->parsed_headers
              // This does not parse the referrer policy, which will be
              // updated in ServiceWorkerGlobalScope::Initialize
              ? PolicyContainerPolicies(script_url_, response_head.get(),
                                        nullptr)
              : PolicyContainerPolicies());
    }
  }

  network_accessed_ = response_head->network_accessed;

  WriteHeaders(std::move(response_head));

  if (!consumer)
    return;

  network_consumer_ = std::move(consumer);
  network_loader_state_ =
      ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerSingleScriptUpdateChecker::OnReceiveRedirect", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // Resource requests for the main service worker script should not follow
  // redirects.
  // Step 9.5: "Set request's redirect mode to "error"."
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  //
  // TODO(crbug.com/40595655): Follow redirects for imported scripts.
  Fail(blink::ServiceWorkerStatusCode::kErrorNetwork,
       ServiceWorkerConsts::kServiceWorkerRedirectError,
       network::URLLoaderCompletionStatus(net::ERR_INVALID_REDIRECT));
}

void ServiceWorkerSingleScriptUpdateChecker::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  // The network request for update checking shouldn't have upload data.
  NOTREACHED_IN_MIGRATION();
}

void ServiceWorkerSingleScriptUpdateChecker::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::
          kServiceWorkerSingleScriptUpdateChecker);
}

void ServiceWorkerSingleScriptUpdateChecker::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSingleScriptUpdateChecker::OnComplete",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
      net::ErrorToString(status.error_code));

  ServiceWorkerUpdatedScriptLoader::LoaderState previous_loader_state =
      network_loader_state_;
  network_loader_state_ =
      ServiceWorkerUpdatedScriptLoader::LoaderState::kCompleted;
  if (status.error_code != net::OK) {
    Fail(blink::ServiceWorkerStatusCode::kErrorNetwork,
         ServiceWorkerConsts::kServiceWorkerFetchScriptError, status);
    return;
  }

  DCHECK(previous_loader_state ==
             ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingHeader ||
         previous_loader_state ==
             ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingBody);

  // Response body is empty.
  if (previous_loader_state ==
      ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingHeader) {
    DCHECK_EQ(body_writer_state_,
              ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted);
    body_writer_state_ =
        ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted;
    switch (header_writer_state_) {
      case ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted:
        NOTREACHED_IN_MIGRATION()
            << "Response header should be received before OnComplete()";
        break;
      case ServiceWorkerUpdatedScriptLoader::WriterState::kWriting:
        // Wait until it's written. OnWriteHeadersComplete() will call
        // Finish().
        return;
      case ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted:
        DCHECK(!network_consumer_.is_valid());
        // Compare the cached data with an empty data to notify |cache_writer_|
        // of the end of the comparison.
        CompareData(nullptr /* pending_buffer */, 0 /* bytes_available */);
        break;
    }
  }

  // Response body exists.
  if (previous_loader_state ==
      ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingBody) {
    switch (body_writer_state_) {
      case ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted:
        DCHECK_EQ(header_writer_state_,
                  ServiceWorkerUpdatedScriptLoader::WriterState::kWriting);
        return;
      case ServiceWorkerUpdatedScriptLoader::WriterState::kWriting:
        DCHECK_EQ(header_writer_state_,
                  ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted);
        // Still reading the body from the network. Update checking will
        // complete when all the body is read or any difference is found.
        return;
      case ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted:
        DCHECK_EQ(header_writer_state_,
                  ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted);
        // Pass empty data to notify |cache_writer_| that comparison is
        // finished.
        CompareData(/*pending_buffer=*/nullptr, /*bytes_available=*/0);
        return;
    }
  }
}

// static
const char* ServiceWorkerSingleScriptUpdateChecker::ResultToString(
    Result result) {
  switch (result) {
    case Result::kNotCompared:
      return "Not compared";
    case Result::kFailed:
      return "Failed";
    case Result::kIdentical:
      return "Identical";
    case Result::kDifferent:
      return "Different";
  }
}

//------------------------------------------------------------------------------

void ServiceWorkerSingleScriptUpdateChecker::WriteHeaders(
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSingleScriptUpdateChecker::WriteHeaders",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK_EQ(header_writer_state_,
            ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted);
  header_writer_state_ =
      ServiceWorkerUpdatedScriptLoader::WriterState::kWriting;

  // Pass the header to the cache_writer_. This is written to the storage when
  // the body had changes.
  net::Error error = cache_writer_->MaybeWriteHeaders(
      std::move(response_head),
      base::BindOnce(
          &ServiceWorkerSingleScriptUpdateChecker::OnWriteHeadersComplete,
          weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteHeadersComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteHeaders() doesn't run the callback if it finishes synchronously,
  // so explicitly call it here.
  OnWriteHeadersComplete(error);
}

void ServiceWorkerSingleScriptUpdateChecker::OnWriteHeadersComplete(
    net::Error error) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerSingleScriptUpdateChecker::OnWriteHeadersComplete", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error", error);

  DCHECK_EQ(header_writer_state_,
            ServiceWorkerUpdatedScriptLoader::WriterState::kWriting);
  DCHECK_NE(error, net::ERR_IO_PENDING);
  header_writer_state_ =
      ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted;
  if (error != net::OK) {
    // This error means the cache writer failed to read script header from
    // storage.
    Fail(blink::ServiceWorkerStatusCode::kErrorDiskCache,
         ServiceWorkerConsts::kDatabaseErrorMessage,
         network::URLLoaderCompletionStatus(error));
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerSingleScriptUpdateChecker::
    MaybeStartNetworkConsumerHandleWatcher() {
  if (network_loader_state_ ==
      ServiceWorkerUpdatedScriptLoader::LoaderState::kLoadingHeader) {
    TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                           "ServiceWorkerSingleScriptUpdateChecker::"
                           "MaybeStartNetworkConsumerHandleWatcher",
                           this,
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "state", "wait for the body");

    // OnReceiveResponse() or OnComplete() will continue the sequence.
    return;
  }
  if (header_writer_state_ !=
      ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted) {
    TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                           "ServiceWorkerSingleScriptUpdateChecker::"
                           "MaybeStartNetworkConsumerHandleWatcher",
                           this,
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "state", "wait for writing header");

    DCHECK_EQ(header_writer_state_,
              ServiceWorkerUpdatedScriptLoader::WriterState::kWriting);
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }

  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerSingleScriptUpdateChecker::"
                         "MaybeStartNetworkConsumerHandleWatcher",
                         this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "state", "start loading body");

  DCHECK_EQ(body_writer_state_,
            ServiceWorkerUpdatedScriptLoader::WriterState::kNotStarted);
  body_writer_state_ = ServiceWorkerUpdatedScriptLoader::WriterState::kWriting;

  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(
          &ServiceWorkerSingleScriptUpdateChecker::OnNetworkDataAvailable,
          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerSingleScriptUpdateChecker::OnNetworkDataAvailable(
    MojoResult,
    const mojo::HandleSignalsState& state) {
  DCHECK_EQ(header_writer_state_,
            ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted);
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer);

  const uint32_t bytes_available = pending_buffer ? pending_buffer->size() : 0;
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerSingleScriptUpdateChecker::OnNetworkDataAvailable", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result", result,
      "bytes_available", bytes_available);

  switch (result) {
    case MOJO_RESULT_OK:
      CompareData(std::move(pending_buffer), bytes_available);
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      body_writer_state_ =
          ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted;
      // Closed by peer. This indicates all the data from the network service
      // are read or there is an error. In the error case, the reason is
      // notified via OnComplete().
      if (network_loader_state_ ==
          ServiceWorkerUpdatedScriptLoader::LoaderState::kCompleted) {
        // Compare the cached data with an empty data to notify |cache_writer_|
        // the end of the comparison.
        CompareData(nullptr /* pending_buffer */, 0 /* bytes_available */);
      }
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(result);
}

// |pending_buffer| is a buffer keeping a Mojo data pipe which is going to be
// read by a cache writer. It should be kept alive until the read is done. It's
// nullptr when there is no data to be read, and that means the body from the
// network reaches the end. In that case, |bytes_to_compare| is zero.
void ServiceWorkerSingleScriptUpdateChecker::CompareData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_to_compare) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSingleScriptUpdateChecker::CompareData",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK(pending_buffer || bytes_to_compare == 0);
  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr,
      pending_buffer ? pending_buffer->size() : 0);

  // Compare the network data and the stored data.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), bytes_to_compare,
      base::BindOnce(
          &ServiceWorkerSingleScriptUpdateChecker::OnCompareDataComplete,
          weak_factory_.GetWeakPtr(), pending_buffer, bytes_to_compare));

  if (error == net::ERR_IO_PENDING && !cache_writer_->is_pausing()) {
    // OnCompareDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnCompareDataComplete(std::move(pending_buffer), bytes_to_compare, error);
}

// |pending_buffer| is a buffer passed from CompareData(). Please refer to the
// comment on CompareData(). |error| is the result of the comparison done by the
// cache writer (which is actually reading and not yet writing to the cache,
// since it's in the comparison phase). It's net::OK when the body from the
// network and from the disk cache are the same, net::ERR_IO_PENDING if it
// detects a change in the script, or other error code if something went wrong
// reading from the disk cache.
void ServiceWorkerSingleScriptUpdateChecker::OnCompareDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_written,
    net::Error error) {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerSingleScriptUpdateChecker::OnCompareDataComplete", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error", error,
      "bytes_written", bytes_written);

  DCHECK(pending_buffer || bytes_written == 0);

  if (cache_writer_->is_pausing()) {
    // |cache_writer_| can be pausing only when it finds difference between
    // stored body and network body.
    DCHECK_EQ(error, net::ERR_IO_PENDING);
    auto paused_state = std::make_unique<PausedState>(
        std::move(cache_writer_), std::move(network_loader_),
        std::move(network_client_remote_), network_client_receiver_.Unbind(),
        std::move(pending_buffer), bytes_written, network_loader_state_,
        body_writer_state_);
    Succeed(Result::kDifferent, std::move(paused_state));
    return;
  }

  if (pending_buffer) {
    // We consumed |bytes_written| bytes of data from the network so call
    // CompleteRead(), regardless of what |error| is.
    pending_buffer->CompleteRead(bytes_written);
    network_consumer_ = pending_buffer->ReleaseHandle();
  }

  if (error != net::OK) {
    // Something went wrong reading from the disk cache.
    Fail(blink::ServiceWorkerStatusCode::kErrorDiskCache,
         ServiceWorkerConsts::kDatabaseErrorMessage,
         network::URLLoaderCompletionStatus(error));
    return;
  }

  if (bytes_written == 0) {
    // All data has been read. If we reach here without any error, the script
    // from the network was identical to the one in the disk cache.
    Succeed(Result::kIdentical, /*paused_state=*/nullptr);
    return;
  }

  network_watcher_.ArmOrNotify();
}

void ServiceWorkerSingleScriptUpdateChecker::Fail(
    blink::ServiceWorkerStatusCode status,
    const std::string& error_message,
    network::URLLoaderCompletionStatus network_status) {
  TRACE_EVENT_WITH_FLOW2("ServiceWorker",
                         "ServiceWorkerSingleScriptUpdateChecker::Fail", this,
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         blink::ServiceWorkerStatusToString(status),
                         "error_message", error_message);

  Finish(Result::kFailed,
         /*paused_state=*/nullptr,
         std::make_unique<FailureInfo>(status, error_message,
                                       std::move(network_status)),
         /*sha256_checksum=*/std::nullopt);
}

void ServiceWorkerSingleScriptUpdateChecker::Succeed(
    Result result,
    std::unique_ptr<PausedState> paused_state) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSingleScriptUpdateChecker::Succeed", this,
      TRACE_EVENT_FLAG_FLOW_IN, "result", ResultToString(result));
  DCHECK_NE(result, Result::kFailed);

  // Get calculated sha256 checksum when below conditions are both satisfied:
  // 1: |script_checksum_update_option_| is kForceUpdate.
  // 2: |result| is kIdentical.
  //
  // When the result is kDifferent, |cache_writer_| doesn't scan all the data
  // and it can be pausing. In this case, the finalized checksum is still not
  // available here, and that will be handled in
  // ServiceWorkerUpdatedScriptLoader.
  std::optional<std::string> sha256_checksum;
  if (script_checksum_update_option_ ==
          ScriptChecksumUpdateOption::kForceUpdate &&
      result == Result::kIdentical) {
    DCHECK(cache_writer_);
    DCHECK_EQ(cache_writer_->checksum_update_timing(),
              ServiceWorkerCacheWriter::ChecksumUpdateTiming::kAlways);
    sha256_checksum = cache_writer_->GetSha256Checksum();
  }

  Finish(result, std::move(paused_state), /*failure_info=*/nullptr,
         sha256_checksum);
}

void ServiceWorkerSingleScriptUpdateChecker::Finish(
    Result result,
    std::unique_ptr<PausedState> paused_state,
    std::unique_ptr<FailureInfo> failure_info,
    const std::optional<std::string>& sha256_checksum) {
  network_watcher_.Cancel();
  if (Result::kDifferent == result) {
    DCHECK(paused_state);
    // When the result if kDifferent, the checksum will be handled by
    // ServiceWorkerUpdatedScriptLoader.
    std::move(callback_).Run(script_url_, result, nullptr,
                             std::move(paused_state),
                             /*sha256_checksum=*/std::nullopt);
    return;
  }

  network_loader_.reset();
  network_client_receiver_.reset();
  network_consumer_.reset();
  std::move(callback_).Run(script_url_, result, std::move(failure_info),
                           nullptr, sha256_checksum);
}

ServiceWorkerSingleScriptUpdateChecker::PausedState::PausedState(
    std::unique_ptr<ServiceWorkerCacheWriter> cache_writer,
    std::unique_ptr<blink::ThrottlingURLLoader> network_loader,
    mojo::Remote<network::mojom::URLLoaderClient> network_client_remote,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        network_client_receiver,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_bytes,
    ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
    ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state)
    : cache_writer(std::move(cache_writer)),
      network_loader(std::move(network_loader)),
      network_client_remote(std::move(network_client_remote)),
      network_client_receiver(std::move(network_client_receiver)),
      pending_network_buffer(std::move(pending_network_buffer)),
      consumed_bytes(consumed_bytes),
      network_loader_state(network_loader_state),
      body_writer_state(body_writer_state) {}

ServiceWorkerSingleScriptUpdateChecker::PausedState::~PausedState() = default;

ServiceWorkerSingleScriptUpdateChecker::FailureInfo::FailureInfo(
    blink::ServiceWorkerStatusCode status,
    const std::string& error_message,
    network::URLLoaderCompletionStatus network_status)
    : status(status),
      error_message(error_message),
      network_status(network_status) {}

ServiceWorkerSingleScriptUpdateChecker::FailureInfo::~FailureInfo() = default;

}  // namespace content
