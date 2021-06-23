// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delegate.h"

#include "net/base/network_delegate.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {

class ReportingDelegateImpl : public ReportingDelegate {
 public:
  ReportingDelegateImpl(URLRequestContext* request_context)
      : request_context_(request_context) {
    DCHECK(request_context);
  }

  ~ReportingDelegateImpl() override = default;

  bool CanQueueReport(const url::Origin& origin) const override {
    return network_delegate() &&
           network_delegate()->CanQueueReportingReport(origin);
  }

  void CanSendReports(std::set<url::Origin> origins,
                      base::OnceCallback<void(std::set<url::Origin>)>
                          result_callback) const override {
    if (!network_delegate()) {
      origins.clear();
      std::move(result_callback).Run(std::move(origins));
      return;
    }
    network_delegate()->CanSendReportingReports(std::move(origins),
                                                std::move(result_callback));
  }

  bool CanSetClient(const url::Origin& origin,
                    const GURL& endpoint) const override {
    return network_delegate() &&
           network_delegate()->CanSetReportingClient(origin, endpoint);
  }

  bool CanUseClient(const url::Origin& origin,
                    const GURL& endpoint) const override {
    return network_delegate() &&
           network_delegate()->CanUseReportingClient(origin, endpoint);
  }

 private:
  const NetworkDelegate* network_delegate() const {
    return request_context_->network_delegate();
  }

  URLRequestContext* request_context_;
};

}  // namespace

// static
std::unique_ptr<ReportingDelegate> ReportingDelegate::Create(
    URLRequestContext* request_context) {
  return std::make_unique<ReportingDelegateImpl>(request_context);
}

ReportingDelegate::~ReportingDelegate() = default;

}  // namespace net
