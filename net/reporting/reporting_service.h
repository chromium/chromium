// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_SERVICE_H_
#define NET_REPORTING_REPORTING_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_target_type.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace net {

class IsolationInfo;
class NetworkAnonymizationKey;
class ReportingContext;
struct ReportingPolicy;
class URLRequestContext;

// The external interface to the Reporting system, used by the embedder of //net
// and also other parts of //net.
class NET_EXPORT ReportingService {
 public:
  ReportingService(const ReportingService&) = delete;
  ReportingService& operator=(const ReportingService&) = delete;

  virtual ~ReportingService();

  // Creates a ReportingService. |policy| will be copied. |request_context| must
  // outlive the ReportingService. |store| must outlive the ReportingService.
  // If |store| is null, the ReportingCache will be in-memory only.
  static std::unique_ptr<ReportingService> Create(
      const ReportingPolicy& policy,
      URLRequestContext* request_context,
      ReportingCache::PersistentReportingStore* store,
      const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints);

  // Creates a ReportingService for testing purposes using an
  // already-constructed ReportingContext. The ReportingService will take
  // ownership of |reporting_context| and destroy it when the service is
  // destroyed.
  static std::unique_ptr<ReportingService> CreateForTesting(
      std::unique_ptr<ReportingContext> reporting_context);

  // Queues a report for delivery. |url| is the URL that originated the report.
  // |reporting_source| is the reporting source token for the document or
  // worker which triggered this report, if it can be associated with one, or
  // nullopt otherwise. If present, it may not be empty.
  // Along with |network_anonymization_key|, it is used to restrict what reports
  // can be merged, and for sending the report.
  // |user_agent| is the User-Agent header that was used for the request.
  // |group| is the endpoint group to which the report should be delivered.
  // |type| is the type of the report. |body| is the body of the report.
  // |target_type| is used to tag the report as either a web developer report
  // or an enterprise report.
  //
  // The Reporting system will take ownership of |body|; all other parameters
  // will be copied.
  virtual void QueueReport(
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      base::Value::Dict body,
      int depth,
      ReportingTargetType target_type) = 0;

  // Processes a Report-To header. |origin| is the Origin of the URL that the
  // header came from; |header_value| is the normalized value of the Report-To
  // header.
  virtual void ProcessReportToHeader(
      const url::Origin& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& header_value) = 0;

  // Configures reporting endpoints set by the Reporting-Endpoints header, once
  // the associated document or worker (represented by |reporting_source|) has
  // been committed.
  // |reporting_source| is the unique identifier for the resource with which
  // this header was received, and must not be empty.
  // |endpoints| is a mapping of endpoint names to URLs.
  // |origin| is the origin of the reporting source, and
  // |isolation_info| is the appropriate IsolationInfo struct for that source.
  virtual void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) = 0;

  // Configures reporting endpoints set by the ReportingEndpoints enterprise
  // policy.
  // `endpoints` is a mapping of endpoint names to URLs.
  virtual void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) = 0;

  // Attempts to send any queued reports and removes all associated
  // configuration for `reporting_source`. This is called when a source is
  // destroyed.
  virtual void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) = 0;

  // Removes browsing data from the Reporting system. See
  // ReportingBrowsingDataRemover for more details.
  virtual void RemoveBrowsingData(
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const url::Origin&)>&
          origin_filter) = 0;

  // Like RemoveBrowsingData except removes data for all origins without a
  // filter.
  virtual void RemoveAllBrowsingData(uint64_t data_type_mask) = 0;

  // Shuts down the Reporting service so that no new headers or reports are
  // processed, and pending uploads are cancelled.
  virtual void OnShutdown() = 0;

  virtual const ReportingPolicy& GetPolicy() const = 0;

  virtual base::Value StatusAsValue() const;

  virtual std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
  GetReports() const = 0;

  virtual base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const = 0;

  virtual void AddReportingCacheObserver(ReportingCacheObserver* observer) = 0;
  virtual void RemoveReportingCacheObserver(
      ReportingCacheObserver* observer) = 0;

  virtual ReportingContext* GetContextForTesting() const = 0;

 protected:
  ReportingService() = default;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_SERVICE_H_
