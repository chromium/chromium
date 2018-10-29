// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_document_resource_handler.h"

#include <string.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/url_request/url_request.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using MimeType = network::CrossOriginReadBlocking::MimeType;

namespace content {

namespace {

// An IOBuffer to enable writing into a existing IOBuffer at a given offset.
class LocalIoBufferWithOffset : public net::WrappedIOBuffer {
 public:
  LocalIoBufferWithOffset(net::IOBuffer* buf, int offset)
      : net::WrappedIOBuffer(buf->data() + offset), buf_(buf) {}

 private:
  ~LocalIoBufferWithOffset() override {}

  scoped_refptr<net::IOBuffer> buf_;
};

}  // namespace

// static
void CrossSiteDocumentResourceHandler::LogBlockedResponseOnUIThread(
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    bool needed_sniffing,
    MimeType canonical_mime_type,
    ResourceType resource_type,
    int http_response_code,
    int64_t content_length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = std::move(web_contents_getter).Run();
  if (!web_contents)
    return;

  ukm::SourceId source_id = static_cast<WebContentsImpl*>(web_contents)
                                ->GetUkmSourceIdForLastCommittedSource();
  ukm::builders::SiteIsolation_XSD_Browser_Blocked(source_id)
      .SetCanonicalMimeType(static_cast<int64_t>(canonical_mime_type))
      .SetContentLengthWasZero(content_length == 0)
      .SetContentResourceType(resource_type)
      .SetHttpResponseCode(http_response_code)
      .SetNeededSniffing(needed_sniffing)
      .Record(ukm::UkmRecorder::Get());
}

void CrossSiteDocumentResourceHandler::LogBlockedResponse(
    ResourceRequestInfoImpl* resource_request_info,
    int http_response_code) {
  DCHECK(resource_request_info);
  DCHECK(analyzer_);
  DCHECK_NE(network::CrossOriginReadBlocking::MimeType::kInvalidMimeType,
            analyzer_->canonical_mime_type());

  analyzer_->LogBlockedResponse();

  ResourceType resource_type = resource_request_info->GetResourceType();
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked", resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);
  switch (analyzer_->canonical_mime_type()) {
    case MimeType::kHtml:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.HTML",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kXml:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.XML",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kJson:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.JSON",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kPlain:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.Plain",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kOthers:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.Others",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    default:
      NOTREACHED();
  }
  if (analyzer_->found_parser_breaker()) {
    UMA_HISTOGRAM_ENUMERATION(
        "SiteIsolation.XSD.Browser.BlockedForParserBreaker", resource_type,
        content::RESOURCE_TYPE_LAST_TYPE);
  }

  // The last committed URL is only available on the UI thread - we need to hop
  // onto the UI thread to log an UKM event.  Note that this is racey - by the
  // time the posted task runs, the WebContents could have been closed and/or
  // navigated to another URL.  This is understood and acceptable - this should
  // be rare enough to not matter for the collected UKM data.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &CrossSiteDocumentResourceHandler::LogBlockedResponseOnUIThread,
          resource_request_info->GetWebContentsGetterForRequest(),
          analyzer_->needs_sniffing(), analyzer_->canonical_mime_type(),
          resource_type, http_response_code, analyzer_->content_length()));
}

// ResourceController that runs a closure on Resume(), and forwards failures
// back to CrossSiteDocumentHandler. The closure can optionally be run as
// a PostTask.
class CrossSiteDocumentResourceHandler::Controller : public ResourceController {
 public:
  explicit Controller(CrossSiteDocumentResourceHandler* document_handler,
                      bool post_task,
                      base::OnceClosure resume_callback)
      : document_handler_(document_handler),
        resume_callback_(std::move(resume_callback)),
        post_task_(post_task) {}

  ~Controller() override {}

  // ResourceController implementation:
  void Resume() override {
    MarkAsUsed();

    if (post_task_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(resume_callback_));
    } else {
      std::move(resume_callback_).Run();
    }
  }

  void ResumeForRedirect(const base::Optional<net::HttpRequestHeaders>&
                             modified_request_headers) override {
    DCHECK(!modified_request_headers.has_value())
        << "Redirect with modified headers was not supported yet. "
           "crbug.com/845683";
    Resume();
  }

  void Cancel() override {
    MarkAsUsed();
    document_handler_->Cancel();
  }

  void CancelWithError(int error_code) override {
    MarkAsUsed();
    document_handler_->CancelWithError(error_code);
  }

 private:
  void MarkAsUsed() {
#if DCHECK_IS_ON()
    DCHECK(!used_);
    used_ = true;
#endif
  }

