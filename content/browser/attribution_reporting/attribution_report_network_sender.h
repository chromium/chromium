// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_NETWORK_SENDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_NETWORK_SENDER_H_

#include <list>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionReport;

// Issues POST requests containing attribution reports. Maintains a set of all
// ongoing UrlLoaders used for posting reports. Created and owned by
// `AttributionManagerImpl`.
class CONTENT_EXPORT AttributionReportNetworkSender
    : public AttributionReportSender {
 public:
  explicit AttributionReportNetworkSender(
      scoped_refptr<network::SharedURLLoaderFactory>);
  AttributionReportNetworkSender(const AttributionReportNetworkSender&) =
      delete;
  AttributionReportNetworkSender& operator=(
      const AttributionReportNetworkSender&) = delete;
  AttributionReportNetworkSender(AttributionReportNetworkSender&&) = delete;
  AttributionReportNetworkSender& operator=(AttributionReportNetworkSender&&) =
      delete;
  ~AttributionReportNetworkSender() override;

  // AttributionReportSender:
  void SendReport(AttributionReport report,
                  bool is_debug_report,
                  ReportSentCallback sent_callback) override;
  void SendReport(AttributionDebugReport report,
                  DebugReportSentCallback) override;

  void SendReport(AggregatableDebugReport,
                  base::Value::Dict report_body,
                  AggregatableDebugReportSentCallback) override;

 private:
  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  using UrlLoaderCallback =
      base::OnceCallback<void(UrlLoaderList::iterator it,
                              scoped_refptr<net::HttpResponseHeaders>)>;

  void SendReport(GURL url,
                  url::Origin origin,
                  const std::string& body,
                  UrlLoaderCallback callback);

  // Called when headers are available for a sent report.
  void OnReportSent(const AttributionReport&,
                    bool is_debug_report,
                    ReportSentCallback sent_callback,
                    UrlLoaderList::iterator it,
                    scoped_refptr<net::HttpResponseHeaders> headers);

  // Called when headers are available for a sent verbose debug report.
  void OnVerboseDebugReportSent(
      base::OnceCallback<void(int status)> callback,
      UrlLoaderList::iterator it,
      scoped_refptr<net::HttpResponseHeaders> headers);

  void OnAggregatableDebugReportSent(
      base::OnceCallback<void(int status)> callback,
      UrlLoaderList::iterator,
      scoped_refptr<net::HttpResponseHeaders>);

  // Reports that are actively being sent.
  UrlLoaderList loaders_in_progress_;

  // Used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_NETWORK_SENDER_H_
