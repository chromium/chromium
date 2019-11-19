// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_CACHE_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_CACHE_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/payments/payment_request.h"

namespace web {
class WebState;
}

namespace payments {

// Maintains a map of web::WebState to a list of payments::PaymentRequest
// instances maintained for that web state.
class PaymentRequestCache : public KeyedService {
 public:
  typedef std::set<std::unique_ptr<payments::PaymentRequest>,
                   payments::PaymentRequest::Compare>
      PaymentRequestSet;

  PaymentRequestCache();
  ~PaymentRequestCache() override;

  // Adds |payment_request| to the cache for |web_state| and returns a pointer
  // to the payments::PaymentRequest instance wrapped by |payment_request|.
  payments::PaymentRequest* AddPaymentRequest(
      web::WebState* web_state,
      std::unique_ptr<payments::PaymentRequest> payment_request);

  // Returns the payments::PaymentRequest instances maintained for |web_state|.
  PaymentRequestCache::PaymentRequestSet& GetPaymentRequests(
      web::WebState* web_state);

  // Clears the payments::PaymentRequest instances maintained for |web_state|.
  void ClearPaymentRequests(web::WebState* web_state);

 private:
  std::unordered_map<web::WebState*, PaymentRequestSet> payment_requests_;
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_CACHE_H_
