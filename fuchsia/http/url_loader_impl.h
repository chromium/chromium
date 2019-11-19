// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_HTTP_URL_LOADER_IMPL_H_
#define FUCHSIA_HTTP_URL_LOADER_IMPL_H_

#include <fuchsia/net/oldhttp/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/message_loop/message_pump_for_io.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

// URLLoader implementation. This class manages one network request per
// instance. Internally, this class uses a |net::URLRequest| object to handle
// the actual request. The lifespan of |URLLoaderImpl| objects is tied to its
// |binding_|.
// TODO(https://crbug.com/875532): Implement cache-mode.
class URLLoaderImpl : public ::fuchsia::net::oldhttp::URLLoader,
                      public net::URLRequest::Delegate,
                      base::MessagePumpForIO::ZxHandleWatcher {
 public:
  using Callback = ::fuchsia::net::oldhttp::URLLoader::StartCallback;

  URLLoaderImpl(
      std::unique_ptr<net::URLRequestContext> context,
      ::fidl::InterfaceRequest<::fuchsia::net::oldhttp::URLLoader> request);
  ~URLLoaderImpl() override;

  // Returns the number of active requests. Used for testing.
  static int GetNumActiveRequestsForTests();

 private:
  // URLLoader methods:
  void Start(::fuchsia::net::oldhttp::URLRequest request,
             Callback callback) override;
  void FollowRedirect(Callback callback) override;
  void QueryStatus(::fuchsia::net::oldhttp::URLLoader::QueryStatusCallback
                       callback) override;

  // URLRequest::Delegate methods:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      const net::AuthChallengeInfo& auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // MessagePumpForIO::ZxHandleWatcher method:
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override;

  // Reads from the next response buffer available.
  void ReadNextBuffer();

  // If |response_body_mode_| is set to socket mode, |result| bytes in
  // |buffer_| are written to |write_socket_|. In buffer mode, |buffer_| is
  // expanded by |result| bytes.
  // |result| is 0 when all data from the response has been written to
  // |buffer_| and is negative when an error occurred.
  // Returns true if the caller should call |net_request_.Read()| again and
  // false otherwise.
  bool WriteResponseBytes(int result);

  // Helper method to build the response.
  ::fuchsia::net::oldhttp::URLResponse BuildResponse(int net_error);

  // The corresponding binding, this controls the lifetime of the URLLoaderImpl.
  ::fidl::Binding<::fuchsia::net::oldhttp::URLLoader> binding_;

  // Holds the net::URLRequestContext associated with the |net_request_|
  std::unique_ptr<net::URLRequestContext> context_;

  // Holds the net::URLRequest used to perform the network operation.
  std::unique_ptr<net::URLRequest> net_request_;

  // Callback from a Start or FollowRedirect call.
  Callback done_callback_;

  // Populated from the FIDL URLRequest. Indicates whether to let the client
  // manually handle redirects.
  bool auto_follow_redirects_;

  // Populated from the FIDL URLRequest. Indicates how the response body should
  // be populated.
  ::fuchsia::net::oldhttp::ResponseBodyMode response_body_mode_;

  // The resulting net error. Set after the request has completed and
  // |OnResponseStarted| has been called.
  int net_error_;

  // Used to populate the response body.
  scoped_refptr<net::GrowableIOBuffer> buffer_;

  // If |response_body_mode_| is set to socket mode, the server side of the
  // socket. The other end is to be read by the client.
  zx::socket write_socket_;

  // If |response_body_mode_| is set to socket mode, the watch controller used
  // to wait for |write_socket_| to be writable. Unused otherwise.
  base::MessagePumpForIO::ZxHandleWatchController write_watch_;

  // If |response_body_mode_| is set to socket mode and |write_socket_| is not
  // writable, |buffered_bytes_| is used to store the number of bytes waiting
  // in |buffer_|.
  int buffered_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderImpl);
};

#endif  // FUCHSIA_HTTP_URL_LOADER_IMPL_H_
