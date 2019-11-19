// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_LAUNCHER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_LAUNCHER_H_

#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/core/payment_currency_amount.h"
#include "ios/chrome/browser/payments/payment_request.h"

namespace web {
class NavigationItem;
}  // namespace web

namespace payments {

class PaymentDetails;

// Launches a native iOS third party payment app and handles the response
// returned from that payment app. Only one instance of this object can exist
// per browser state. This launcher can only handle one request at a time,
// so any calls this class while another request is processing will fail and
// will cause the in-flight request to fail.
class IOSPaymentInstrumentLauncher : public KeyedService {
 public:
  IOSPaymentInstrumentLauncher();
  ~IOSPaymentInstrumentLauncher() override;

  // Attempts to launch a third party iOS payment app. Uses |payment_request|
  // and |acitive_web_state| to build numerous parameters that get seraliazed
  // into a JSON string and then encoded into base-64. |universal_link| is then
  // invoked with the built parameters passed in as a query string. If the class
  // fails to open the universal link the error callback of |delegate| will
  // be invoked. If the class is successful in opening the universal link
  // the success callback will be invoked when the payment app calls back into
  // Chrome with a payment response. Finally, the class returns a boolean
  // indicating if it made an attempt to launch the IOSPaymentInstrument. The
  // only instance when the launcher will not attempt a launch is when there is
  // another in-flight request already happening.
  bool LaunchIOSPaymentInstrument(payments::PaymentRequest* payment_request,
                                  web::WebState* active_web_state,
                                  GURL& universal_link,
                                  payments::PaymentApp::Delegate* delegate);

  // Callback for when an iOS payment app sends a response back to Chrome.
  // |response| is a base-64 encodeded string. When decoded, |response| is
  // is expected to contain the method name of the payment instrument used,
  // whether or not the payment app was able to successfully complete its part
  // of the transaction, and details that contain information for the merchant
  // website to complete the transaction. The details are only parsed if
  // the payment app claims to have successfully completed its part of the
  // transaction.
  void ReceiveResponseFromIOSPaymentInstrument(
      const std::string& base_64_response);

  // Before invoking ReceieveResponseFromIOSPaymentInstrument, callers can
  // use delegate() to ensure that the delegate property is valid.
  payments::PaymentApp::Delegate* delegate() { return delegate_; }

  // Sets the delegate for the current IOSPaymentInstrumentLauncher request.
  void set_delegate(payments::PaymentApp::Delegate* delegate) {
    delegate_ = delegate;
  }

  // The payment request ID is exposed in order validate responses from
  // third party payment apps.
  std::string payment_request_id() { return payment_request_id_; }

 private:
  friend class PaymentRequestIOSPaymentInstrumentLauncherTest;

  // Returns the JSON-serialized dictionary from each method name the merchant
  // requested to the corresponding method data. |stringified_method_data| is
  // a mapping of the payment method names to the corresponding JSON-stringified
  // payment method specific data. This function converts that map into a JSON
  // readable object.
  base::Value SerializeMethodData(
      const std::map<std::string, std::set<std::string>>&
          stringified_method_data);

  // Returns the JSON-serialized top-level certificate chain of the browsing
  // context. |item| has information on the browsing state, including the
  // SSL certificate needed to build the certificate chain.
  base::Value SerializeCertificateChain(web::NavigationItem* item);

  // Returns the JSON-serialized array of PaymentDetailsModifier objects.
  // |details| is the object that represents the details of a PaymentRequest
  // object and contains the vector of PaymentDetailsModifier objects to
  // serialize.
  base::Value SerializeModifiers(PaymentDetails details);

  // Invokes the payment instrument delegate with the appropriate function.
  // If |method_name| or |details| are empty then |delegate_| calls
  // OnInstrumentDetailsError, otherwise |delegate_| calls
  // OnInstrumentDetailsReady. After invoking the delegate function this method
  // will also reset |delegate_| and |payment_request_id_|.
  void CompleteLaunchRequest(const std::string& method_name,
                             const std::string& details);

  payments::PaymentApp::Delegate* delegate_;
  std::string payment_request_id_;
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_LAUNCHER_H_
