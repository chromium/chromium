// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_uploader.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr int kMaxJsonSize = 16 * 1024;
constexpr int kMaxJsonDepth = 5;

class ReportingServiceImpl : public ReportingService {
 public:
  ReportingServiceImpl(std::unique_ptr<ReportingContext> context)
      : context_(std::move(context)) {}

  // ReportingService implementation:

  ~ReportingServiceImpl() override = default;

  void QueueReport(const GURL& url,
                   const std::string& user_agent,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body,
                   int depth) override {
    DCHECK(context_);
    DCHECK(context_->delegate());

    if (!context_->delegate()->CanQueueReport(url::Origin::Create(url)))
      return;

    context_->cache()->AddReport(url, user_agent, group, type, std::move(body),
                                 depth, context_->tick_clock()->NowTicks(), 0);
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_string) override {
    if (header_string.size() > kMaxJsonSize) {
      ReportingHeaderParser::RecordHeaderDiscardedForJsonTooBig();
      return;
    }

    std::unique_ptr<base::Value> header_value = base::JSONReader::Read(
        "[" + header_string + "]", base::JSON_PARSE_RFC, kMaxJsonDepth);
    if (!header_value) {
      ReportingHeaderParser::RecordHeaderDiscardedForJsonInvalid();
      return;
    }

    ReportingHeaderParser::ParseHeader(context_.get(), url,
                                       std::move(header_value));
  }

  void RemoveBrowsingData(int data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ReportingBrowsingDataRemover::RemoveBrowsingData(
        context_->cache(), data_type_mask, origin_filter);
  }

  void RemoveAllBrowsingData(int data_type_mask) override {
    ReportingBrowsingDataRemover::RemoveAllBrowsingData(context_->cache(),
                                                        data_type_mask);
  }

  int GetUploadDepth(const URLRequest& request) override {
    return context_->uploader()->GetUploadDepth(request);
  }

  const ReportingPolicy& GetPolicy() const override {
    return context_->policy();
  }

  base::Value StatusAsValue() const override {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("reportingEnabled", base::Value(true));
    dict.SetKey("clients", context_->cache()->GetClientsAsValue());
    dict.SetKey("reports", context_->cache()->GetReportsAsValue());
    return dict;
  }

 private:
  std::unique_ptr<ReportingContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ReportingServiceImpl);
};

}  // namespace

ReportingService::~ReportingService() = default;

// static
std::unique_ptr<ReportingService> ReportingService::Create(
    const ReportingPolicy& policy,
    URLRequestContext* request_context) {
  return std::make_unique<ReportingServiceImpl>(
      ReportingContext::Create(policy, request_context));
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
