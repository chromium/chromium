// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_network_change_observer.h"

#include "base/memory/raw_ptr.h"
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
  explicit ReportingNetworkChangeObserverImpl(ReportingContext* context)
      : context_(context) {
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  ReportingNetworkChangeObserverImpl(
      const ReportingNetworkChangeObserverImpl&) = delete;
  ReportingNetworkChangeObserverImpl& operator=(
      const ReportingNetworkChangeObserverImpl&) = delete;

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
      context_->cache()->RemoveAllReports();

    if (!context_->policy().persist_clients_across_network_changes)
      context_->cache()->RemoveAllClients();
  }

 private:
  raw_ptr<ReportingContext> context_;
};

}  // namespace

// static
std::unique_ptr<ReportingNetworkChangeObserver>
ReportingNetworkChangeObserver::Create(ReportingContext* context) {
  return std::make_unique<ReportingNetworkChangeObserverImpl>(context);
}

ReportingNetworkChangeObserver::~ReportingNetworkChangeObserver() = default;

}  // namespace net
