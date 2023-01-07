// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/network_error_logging/network_error_logging_test_util.h"

#include "base/containers/contains.h"
#include "net/base/ip_address.h"

namespace net {

TestNetworkErrorLoggingService::TestNetworkErrorLoggingService() = default;
TestNetworkErrorLoggingService::~TestNetworkErrorLoggingService() = default;

void TestNetworkErrorLoggingService::OnHeader(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    const IPAddress& received_ip_address,
    const std::string& value) {
  VLOG(1) << "Received NEL policy for " << origin;
  Header header;
  header.network_anonymization_key = network_anonymization_key;
  header.origin = origin;
  header.received_ip_address = received_ip_address;
  header.value = value;
  headers_.push_back(header);
}

void TestNetworkErrorLoggingService::OnRequest(RequestDetails details) {
  VLOG(1) << "Created NEL report (status=" << details.status_code
          << ", depth=" << details.reporting_upload_depth << ") for "
          << details.uri;
  errors_.push_back(std::move(details));
}

void TestNetworkErrorLoggingService::QueueSignedExchangeReport(
    SignedExchangeReportDetails details) {}

void TestNetworkErrorLoggingService::RemoveBrowsingData(
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {}

void TestNetworkErrorLoggingService::RemoveAllBrowsingData() {}

bool TestNetworkErrorLoggingService::Header::MatchesAddressList(
    const AddressList& address_list) const {
  return base::Contains(address_list, received_ip_address,
                        &IPEndPoint::address);
}

}  // namespace net
