// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_network_change_observer.h"

#include "base/macros.h"
#include "net/base/network_change_notifier.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"

namespace net {

namespace {

class ReportingNetworkChangeObserverImpl
    : public ReportingNetworkChangeObserver,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  ReportingNetworkChangeObserverImpl(ReportingContext* context)
      : context_(context) {
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  // ReportingNetworkChangeObserver implementation:
  ~ReportingNetworkChangeObserverImpl() override {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override {
    // Every network change will be preceded by a call with CONNECTION_NONE, and
    // NetworkChangeNotifier suggests that destructive actions be performed in
    // the call with CONNECTION_NONE, so clear reports and/or clients only in
    // that case.
    if (type != NetworkChangeNotifier::ConnectionType::CONNECTION_NONE)
      return;

    if (!context_->policy().persist_reports_across_network_changes)
      context_->cache()->RemoveAllReports(
          ReportingReport::Outcome::ERASED_NETWORK_CHANGED);

    if (!context_->policy().persist_clients_across_network_changes)
      context_->cache()->RemoveAllClients();
  }

 private:
  ReportingContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ReportingNetworkChangeObserverImpl);
};

}  // namespace

// static
std::unique_ptr<ReportingNetworkChangeObserver>
ReportingNetworkChangeObserver::Create(ReportingContext* context) {
  return std::make_unique<ReportingNetworkChangeObserverImpl>(context);
}

ReportingNetworkChangeObserver::~ReportingNetworkChangeObserver() = default;

}  // namespace net
