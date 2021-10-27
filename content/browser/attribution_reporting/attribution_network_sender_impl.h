// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_IMPL_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "content/browser/attribution_reporting/attribution_reporter_impl.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {

class StoragePartition;

struct AttributionReport;

// Implemented a NetworkSender capable of issuing POST requests for complete
// conversions. Maintains a set of all ongoing UrlLoaders used for posting
// conversion reports. Created and owned by AttributionReporterImpl.
class CONTENT_EXPORT AttributionNetworkSenderImpl
    : public AttributionReporterImpl::NetworkSender {
 public:
  explicit AttributionNetworkSenderImpl(StoragePartition* storage_partition);
  AttributionNetworkSenderImpl(const AttributionNetworkSenderImpl&) = delete;
  AttributionNetworkSenderImpl& operator=(const AttributionNetworkSenderImpl&) =
      delete;
  AttributionNetworkSenderImpl(AttributionNetworkSenderImpl&&) = delete;
  AttributionNetworkSenderImpl& operator=(AttributionNetworkSenderImpl&&) =
      delete;
  ~AttributionNetworkSenderImpl() override;

  // Generates a resource request for |report| and creates a new UrlLoader to
  // send it. A report is only attempted to be sent once, with a timeout of 30
  // seconds.
  // |sent_callback| is run after the request finishes, whether or not it
  // succeeded,
  void SendReport(AttributionReport report,
                  ReportSentCallback sent_callback) override;

  // Tests inject a TestURLLoaderFactory so they can mock the network response.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Called when headers are available for a sent report.
  void OnReportSent(UrlLoaderList::iterator it,
                    AttributionReport report,
                    ReportSentCallback sent_callback,
                    scoped_refptr<net::HttpResponseHeaders> headers);

  // Reports that are actively being sent.
  UrlLoaderList loaders_in_progress_;

  // Must outlive |this|.
  StoragePartition* storage_partition_;

  // Lazily accessed URLLoaderFactory used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_IMPL_H_