#if DCHECK_IS_ON()
  bool used_ = false;
#endif

  CrossSiteDocumentResourceHandler* document_handler_;

  // Runs on Resume().
  base::OnceClosure resume_callback_;
  bool post_task_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

CrossSiteDocumentResourceHandler::CrossSiteDocumentResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request,
    bool is_nocors_plugin_request)
    : LayeredResourceHandler(request, std::move(next_handler)),
      weak_next_handler_(next_handler_.get()),
      is_nocors_plugin_request_(is_nocors_plugin_request),
      weak_this_(this) {}

CrossSiteDocumentResourceHandler::~CrossSiteDocumentResourceHandler() {}

void CrossSiteDocumentResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  has_response_started_ = true;

  if (request()->initiator().has_value()) {
    const char* initiator_scheme_exception =
        GetContentClient()
            ->browser()
            ->GetInitiatorSchemeBypassingDocumentBlocking();
    is_initiator_scheme_excluded_ =
        initiator_scheme_exception &&
        request()->initiator().value().scheme() == initiator_scheme_exception;
  }

  network::CrossOriginReadBlocking::LogAction(
      network::CrossOriginReadBlocking::Action::kResponseStarted);

  should_block_based_on_headers_ = ShouldBlockBasedOnHeaders(*response);

  // If blocking is possible, postpone forwarding |response| to the
  // |next_handler_|, until we have made the allow-vs-block decision
  // (which might need more time depending on the sniffing needs).
  if (should_block_based_on_headers_) {
    pending_response_start_ = response;
    controller->Resume();
  } else {
    next_handler_->OnResponseStarted(response, std::move(controller));
  }
}

void CrossSiteDocumentResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  // For allowed responses, the data is directly streamed to the next handler.
  // Note that OnWillRead may be called before OnResponseStarted (because the
  // MimeSniffingResourceHandler upstream changes the order of the calls) - this
  // means that |has_response_started_| has to be explicitly checked below.
  if (has_response_started_ &&
      (!should_block_based_on_headers_ || allow_based_on_sniffing_)) {
    DCHECK(!local_buffer_);
    next_handler_->OnWillRead(buf, buf_size, std::move(controller));
    return;
  }

  // If |local_buffer_| exists, continue buffering data into the end of it.
  if (local_buffer_) {
    // Check that we still have room for more local bufferring.
    DCHECK_GT(next_handler_buffer_size_, local_buffer_bytes_read_);
    *buf = new LocalIoBufferWithOffset(local_buffer_.get(),
                                       local_buffer_bytes_read_);
    *buf_size = next_handler_buffer_size_ - local_buffer_bytes_read_;
    controller->Resume();
    return;
  }

  // On the next read attempt after the response was blocked, either cancel the
  // rest of the request or allow it to proceed in a detached state.
  if (blocked_read_completed_) {
    DCHECK(should_block_based_on_headers_);
    DCHECK(!allow_based_on_sniffing_);
    const ResourceRequestInfoImpl* info = GetRequestInfo();
    if (info && info->detachable_handler()) {
      // Ensure that prefetch, etc, continue to cache the response, without
      // sending it to the renderer.
      info->detachable_handler()->Detach();
    } else {
      // If it's not detachable, cancel the rest of the request.
      controller->Cancel();
    }
    return;
  }

  // If we haven't yet decided to allow or block the response, we should read
  // the data into a local buffer 1) to temporarily prevent the data from
  // reaching the renderer and 2) to potentially sniff the data to confirm the
  // content type.
  //
  // Since the downstream handler may defer during the OnWillRead call below,
  // the values of |buf| and |buf_size| may not be available right away.
  // Instead, create a Controller that will start the sniffing after the
  // downstream handler has called Resume on it.
  HoldController(std::move(controller));
  controller = std::make_unique<Controller>(
      this, false /* post_task */,
      base::BindOnce(&CrossSiteDocumentResourceHandler::ResumeOnWillRead,
                     weak_this_.GetWeakPtr(), buf, buf_size));
  next_handler_->OnWillRead(buf, buf_size, std::move(controller));
}

