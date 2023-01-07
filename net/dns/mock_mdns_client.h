// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_MDNS_CLIENT_H_
#define NET_DNS_MOCK_MDNS_CLIENT_H_

#include <memory>
#include <string>

#include "net/dns/mdns_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class MockMDnsTransaction : public MDnsTransaction {
 public:
  MockMDnsTransaction();
  ~MockMDnsTransaction() override;

  MOCK_METHOD0(Start, bool());
  MOCK_CONST_METHOD0(GetName, const std::string&());
  MOCK_CONST_METHOD0(GetType, uint16_t());
};

class MockMDnsClient : public MDnsClient {
 public:
  MockMDnsClient();
  ~MockMDnsClient() override;

  MOCK_METHOD3(CreateListener,
               std::unique_ptr<MDnsListener>(uint16_t,
                                             const std::string&,
                                             MDnsListener::Delegate*));
  MOCK_METHOD4(
      CreateTransaction,
      std::unique_ptr<MDnsTransaction>(uint16_t,
                                       const std::string&,
                                       int,
                                       const MDnsTransaction::ResultCallback&));
  MOCK_METHOD1(StartListening, int(MDnsSocketFactory*));
  MOCK_METHOD0(StopListening, void());
  MOCK_CONST_METHOD0(IsListening, bool());
};

}  // namespace net

#endif  // NET_DNS_MOCK_MDNS_CLIENT_H_
