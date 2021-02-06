// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_NETWORK_SENDER_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_NETWORK_SENDER_IMPL_H_

#include <stdint.h>
#include <list>
#include <memory>

#include "base/callback.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_reporter_impl.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {

class StoragePartition;

// Implemented a NetworkSender capable of issuing POST requests for complete
// conversions. Maintains a set of all ongoing UrlLoaders used for posting
// conversion reports. Created and owned by ConversionReporterImpl.
class CONTENT_EXPORT ConversionNetworkSenderImpl
    : public ConversionReporterImpl::NetworkSender {
 public:
  explicit ConversionNetworkSenderImpl(StoragePartition* storage_partition);
  ConversionNetworkSenderImpl(const ConversionNetworkSenderImpl&) = delete;
  ConversionNetworkSenderImpl& operator=(const ConversionNetworkSenderImpl&) =
      delete;
  ~ConversionNetworkSenderImpl() override;

  // Generates a resource request for |report| and creates a new UrlLoader to
  // send it. A report is only attempted to be sent once, with a timeout of 30
  // seconds. |report| is destroyed after this call finishes.
  // |sent_callback| is run after the request finishes, whether or not it
  // succeeded,
  void SendReport(ConversionReport* report,
                  ReportSentCallback sent_callback) override;

  // Tests inject a TestURLLoaderFactory so they can mock the network response.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Called when headers are available for a sent report.
  void OnReportSent(UrlLoaderList::iterator it,
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

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_NETWORK_SENDER_IMPL_H_
