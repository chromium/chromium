// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_
#define NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "url/origin.h"

namespace net {

class IPAddress;

// A NetworkErrorLoggingService implementation that stashes all NEL headers and
// reports so that they can be easily verified in unit tests.
class TestNetworkErrorLoggingService : public NetworkErrorLoggingService {
 public:
  struct Header {
    Header() = default;
    ~Header() = default;

    // Returns whether the |received_ip_address| field matches any of the
    // addresses in |address_list|.
    bool MatchesAddressList(const AddressList& address_list) const;

    NetworkAnonymizationKey network_anonymization_key;
    url::Origin origin;
    IPAddress received_ip_address;
    std::string value;
  };

  TestNetworkErrorLoggingService();

  TestNetworkErrorLoggingService(const TestNetworkErrorLoggingService&) =
      delete;
  TestNetworkErrorLoggingService& operator=(
      const TestNetworkErrorLoggingService&) = delete;

  ~TestNetworkErrorLoggingService() override;

  const std::vector<Header>& headers() { return headers_; }
  const std::vector<RequestDetails>& errors() { return errors_; }

  // NetworkErrorLoggingService implementation
  void OnHeader(const NetworkAnonymizationKey& network_anonymization_key,
                const url::Origin& origin,
                const IPAddress& received_ip_address,
                const std::string& value) override;
  void OnRequest(RequestDetails details) override;
  void QueueSignedExchangeReport(SignedExchangeReportDetails details) override;
  void RemoveBrowsingData(
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter)
      override;
  void RemoveAllBrowsingData() override;

 private:
  std::vector<Header> headers_;
  std::vector<RequestDetails> errors_;
};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_
