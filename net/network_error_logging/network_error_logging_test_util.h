// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_
#define NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "url/gurl.h"
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

    url::Origin origin;
    IPAddress received_ip_address;
    std::string value;
  };

  TestNetworkErrorLoggingService();
  ~TestNetworkErrorLoggingService() override;

  const std::vector<Header>& headers() { return headers_; }
  const std::vector<RequestDetails>& errors() { return errors_; }

  // NetworkErrorLoggingService implementation
  void OnHeader(const url::Origin& origin,
                const IPAddress& received_ip_address,
                const std::string& value) override;
  void OnRequest(RequestDetails details) override;
  void QueueSignedExchangeReport(SignedExchangeReportDetails details) override;
  void RemoveBrowsingData(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;
  void RemoveAllBrowsingData() override;

 private:
  std::vector<Header> headers_;
  std::vector<RequestDetails> errors_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkErrorLoggingService);
};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_TEST_UTIL_H_
