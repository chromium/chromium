// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

#include "net/tools/quic/quic_http_proxy_backend_stream.h"

namespace net {

// This is the Size of the buffer that consumes the response from the Backend
// The response is consumed upto 64KB at a time to avoid a large response
// from hogging resources from smaller responses.
const int QuicHttpProxyBackendStream::kBufferSize = 64000;
/*502 Bad Gateway
  The server was acting as a gateway or proxy and received an
  invalid response from the upstream server.*/
const int QuicHttpProxyBackendStream::kProxyHttpBackendError = 502;
// Hop-by-hop headers (small-caps). These are removed when sent to the backend.
// http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
// not Trailers per URL above;
// http://www.rfc-editor.org/errata_search.php?eid=4522
const std::set<std::string> QuicHttpProxyBackendStream::kHopHeaders = {
    "connection",
    "proxy-connection",  // non-standard but still sent by libcurl and rejected
                         // by e.g. google
    "keep-alive", "proxy-authenticate", "proxy-authorization",
    "te",       // canonicalized version of "TE"
    "trailer",  // not Trailers per URL above;
                // http://www.rfc-editor.org/errata_search.php?eid=4522
    "transfer-encoding", "upgrade",
};
const std::string QuicHttpProxyBackendStream::kDefaultQuicPeerIP = "Unknown";

QuicHttpProxyBackendStream::QuicHttpProxyBackendStream(
    QuicHttpProxyBackend* proxy_context)
    : proxy_context_(proxy_context),
      delegate_(nullptr),
      quic_peer_ip_(kDefaultQuicPeerIP),
      url_request_(nullptr),
      buf_(base::MakeRefCounted<IOBuffer>(kBufferSize)),
      response_completed_(false),
      headers_set_(false),
      quic_response_(new quic::QuicBackendResponse()) {}

QuicHttpProxyBackendStream::~QuicHttpProxyBackendStream() {}

void QuicHttpProxyBackendStream::Initialize(
    quic::QuicConnectionId quic_connection_id,
    quic::QuicStreamId quic_stream_id,
    std::string quic_peer_ip) {
  quic_connection_id_ = quic_connection_id;
  quic_stream_id_ = quic_stream_id;
  quic_peer_ip_ = quic_peer_ip;
  if (!quic_proxy_task_runner_.get()) {
    quic_proxy_task_runner_ = proxy_context_->GetProxyTaskRunner();
  } else {
    DCHECK_EQ(quic_proxy_task_runner_, proxy_context_->GetProxyTaskRunner());
  }

  quic_response_->set_response_type(
      quic::QuicBackendResponse::BACKEND_ERR_RESPONSE);
}

void QuicHttpProxyBackendStream::set_delegate(
    quic::QuicSimpleServerBackend::RequestHandler* delegate) {
  delegate_ = delegate;
  delegate_task_runner_ = base::SequencedTaskRunnerHandle::Get();
}

bool QuicHttpProxyBackendStream::SendRequestToBackend(
    const spdy::SpdyHeaderBlock* incoming_request_headers,
    const std::string& incoming_body) {
  DCHECK(proxy_context_->IsBackendInitialized())
      << " The quic-backend-proxy-context should be initialized";

  // Get Path From the Incoming Header Block
  spdy::SpdyHeaderBlock::const_iterator it =
      incoming_request_headers->find(":path");

  GURL url = proxy_context_->backend_url();
  std::string backend_spec = url.spec();
  if (it != incoming_request_headers->end()) {
    if (url.path().compare("/") == 0) {
      backend_spec.pop_back();
    }
    backend_spec.append(it->second.as_string());
  }

  url_ = GURL(backend_spec.c_str());
  if (!url_.is_valid()) {
    LOG(ERROR) << "Invalid URL received from QUIC client " << backend_spec;
    return false;
  }
  LOG(INFO) << "QUIC Proxy Making a request to the Backed URL: " + url_.spec();

  // Set the Method From the Incoming Header Block
  std::string method = "";
  it = incoming_request_headers->find(":method");
  if (it != incoming_request_headers->end()) {
    method.append(it->second.as_string());
  }
  if (ValidateHttpMethod(method) != true) {
    LOG(INFO) << "Unknown Request Type received from QUIC client " << method;
    return false;
  }
  CopyHeaders(incoming_request_headers);
  if (method_type_ == "POST" || method_type_ == "PUT" ||
      method_type_ == "PATCH") {
    // Upload content must be set
    if (!incoming_body.empty()) {
      std::unique_ptr<UploadElementReader> reader(new UploadBytesElementReader(
          incoming_body.data(), incoming_body.size()));
      SetUpload(
          ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
    }
  }
  // Start the request on the backend thread
  bool posted = quic_proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicHttpProxyBackendStream::SendRequestOnBackendThread,
                     weak_factory_.GetWeakPtr()));
  return posted;
}