void CrossSiteDocumentResourceHandler::ResumeOnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size) {
  // We should only get here if we haven't yet made a block-vs-allow decision
  // (we get here after downstream handler finishes its work from OnWillRead).
  DCHECK(!allow_based_on_sniffing_);
  DCHECK(!blocked_read_completed_);

  // For most blocked responses, we need to sniff the data to confirm it looks
  // like the claimed MIME type (to avoid blocking mislabeled JavaScript,
  // JSONP, etc).  Read this data into a separate buffer (not shared with the
  // renderer), which we will only copy over if we decide to allow it through.
  // This is only done when we suspect the response should be blocked.
  //
  // Make it as big as the downstream handler's buffer to make it easy to copy
  // over in one operation.
  DCHECK_GE(*buf_size, net::kMaxBytesToSniff * 2);
  local_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(*buf_size));

  // Store the next handler's buffer but don't read into it while sniffing,
  // since we possibly won't want to send the data to the renderer process.
  next_handler_buffer_ = *buf;
  next_handler_buffer_size_ = *buf_size;
  *buf = local_buffer_;

  Resume();
}

void CrossSiteDocumentResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(has_response_started_);
  DCHECK(!blocked_read_completed_);

  if (!should_block_based_on_headers_) {
    // CrossSiteDocumentResourceHandler always intercepts the buffer allocated
    // by the first call to |next_handler_|'s OnWillRead and passes the
    // |local_buffer_| upstream.  If we decide not to block based on headers,
    // then the data needs to be passed into the |next_handler_|.
    if (local_buffer_) {
      DCHECK_EQ(0, local_buffer_bytes_read_);
      local_buffer_bytes_read_ = bytes_read;
      StopLocalBuffering(true /* = copy_data_to_next_handler */);
    }

    next_handler_->OnReadCompleted(bytes_read, std::move(controller));
    return;
  }

  if (allow_based_on_sniffing_) {
    // If CrossSiteDocumentResourceHandler decided to allow the response based
    // on sniffing, then StopLocalBuffering was already called below by the
    // previous execution of CrossSiteDocumentResourceHandler::OnReadCompleted.
    // From there onward, we just need to foward all the calls to the
    // |next_handler_|.
    DCHECK(!local_buffer_);
    next_handler_->OnReadCompleted(bytes_read, std::move(controller));
    return;
  }

  // If |next_handler_->OnReadCompleted(...)| was not called above, then the
  // response bytes are being accumulated in the local buffer we've allocated in
  // ResumeOnWillRead.
  const size_t new_data_offset = local_buffer_bytes_read_;
  local_buffer_bytes_read_ += bytes_read;

  // If we intended to block the response and haven't sniffed yet, try to
  // confirm that we should block it.  If sniffing is needed, look at the local
  // buffer and either report that zero bytes were read (to indicate the
  // response is empty and complete), or copy the sniffed data to the next
  // handler's buffer and resume the response without blocking.
  bool confirmed_blockable = false;
  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!analyzer_->needs_sniffing()) {
    // If sniffing is impossible (e.g., because this is a range request), or
    // if sniffing is disabled due to a nosniff header AND the server returned
    // a protected mime type, then we have enough information to block
    // immediately.
    confirmed_blockable = true;
  } else {
    // Sniff the data to see if it likely matches the MIME type that caused us
    // to decide to block it.  If it doesn't match, it may be JavaScript,
    // JSONP, or another allowable data type and we should let it through.
    // Record how many bytes were read to see how often it's too small.  (This
    // will typically be under 100,000.)
    DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);
    const bool more_data_possible =
        bytes_read != 0 && local_buffer_bytes_read_ < net::kMaxBytesToSniff &&
        local_buffer_bytes_read_ < next_handler_buffer_size_;

    // To ensure determinism with respect to network packet ordering and
    // sizing, never examine more than kMaxBytesToSniff bytes, even if more
    // are available.
    size_t bytes_to_sniff =
        std::min(local_buffer_bytes_read_, net::kMaxBytesToSniff);
    base::StringPiece data(local_buffer_->data(), bytes_to_sniff);

    // If we have some new data, ask the |analyzer_| to sniff it.
    analyzer_->SniffResponseBody(data, new_data_offset);

    const bool confirmed_allowed = analyzer_->ShouldAllow();
    confirmed_blockable = analyzer_->ShouldBlock();
    DCHECK(!(confirmed_blockable && confirmed_allowed));

    // If sniffing didn't yield a conclusive response, and we haven't read too
    // many bytes yet or hit the end of the stream, buffer up some more data.
    if (!(confirmed_blockable || confirmed_allowed) && more_data_possible) {
      controller->Resume();
      return;
    }
  }

  // At this point the block-vs-allow decision was made, but might be still
  // suppressed because of |is_initiator_scheme_excluded_|.  We perform the
  // suppression at such a late point, because we want to ensure we only call
  // LogInitiatorSchemeBypassingDocumentBlocking for cases that actuall matter
  // in practice.
  if (confirmed_blockable && is_initiator_scheme_excluded_) {
    initiator_scheme_prevented_blocking_ = true;
    confirmed_blockable = false;
  }

  // At this point we have already made a block-vs-allow decision and we know
  // that we can wake the |next_handler_| and let it catch-up with our
  // processing of the response.  The first step will always be calling
  // |next_handler_->OnResponseStarted(...)|, but we need to figure out what
  // other steps need to happen, before we can resume the real response
  // upstream.  These steps will be gathered into |controller|.
  // The last step will always be calling
  // CrossSiteDocumentResourceHandler::Resume.
  HoldController(std::move(controller));
  controller = std::make_unique<Controller>(
      this, false /* post_task */,
      base::BindOnce(&CrossSiteDocumentResourceHandler::Resume,
                     weak_this_.GetWeakPtr()));

  if (confirmed_blockable) {
    // Log the blocking event.  Inline the Serialize call to avoid it when
    // tracing is disabled.
    TRACE_EVENT2("navigation",
                 "CrossSiteDocumentResourceHandler::ShouldBlockResponse",
                 "initiator",
                 request()->initiator().has_value()
                     ? request()->initiator().value().Serialize()
                     : "null",
                 "url", request()->url().spec());

    LogBlockedResponse(info, analyzer_->http_response_code());

    // Block the response and throw away the data.  Report zero bytes read.
    blocked_read_completed_ = true;
    info->set_blocked_response_from_reaching_renderer(true);
    if (analyzer_->ShouldReportBlockedResponse())
      info->set_should_report_corb_blocking(true);
    network::CrossOriginReadBlocking::SanitizeBlockedResponse(
        pending_response_start_);

    // Pass an empty/blocked body onto the next handler.  size of the two
    // buffers is the same (see OnWillRead).  After the next statement,
    // |controller| will store a sequence of steps like this:
    //  - next_handler_->OnReadCompleted(bytes_read = 0)
    //  - ... steps from the old |controller| (typically this->Resume()) ...
    controller = std::make_unique<Controller>(
        this, true /* post_task */,
        base::BindOnce(&ResourceHandler::OnReadCompleted,
                       weak_next_handler_.GetWeakPtr(), 0 /* bytes_read */,
                       std::move(controller)));
    StopLocalBuffering(false /* = copy_data_to_next_handler*/);
  } else {
    // Choose not block this response.
    allow_based_on_sniffing_ = true;

    if (bytes_read == 0 && local_buffer_bytes_read_ != 0) {
      // |bytes_read == 0| indicates the end-of-stream. In this case, we need
      // to synthesize an additional OnWillRead() and OnReadCompleted(0) on
      // |next_handler_|, so that |next_handler_| sees both the full response
      // and the end-of-stream marker.  After the next statement, |controller|
      // will store a sequence of steps like this:
      //  - next_handler_->OnWillRead(...)
      //  - next_handler_->OnReadCompleted(bytes_read = 0)
      //  - ... steps from the old |controller| (typically this->Resume()) ...
      //
      // Note that if |weak_next_handler_| is alive, then |this| should also be
      // alive and therefore it is safe to dereference |&next_handler_buffer_|
      // and |&next_handler_buffer_size_|.
      controller = std::make_unique<Controller>(
          this, false /* post_task */,
          base::BindOnce(
              &ResourceHandler::OnWillRead, weak_next_handler_.GetWeakPtr(),
              &next_handler_buffer_, &next_handler_buffer_size_,
              std::make_unique<Controller>(
                  this, true /* post_task */,
                  base::BindOnce(&ResourceHandler::OnReadCompleted,
                                 weak_next_handler_.GetWeakPtr(),
                                 0 /* bytes_read */, std::move(controller)))));
    }

    // Pass the contents of |local_buffer_| onto the next handler.  Afterwards,
    // |controller| will store a sequence of steps like this:
    //  - next_handler_->OnReadCompleted(local_buffer_bytes_read_)
    //  - ... steps from the old |controller| ...
    controller = std::make_unique<Controller>(
        this, true /* post_task */,
        base::BindOnce(&ResourceHandler::OnReadCompleted,
                       weak_next_handler_.GetWeakPtr(),
                       local_buffer_bytes_read_, std::move(controller)));
    StopLocalBuffering(true /* = copy_data_to_next_handler*/);
  }

  // In both the blocked and allowed cases, we need to resume by notifying the
  // downstream handler about the response start.
  DCHECK(pending_response_start_);
  next_handler_->OnResponseStarted(pending_response_start_.get(),
                                   std::move(controller));
  pending_response_start_ = nullptr;
}

