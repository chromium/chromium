// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the Financial Server ping.

#ifndef RLZ_LIB_FINANCIAL_PING_H_
#define RLZ_LIB_FINANCIAL_PING_H_

#include <string>
#include "rlz/lib/rlz_enums.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace rlz_lib {

class FinancialPing {
 public:
  // Return values of the PingServer() method.
  enum PingResponse {
    PING_SUCCESSFUL,  // Ping sent and response processed successfully.
    PING_FAILURE,     // Ping not sent.
    PING_SHUTDOWN     // Ping not sent because chrome is shutting down.
  };

  // Form the HTTP request to send to the PSO server.
  // Will look something like:
  // /pso/ping?as=swg&brand=GGLD&id=124&hl=en&
  //           events=I7S&rep=1&rlz=I7:val,W1:&dcc=dval
  static bool FormRequest(Product product, const AccessPoint* access_points,
                          const char* product_signature,
                          const char* product_brand, const char* product_id,
                          const char* product_lang, bool exclude_machine_id,
                          std::string* request);

  // Returns whether the time is right to send a ping.
  // If no_delay is true, this should always ping if there are events,
  // or one week has passed since last_ping when there are no new events.
  // If no_delay is false, this should ping if current time < last_ping time
  // (case of time reset) or if one day has passed since last_ping and there
  // are events, or one week has passed since last_ping when there are
  // no new events.
  static bool IsPingTime(Product product, bool no_delay);

  // Set the last ping time to be now. Writes to RlzValueStore.
  static bool UpdateLastPingTime(Product product);

  // Clear the last ping time - should be called on uninstall.
  // Writes to RlzValueStore.
  static bool ClearLastPingTime(Product product);

  // Ping the financial server with request. Writes to RlzValueStore.
  static PingResponse PingServer(const char* request, std::string* response);

  static bool SetURLLoaderFactory(network::mojom::URLLoaderFactory* factory);

 private:
  FinancialPing() {}
  ~FinancialPing() {}
};

namespace test {
void ResetSendFinancialPingInterrupted();
bool WasSendFinancialPingInterrupted();
}  // namespace test

}  // namespace rlz_lib


#endif  // RLZ_LIB_FINANCIAL_PING_H_
