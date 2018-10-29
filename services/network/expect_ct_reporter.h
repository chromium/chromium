// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_EXPECT_CT_REPORTER_H_
#define SERVICES_NETWORK_EXPECT_CT_REPORTER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request.h"

namespace net {
class ReportSender;
class URLRequestContext;
}  // namespace net

namespace network {

// This class monitors for violations of CT policy and sends reports
// about failures for sites that have opted in. Must be deleted before
// the URLRequestContext that is passed to the constructor, so that it
// can cancel its requests.
//
// Since reports are sent with a non-CORS-whitelisted Content-Type, this class
// sends CORS preflight requests before sending reports. Expect-CT is not
// evaluated with a particular frame or request as context, so the preflight
// request contains an `Origin: null` header instead of a particular origin.
class COMPONENT_EXPORT(NETWORK_SERVICE) ExpectCTReporter
    : public net::TransportSecurityState::ExpectCTReporter,
      net::URLRequest::Delegate {
 public:
  // Constructs a ExpectCTReporter that sends reports with the given
  // |request_context|. |success_callback| is called whenever a report sends
  // successfully, and |failure_callback| whenever a report fails to send.
  ExpectCTReporter(net::URLRequestContext* request_context,
                   const base::Closure& success_callback,
                   const base::Closure& failure_callback);
  ~ExpectCTReporter() override;

  // net::TransportSecurityState::ExpectCTReporter:
  void OnExpectCTFailed(const net::HostPortPair& host_port_pair,
                        const GURL& report_uri,
                        base::Time expiration,
                        const net::X509Certificate* validated_certificate_chain,
                        const net::X509Certificate* served_certificate_chain,
                        const net::SignedCertificateTimestampAndStatusList&
                            signed_certificate_timestamps) override;

  // net::URLRequest::Delegate:
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

 private:
  // Used to keep track of in-flight CORS preflight requests. When |request|
  // completes successfully and the CORS check passes, |serialized_report| will
  // be sent to |report_uri| using |report_sender_|.
  struct PreflightInProgress {
    PreflightInProgress(std::unique_ptr<net::URLRequest> request,
                        const std::string& serialized_report,
                        const GURL& report_uri);
    ~PreflightInProgress();
    // The preflight request.
    const std::unique_ptr<net::URLRequest> request;
    // |serialized_report| should be sent to |report_uri| if the preflight
    // succeeds.
    const std::string serialized_report;
    const GURL report_uri;
  };

  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest, FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest, EmptyReportURI);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest, SendReport);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest, PreflightContainsWhitespace);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest,
                           BadCORSPreflightResponseOrigin);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest,
                           BadCORSPreflightResponseMethods);
  FRIEND_TEST_ALL_PREFIXES(ExpectCTReporterTest,
                           BadCORSPreflightResponseHeaders);

  // Starts a CORS preflight request to obtain permission from the server to
  // send a report with Content-Type: application/expect-ct-report+json. The
  // preflight result is checked in OnResponseStarted(), and an actual report is
  // sent with |report_sender_| if the preflight succeeds.
  void SendPreflight(const GURL& report_uri,
                     const std::string& serialized_report);

  // When a report fails to send, this method records an UMA histogram and calls
  // |failure_callback_|.
  void OnReportFailure(const GURL& report_uri,
                       int net_error,
                       int http_response_code);

  std::unique_ptr<net::ReportSender> report_sender_;

  net::URLRequestContext* request_context_;

  base::Closure success_callback_;
  base::Closure failure_callback_;

  // The CORS preflight requests, with corresponding report information, that
  // are currently in-flight. Entries in this map are deleted when the
  // preflight's OnResponseStarted() is called.
  std::map<net::URLRequest*, std::unique_ptr<PreflightInProgress>>
      inflight_preflights_;

  DISALLOW_COPY_AND_ASSIGN(ExpectCTReporter);
};

}  // namespace network

#endif  // SERVICES_NETWORK_EXPECT_CT_REPORTER_H_