void CrossSiteDocumentResourceHandler::StopLocalBuffering(
    bool copy_data_to_next_handler) {
  DCHECK(has_response_started_);
  DCHECK(!should_block_based_on_headers_ || allow_based_on_sniffing_ ||
         blocked_read_completed_);
  DCHECK(local_buffer_);
  DCHECK(next_handler_buffer_);

  if (copy_data_to_next_handler) {
    // Pass the contents of |local_buffer_| onto the next handler. Note that the
    // size of the two buffers is the same (see OnWillRead).
    DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);
    memcpy(next_handler_buffer_->data(), local_buffer_->data(),
           local_buffer_bytes_read_);
  }

  local_buffer_ = nullptr;
  local_buffer_bytes_read_ = 0;
  next_handler_buffer_ = nullptr;
  next_handler_buffer_size_ = 0;
}

void CrossSiteDocumentResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (blocked_read_completed_) {
    // Report blocked responses as successful, rather than the cancellation
    // from OnWillRead.
    next_handler_->OnResponseCompleted(net::URLRequestStatus(),
                                       std::move(controller));
  } else {
    // Only report CORB status for successful (i.e. non-aborted,
    // non-errored-out) requests.
    if (status.is_success()) {
      analyzer_->LogAllowedResponse();
      if (initiator_scheme_prevented_blocking_ &&
          analyzer_->ShouldReportBlockedResponse() && GetRequestInfo()) {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&ContentBrowserClient::
                               LogInitiatorSchemeBypassingDocumentBlocking,
                           base::Unretained(GetContentClient()->browser()),
                           request()->initiator().value(),
                           GetRequestInfo()->GetChildID(),
                           GetRequestInfo()->GetResourceType()));
      }
    }

    next_handler_->OnResponseCompleted(status, std::move(controller));
  }
}

