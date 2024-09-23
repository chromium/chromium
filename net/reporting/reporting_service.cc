// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/http/structured_headers.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_delivery_agent.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_uploader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

constexpr int kMaxJsonSize = 16 * 1024;
constexpr int kMaxJsonDepth = 5;

// If constructed with a PersistentReportingStore, the first call to any of
// QueueReport(), ProcessHeader(), RemoveBrowsingData(), or
// RemoveAllBrowsingData() on a valid input will trigger a load from the store.
// Tasks are queued pending completion of loading from the store.
class ReportingServiceImpl : public ReportingService {
 public:
  explicit ReportingServiceImpl(std::unique_ptr<ReportingContext> context)
      : context_(std::move(context)) {
    if (!context_->IsClientDataPersisted())
      initialized_ = true;
  }

  ReportingServiceImpl(const ReportingServiceImpl&) = delete;
  ReportingServiceImpl& operator=(const ReportingServiceImpl&) = delete;

  // ReportingService implementation:

  ~ReportingServiceImpl() override {
    if (initialized_)
      context_->cache()->Flush();
  }

  void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) override {
    DCHECK(!reporting_source.is_empty());
    DoOrBacklogTask(
        base::BindOnce(&ReportingServiceImpl::DoSetDocumentReportingEndpoints,
                       base::Unretained(this), reporting_source, isolation_info,
                       FixupNetworkAnonymizationKey(
                           isolation_info.network_anonymization_key()),
                       origin, std::move(endpoints)));
  }

  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override {
    if (!base::FeatureList::IsEnabled(
            net::features::kReportingApiEnableEnterpriseCookieIssues)) {
      return;
    }
    context_->cache()->SetEnterpriseReportingEndpoints(endpoints);
  }

  void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) override {
    DCHECK(!reporting_source.is_empty());
    context_->delivery_agent()->SendReportsForSource(reporting_source);
    context_->cache()->SetExpiredSource(reporting_source);
  }

  void QueueReport(
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      base::Value::Dict body,
      int depth,
      ReportingTargetType target_type) override {
    DCHECK(context_);
    DCHECK(context_->delegate());
    // If |reporting_source| is provided, it must not be empty.
    DCHECK(!(reporting_source.has_value() && reporting_source->is_empty()));

    if (!context_->delegate()->CanQueueReport(url::Origin::Create(url)))
      return;

    // Strip username, password, and ref fragment from the URL.
    GURL sanitized_url = url.GetAsReferrer();
    if (!sanitized_url.is_valid())
      return;

    base::TimeTicks queued_ticks = context_->tick_clock().NowTicks();

    // base::Unretained is safe because the callback is stored in
    // |task_backlog_| which will not outlive |this|.
    DoOrBacklogTask(
        base::BindOnce(&ReportingServiceImpl::DoQueueReport,
                       base::Unretained(this), reporting_source,
                       FixupNetworkAnonymizationKey(network_anonymization_key),
                       std::move(sanitized_url), user_agent, group, type,
                       std::move(body), depth, queued_ticks, target_type));
  }

  void ProcessReportToHeader(
      const url::Origin& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& header_string) override {
    if (header_string.size() > kMaxJsonSize)
      return;

    std::optional<base::Value> header_value = base::JSONReader::Read(
        "[" + header_string + "]", base::JSON_PARSE_RFC, kMaxJsonDepth);
    if (!header_value)
      return;

    DVLOG(1) << "Received Reporting policy for " << origin;
    DoOrBacklogTask(base::BindOnce(
        &ReportingServiceImpl::DoProcessReportToHeader, base::Unretained(this),
        FixupNetworkAnonymizationKey(network_anonymization_key), origin,
        std::move(header_value).value()));
  }

  void RemoveBrowsingData(
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter)
      override {
    DoOrBacklogTask(base::BindOnce(&ReportingServiceImpl::DoRemoveBrowsingData,
                                   base::Unretained(this), data_type_mask,
                                   origin_filter));
  }

  void RemoveAllBrowsingData(uint64_t data_type_mask) override {
    DoOrBacklogTask(
        base::BindOnce(&ReportingServiceImpl::DoRemoveAllBrowsingData,
                       base::Unretained(this), data_type_mask));
  }

  void OnShutdown() override {
    shut_down_ = true;
    context_->OnShutdown();
  }

  const ReportingPolicy& GetPolicy() const override {
    return context_->policy();
  }

  base::Value StatusAsValue() const override {
    base::Value::Dict dict;
    dict.Set("reportingEnabled", true);
    dict.Set("clients", context_->cache()->GetClientsAsValue());
    dict.Set("reports", context_->cache()->GetReportsAsValue());
    return base::Value(std::move(dict));
  }

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> GetReports()
      const override {
    std::vector<raw_ptr<const net::ReportingReport, VectorExperimental>>
        reports;
    context_->cache()->GetReports(&reports);
    return reports;
  }

  base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const override {
    return context_->cache()->GetV1ReportingEndpointsByOrigin();
  }

  void AddReportingCacheObserver(ReportingCacheObserver* observer) override {
    context_->AddCacheObserver(observer);
  }

  void RemoveReportingCacheObserver(ReportingCacheObserver* observer) override {
    context_->RemoveCacheObserver(observer);
  }

  ReportingContext* GetContextForTesting() const override {
    return context_.get();
  }

 private:
  void DoOrBacklogTask(base::OnceClosure task) {
    if (shut_down_)
      return;

    FetchAllClientsFromStoreIfNecessary();

    if (!initialized_) {
      task_backlog_.push_back(std::move(task));
      return;
    }

    std::move(task).Run();
  }

  void DoQueueReport(
      const std::optional<base::UnguessableToken>& reporting_source,
      const NetworkAnonymizationKey& network_anonymization_key,
      GURL sanitized_url,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      base::Value::Dict body,
      int depth,
      base::TimeTicks queued_ticks,
      ReportingTargetType target_type) {
    DCHECK(initialized_);
    context_->cache()->AddReport(reporting_source, network_anonymization_key,
                                 sanitized_url, user_agent, group, type,
                                 std::move(body), depth, queued_ticks,
                                 0 /* attempts */, target_type);
  }

  void DoProcessReportToHeader(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const base::Value& header_value) {
    DCHECK(initialized_);
    DCHECK(header_value.is_list());
    ReportingHeaderParser::ParseReportToHeader(context_.get(),
                                               network_anonymization_key,
                                               origin, header_value.GetList());
  }

  void DoSetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const IsolationInfo& isolation_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      base::flat_map<std::string, std::string> header_value) {
    DCHECK(initialized_);
    ReportingHeaderParser::ProcessParsedReportingEndpointsHeader(
        context_.get(), reporting_source, isolation_info,
        network_anonymization_key, origin, std::move(header_value));
  }

  void DoRemoveBrowsingData(
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {
    DCHECK(initialized_);
    ReportingBrowsingDataRemover::RemoveBrowsingData(
        context_->cache(), data_type_mask, origin_filter);
  }

  void DoRemoveAllBrowsingData(uint64_t data_type_mask) {
    DCHECK(initialized_);
    ReportingBrowsingDataRemover::RemoveAllBrowsingData(context_->cache(),
                                                        data_type_mask);
  }

  void ExecuteBacklog() {
    DCHECK(initialized_);
    DCHECK(context_);

    if (shut_down_)
      return;

    for (base::OnceClosure& task : task_backlog_) {
      std::move(task).Run();
    }
    task_backlog_.clear();
  }

  void FetchAllClientsFromStoreIfNecessary() {
    if (!context_->IsClientDataPersisted() || started_loading_from_store_)
      return;

    started_loading_from_store_ = true;
    FetchAllClientsFromStore();
  }

  void FetchAllClientsFromStore() {
    DCHECK(context_->IsClientDataPersisted());
    DCHECK(!initialized_);

    context_->store()->LoadReportingClients(base::BindOnce(
        &ReportingServiceImpl::OnClientsLoaded, weak_factory_.GetWeakPtr()));
  }

  void OnClientsLoaded(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups) {
    initialized_ = true;
    context_->cache()->AddClientsLoadedFromStore(
        std::move(loaded_endpoints), std::move(loaded_endpoint_groups));
    ExecuteBacklog();
  }

  // Returns either |network_anonymization_key| or an empty
  // NetworkAnonymizationKey, based on |respect_network_anonymization_key_|.
  // Should be used on all NetworkAnonymizationKeys passed in through public API
  // calls.
  const NetworkAnonymizationKey& FixupNetworkAnonymizationKey(
      const NetworkAnonymizationKey& network_anonymization_key) {
    if (respect_network_anonymization_key_)
      return network_anonymization_key;
    return empty_nak_;
  }

  std::unique_ptr<ReportingContext> context_;
  bool shut_down_ = false;
  bool started_loading_from_store_ = false;
  bool initialized_ = false;
  std::vector<base::OnceClosure> task_backlog_;

  bool respect_network_anonymization_key_ =
      NetworkAnonymizationKey::IsPartitioningEnabled();

  // Allows returning a NetworkAnonymizationKey by reference when
  // |respect_network_anonymization_key_| is false.
  NetworkAnonymizationKey empty_nak_;

  base::WeakPtrFactory<ReportingServiceImpl> weak_factory_{this};
};

}  // namespace

ReportingService::~ReportingService() = default;

// static
std::unique_ptr<ReportingService> ReportingService::Create(
    const ReportingPolicy& policy,
    URLRequestContext* request_context,
    ReportingCache::PersistentReportingStore* store,
    const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints) {
  return std::make_unique<ReportingServiceImpl>(ReportingContext::Create(
      policy, request_context, store, enterprise_reporting_endpoints));
}

// static
std::unique_ptr<ReportingService> ReportingService::CreateForTesting(
    std::unique_ptr<ReportingContext> reporting_context) {
  return std::make_unique<ReportingServiceImpl>(std::move(reporting_context));
}

base::Value ReportingService::StatusAsValue() const {
  NOTIMPLEMENTED();
  return base::Value();
}

}  // namespace net