void QuicHttpProxyBackendStream::CopyHeaders(
    const spdy::SpdyHeaderBlock* incoming_request_headers) {
  // Set all the request headers
  // Add or append the X-Forwarded-For Header and X-Real-IP
  for (spdy::SpdyHeaderBlock::const_iterator it =
           incoming_request_headers->begin();
       it != incoming_request_headers->end(); ++it) {
    std::string key = it->first.as_string();
    std::string value = it->second.as_string();
    // Ignore the spdy headers
    if (!key.empty() && key[0] != ':') {
      // Remove hop-by-hop headers
      if (base::Contains(kHopHeaders, key)) {
        LOG(INFO) << "QUIC Proxy Ignoring Hop-by-hop Request Header: " << key
                  << ":" << value;
      } else {
        LOG(INFO) << "QUIC Proxy Copying to backend Request Header: " << key
                  << ":" << value;
        AddRequestHeader(key, value);
      }
    }
  }
  // ToDo append proxy ip when x_forwarded_for header already present
  AddRequestHeader("X-Forwarded-For", quic_peer_ip_);
}

bool QuicHttpProxyBackendStream::ValidateHttpMethod(std::string method) {
  // Http method is a token, just as header name.
  if (!net::HttpUtil::IsValidHeaderName(method))
    return false;
  method_type_ = method;
  return true;
}

bool QuicHttpProxyBackendStream::AddRequestHeader(std::string name,
                                                  std::string value) {
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return false;
  }
  request_headers_.SetHeader(name, value);
  return true;
}

void QuicHttpProxyBackendStream::SetUpload(
    std::unique_ptr<net::UploadDataStream> upload) {
  DCHECK(!upload_);
  upload_ = std::move(upload);
}

void QuicHttpProxyBackendStream::SendRequestOnBackendThread() {
  DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  url_request_ = proxy_context_->GetURLRequestContext()->CreateRequest(
      url_, net::DEFAULT_PRIORITY, this, MISSING_TRAFFIC_ANNOTATION);
  url_request_->set_method(method_type_);
  url_request_->SetExtraRequestHeaders(request_headers_);
  if (upload_) {
    url_request_->set_upload(std::move(upload_));
  }
  url_request_->Start();
  VLOG(1) << "Quic Proxy Sending Request to Backend for quic_conn_id: "
          << quic_connection_id_ << " quic_stream_id: " << quic_stream_id_
          << " url: " << url_;
}

void QuicHttpProxyBackendStream::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_EQ(request, url_request_.get());
  DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  // Do not defer redirect, retry again from the proxy with the new url
  *defer_redirect = false;
  LOG(ERROR) << "Received Redirect from Backend "
             << " redirectUrl: "
             << redirect_info.new_url.possibly_invalid_spec().c_str()
             << " RespCode " << request->GetResponseCode();
}

void QuicHttpProxyBackendStream::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(request, url_request_.get());
  DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  // Continue the SSL handshake without a client certificate.
  request->ContinueWithCertificate(nullptr, nullptr);
}

void QuicHttpProxyBackendStream::OnSSLCertificateError(
    net::URLRequest* request,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  request->Cancel();
  OnResponseCompleted();
}

void QuicHttpProxyBackendStream::OnResponseStarted(net::URLRequest* request,
                                                   int net_error) {
  DCHECK_EQ(request, url_request_.get());
  DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  if (net_error != net::OK) {
    LOG(ERROR) << "OnResponseStarted Error from Backend "
               << " url: "
               << url_request_->url().possibly_invalid_spec().c_str()
               << " RespError " << net::ErrorToString(net_error);
    OnResponseCompleted();
    return;
  }
  // Initialite the first read
  ReadOnceTask();
}

void QuicHttpProxyBackendStream::ReadOnceTask() {
  // Initiate a read for a max of kBufferSize
  // This avoids a request with a large response from starving
  // requests with smaller responses
  int bytes_read = url_request_->Read(buf_.get(), kBufferSize);
  OnReadCompleted(url_request_.get(), bytes_read);
}

// In the case of net::ERR_IO_PENDING,
// OnReadCompleted callback will be called by URLRequest
void QuicHttpProxyBackendStream::OnReadCompleted(net::URLRequest* unused,
                                                 int bytes_read) {
  DCHECK_EQ(url_request_.get(), unused);
  LOG(INFO) << "OnReadCompleted Backend with"
            << " RespCode " << url_request_->GetResponseCode()
            << " RcvdBytesCount " << bytes_read << " RcvdTotalBytes "
            << data_received_.size();

  if (bytes_read > 0) {
    data_received_.append(buf_->data(), bytes_read);
    // More data to be read, send a task to self
    quic_proxy_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuicHttpProxyBackendStream::ReadOnceTask,
                                  weak_factory_.GetWeakPtr()));
  } else if (bytes_read != net::ERR_IO_PENDING) {
    quic_response_->set_response_type(
        quic::QuicBackendResponse::REGULAR_RESPONSE);
    OnResponseCompleted();
  }
}

