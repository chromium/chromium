// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SENDER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SENDER_H_

#include <list>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

class StoragePartition;

// This class is responsible for sending aggregatable reports to the reporting
// endpoints over the network.
class CONTENT_EXPORT AggregatableReportSender {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestStatus {
    kOk = 0,
    // Corresponds to a non-zero NET_ERROR indicating error occurred reading or
    // consuming the response body.
    kNetworkError = 1,
    // Corresponds to a non-200 HTTP response code from the reporting endpoint.
    kServerError = 2,
    kMaxValue = kServerError,
  };

  explicit AggregatableReportSender(StoragePartition* storage_partition);
  AggregatableReportSender(const AggregatableReportSender&) = delete;
  AggregatableReportSender& operator=(const AggregatableReportSender&) = delete;
  virtual ~AggregatableReportSender();

  // Callback used to notify caller that the requested report has been sent.
  using ReportSentCallback = base::OnceCallback<void(RequestStatus)>;

  // Sends an aggregatable report to the reporting endpoint `url`. This should
  // generate a secure POST request with no-credentials. The `delay_type`
  // parameter is only used to select histogram names.
  virtual void SendReport(
      const GURL& url,
      const base::Value& contents,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      ReportSentCallback callback);

  // Used by tests to inject a TestURLLoaderFactory so they can mock the
  // network response. Also used by the aggregation service tool to inject a
  // `url_loader_factory` if one is provided.
  static std::unique_ptr<AggregatableReportSender> CreateForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging = false);

 protected:
  // For testing only.
  explicit AggregatableReportSender(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging = false);

 private:
  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Called when headers are available for a sent report.
  void OnReportSent(
      UrlLoaderList::iterator it,
      ReportSentCallback callback,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Reports that are actively being sent.
  UrlLoaderList loaders_in_progress_;

  // Might be `nullptr` for testing, otherwise must outlive `this`.
  raw_ptr<StoragePartition> storage_partition_;

  // Lazily accessed URLLoaderFactory used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Whether to enable debug logging. Should be false in production.
  bool enable_debug_logging_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SENDER_H_