bool CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders(
    const network::ResourceResponse& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delegate most decisions to CrossOriginReadBlocking::ResponseAnalyzer.
  analyzer_ =
      std::make_unique<network::CrossOriginReadBlocking::ResponseAnalyzer>(
          *request(), response);
  if (analyzer_->ShouldAllow())
    return false;

  // --disable-web-security also disables Cross-Origin Read Blocking (CORB).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity))
    return false;

  // Only block if this is a request made from a renderer process.
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info || info->GetChildID() == -1)
    return false;

  // Don't block some plugin requests.
  //
  // Note that in practice this exception only only matters to Flash and test
  // plugins (both can issue requests without CORS and both will be covered by
  // CORB::ShouldAllowForPlugin below).
  //
  // This exception is not needed for:
  // - PNaCl (which always issues CORS requests)
  // - PDF (which is already covered by the exception for chrome-extension://...
  //   initiators and therefore doesn't need another exception here;
  //   additionally PDF doesn't _really_ make *cross*-origin requests - it just
  //   seems that way because of the usage of the Chrome extension).
  if (info->GetResourceType() == RESOURCE_TYPE_PLUGIN_RESOURCE &&
      is_nocors_plugin_request_ &&
      network::CrossOriginReadBlocking::ShouldAllowForPlugin(
          info->GetChildID())) {
    return false;
  }

  return true;
}

}  // namespace content