/* Response from Backend complete, send the last chunk of data with fin=true to
 * the corresponding quic stream */
void QuicHttpProxyBackendStream::OnResponseCompleted() {
  DCHECK(!response_completed_);
  LOG(INFO) << "Quic Proxy Received Response from Backend for quic_conn_id: "
            << quic_connection_id_ << " quic_stream_id: " << quic_stream_id_
            << " url: " << url_;

  // ToDo Stream the response
  spdy::SpdyHeaderBlock response_headers;
  if (quic_response_->response_type() !=
      quic::QuicBackendResponse::BACKEND_ERR_RESPONSE) {
    response_headers = getAsQuicHeaders(url_request_->response_headers(),
                                        url_request_->GetResponseCode(),
                                        data_received_.size());
    quic_response_->set_headers(std::move(response_headers));
    quic_response_->set_body(std::move(data_received_));
  } else {
    response_headers =
        getAsQuicHeaders(url_request_->response_headers(),
                         kProxyHttpBackendError, data_received_.size());
    quic_response_->set_headers(std::move(response_headers));
  }
  response_completed_ = true;
  ReleaseRequest();

  // Send the response back to the quic client on the quic/main thread
  if (delegate_ != nullptr) {
    delegate_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &QuicHttpProxyBackendStream::SendResponseOnDelegateThread,
            base::Unretained(this)));
  }
}

void QuicHttpProxyBackendStream::SendResponseOnDelegateThread() {
  DCHECK(delegate_ != nullptr);
  // Proxy currently does not support push resources
  std::list<quic::QuicBackendResponse::ServerPushInfo> empty_resources;
  delegate_->OnResponseBackendComplete(quic_response_.get(), empty_resources);
}

void QuicHttpProxyBackendStream::CancelRequest() {
  DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  if (quic_proxy_task_runner_.get())
    DCHECK(quic_proxy_task_runner_->BelongsToCurrentThread());
  delegate_ = nullptr;
  if (url_request_.get()) {
    url_request_->CancelWithError(ERR_ABORTED);
    ReleaseRequest();
  }
}

void QuicHttpProxyBackendStream::ReleaseRequest() {
  url_request_.reset();
  buf_ = nullptr;
}

quic::QuicBackendResponse* QuicHttpProxyBackendStream::GetBackendResponse()
    const {
  return quic_response_.get();
}

// Copy Backend Response headers to Quic response headers
spdy::SpdyHeaderBlock QuicHttpProxyBackendStream::getAsQuicHeaders(
    net::HttpResponseHeaders* resp_headers,
    int response_code,
    uint64_t response_decoded_body_size) {
  DCHECK(!headers_set_);
  bool response_body_encoded = false;
  spdy::SpdyHeaderBlock quic_response_headers;
  // Add spdy headers: Status, version need : before the header
  quic_response_headers[":status"] = base::NumberToString(response_code);
  headers_set_ = true;
  // Returns an empty array if |headers| is nullptr.
  if (resp_headers != nullptr) {
    size_t iter = 0;
    std::string header_name;
    std::string header_value;
    while (resp_headers->EnumerateHeaderLines(&iter, &header_name,
                                              &header_value)) {
      header_name = base::ToLowerASCII(header_name);
      // Do not copy status again since status needs a ":" before the header
      // name
      if (header_name.compare("status") != 0) {
        if (header_name.compare("content-encoding") != 0) {
          // Remove hop-by-hop headers
          if (base::Contains(kHopHeaders, header_name)) {
            LOG(INFO) << "Quic Proxy Ignoring Hop-by-hop Response Header: "
                      << header_name << ":" << header_value;
          } else {
            LOG(INFO) << " Quic Proxy Copying Response Header: " << header_name
                      << ":" << header_value;
            quic_response_headers.AppendValueOrAddHeader(header_name,
                                                         header_value);
          }
        } else {
          response_body_encoded = true;
        }
      }
    }  // while
    // Currently URLRequest class does not support ability to disable decoding,
    // response body (gzip, deflate etc. )
    // Instead of re-encoding the body, we send decode body to the quic client
    // and re-write the content length to the original body size
    if (response_body_encoded) {
      LOG(INFO) << " Quic Proxy Rewriting the Content-Length Header since "
                   "the response was encoded : "
                << response_decoded_body_size;
      quic_response_headers["content-length"] =
          base::NumberToString(response_decoded_body_size);
    }
  }
  return quic_response_headers;
}
}  // namespace net
