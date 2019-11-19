// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/address_normalization_manager.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/web_payment_request.h"
#import "ios/chrome/browser/payments/ios_payment_instrument_finder.h"
#import "ios/chrome/browser/payments/payment_response_helper.h"
#include "url/gurl.h"

namespace autofill {
class AddressNormalizer;
class AutofillProfile;
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

namespace payments {
class AutofillPaymentApp;
class CurrencyFormatter;
class PaymentDetails;
class PaymentDetailsModifier;
class PaymentItem;
class PaymentShippingOption;
}  // namespace payments

namespace ios {
class ChromeBrowserState;
}  // namepsace ios

namespace web {
class WebState;
}  // namespace web

@protocol PaymentRequestUIDelegate<NSObject>

// Called when all payment instruments have been fetched.
- (void)paymentRequestDidFetchPaymentMethods:
    (payments::PaymentRequest*)paymentRequest;

// Called when the credit card unmask UI should be revealed so that the user
// may provide provide card details such as their CVC.
- (void)
       paymentRequest:(payments::PaymentRequest*)paymentRequest
requestFullCreditCard:(const autofill::CreditCard&)creditCard
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               delegate;

// Called when a native iOS payment app should be launched.
- (void)paymentInstrument:(payments::IOSPaymentInstrument*)paymentInstrument
    launchAppWithUniversalLink:(GURL)universalLink
            instrumentDelegate:(payments::PaymentApp::Delegate*)delegate;

@end

namespace payments {

// Has a copy of payments::WebPaymentRequest as provided by the page invoking
// the PaymentRequest API. Also caches credit cards and addresses provided by
// the |personal_data_manager| and manages shared resources and user selections
// for the current PaymentRequest flow. It must be initialized with non-null
// instances of |browser_state|, |web_state|, and |personal_data_manager| that
// outlive this class.
class PaymentRequest : public PaymentOptionsProvider,
                       public PaymentRequestBaseDelegate {
 public:
  // Represents the state of the payment request.
  enum class State {
    // The payment request is constructed but has not been presented.
    CREATED,

    // The payment request is being presented to the user.
    INTERACTIVE,

    // The payment request completed.
    CLOSED,
  };

  // |personal_data_manager| should not be null and should outlive this object.
  PaymentRequest(const payments::WebPaymentRequest& web_payment_request,
                 ios::ChromeBrowserState* browser_state,
                 web::WebState* web_state,
                 autofill::PersonalDataManager* personal_data_manager,
                 id<PaymentRequestUIDelegate> payment_request_ui_delegate);
  ~PaymentRequest() override;

  // Functor used as a simplified comparison function for unique pointers to
  // PaymentRequest. Only compares |web_payment_request_.payment_request_id|.
  struct Compare {
    bool operator()(const std::unique_ptr<PaymentRequest>& lhs,
                    const std::unique_ptr<PaymentRequest>& rhs) const;
  };

  // PaymentRequestBaseDelegate:
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsIncognito() const override;
  const GURL& GetLastCommittedURL() const override;
  void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;

  State state() const { return state_; }

  void set_state(State state) { state_ = state; }

  bool updating() const { return updating_; }

  void set_updating(bool updating) { updating_ = updating; }

  // Returns the payments::WebPaymentRequest that was used to build this
  // instance.
  const payments::WebPaymentRequest& web_payment_request() const {
    return web_payment_request_;
  }

  // Returns the payment details from |web_payment_request_|.
  const PaymentDetails& payment_details() const {
    return web_payment_request_.details;
  }

  // Returns the JourneyLogger for this instance.
  JourneyLogger& journey_logger() { return journey_logger_; }

  // Returns the total object of this payment request, taking into account the
  // applicable modifier for |selected_instrument|, if any.
  const PaymentItem& GetTotal(PaymentApp* selected_instrument) const;

  // Returns the display items for this payment request, taking into account the
  // applicable modifier for |selected_instrument|, if any.
  std::vector<PaymentItem> GetDisplayItems(
      PaymentApp* selected_instrument) const;

  // Updates the payment details of the |web_payment_request_|. It also updates
  // the cached references to the shipping options in |web_payment_request_| as
  // well as the reference to the selected shipping option.
  void UpdatePaymentDetails(const PaymentDetails& details);

  // PaymentOptionsProvider:
  bool request_shipping() const override;
  bool request_payer_name() const override;
  bool request_payer_phone() const override;
  bool request_payer_email() const override;
  PaymentShippingType shipping_type() const override;

  // Returns the payments::CurrencyFormatter instance for this PaymentRequest.
  // Note: Having multiple currencies per PaymentRequest flow is not supported;
  // hence the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter();

  // Returns the AddressNormalizationManager for this instance.
  virtual autofill::AddressNormalizationManager*
  GetAddressNormalizationManager();

  // Adds |profile| to the list of cached profiles, updates the list of
  // available shipping and contact profiles, and returns a reference to the
  // cached copy of |profile|.
  virtual autofill::AutofillProfile* AddAutofillProfile(
      const autofill::AutofillProfile& profile);

  // Updates the given |profile| in the PersonalDataManager if the user is
  // not in incognito mode.
  virtual void UpdateAutofillProfile(const autofill::AutofillProfile& profile);

  // Returns the available autofill profiles for this user to be used as
  // shipping profiles.
  const std::vector<autofill::AutofillProfile*>& shipping_profiles() const {
    return shipping_profiles_;
  }

  // Returns the currently selected shipping profile for this PaymentRequest
  // flow if there is one. Returns nullptr if there is no selected profile.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }

  // Sets the currently selected shipping profile for this PaymentRequest flow.
  void set_selected_shipping_profile(autofill::AutofillProfile* profile) {
    selected_shipping_profile_ = profile;
  }

  // Returns the available autofill profiles for this user to be used as
  // contact profiles.
  const std::vector<autofill::AutofillProfile*>& contact_profiles() const {
    return contact_profiles_;
  }

  // Returns the currently selected contact profile for this PaymentRequest
  // flow if there is one. Returns nullptr if there is no selected profile.
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }

  // Sets the currently selected contact profile for this PaymentRequest flow.
  void set_selected_contact_profile(autofill::AutofillProfile* profile) {
    selected_contact_profile_ = profile;
  }

  // Returns the available autofill profiles for this user to be used as
  // billing profiles.
  const std::vector<autofill::AutofillProfile*>& billing_profiles() const {
    return shipping_profiles_;
  }

  const std::vector<std::string>& supported_card_networks() const {
    return supported_card_networks_;
  }

  const std::set<std::string>& supported_card_networks_set() const {
    return supported_card_networks_set_;
  }

  const std::vector<GURL>& url_payment_method_identifiers() const {
    return url_payment_method_identifiers_;
  }

  const std::map<std::string, std::set<std::string>>& stringified_method_data()
      const {
    return stringified_method_data_;
  }

  const std::set<autofill::CreditCard::CardType>& supported_card_types_set()
      const {
    return supported_card_types_set_;
  }

  // Creates and adds an AutofillPaymentApp to the list of payment
  // instruments by making a copy of |credit_card|.
  virtual AutofillPaymentApp* CreateAndAddAutofillPaymentInstrument(
      const autofill::CreditCard& credit_card);

  // Updates the given |credit_card| in the PersonalDataManager if the user is
  // not in incognito mode.
  virtual void UpdateAutofillPaymentInstrument(
      const autofill::CreditCard& credit_card);

  // Returns the available payment methods for this user that match a supported
  // type specified in |web_payment_request_|.
  const std::vector<PaymentApp*>& payment_methods() const {
    return payment_methods_;
  }

  // Returns the currently selected payment method for this PaymentRequest flow
  // if there is one. Returns nullptr if there is no selected payment method.
  PaymentApp* selected_payment_method() const {
    return selected_payment_method_;
  }

  // Sets the currently selected payment method for this PaymentRequest flow.
  void set_selected_payment_method(PaymentApp* payment_method) {
    selected_payment_method_ = payment_method;
  }

  // Returns the available shipping options from |web_payment_request_|.
  const std::vector<PaymentShippingOption*>& shipping_options() const {
    return shipping_options_;
  }

  // Returns the selected shipping option from |web_payment_request_| if there
  // is one. Returns nullptr otherwise.
  PaymentShippingOption* selected_shipping_option() const {
    return selected_shipping_option_;
  }

  virtual PaymentsProfileComparator* profile_comparator();

  // Returns a const version of what the non-const |profile_comparator()|
  // method above returns.
  const PaymentsProfileComparator* profile_comparator() const;

  // Returns whether or not all payment instruments have been fetched.
  bool payment_instruments_ready() { return payment_instruments_ready_; }

  // Returns whether the current PaymentRequest can be used to make a payment.
  bool CanMakePayment() const;

  // Returns YES if there's a selected payment method. If shipping is requested,
  // there must be a selected shipping address and a shipping option, otherwise
  // returns NO. If contact info is requeted, there must be a selected contact
  // info, otherwise returns NO.
  bool IsAbleToPay();

  // Returns YES if either payer's name, phone number, or email address are
  // requested and NO otherwise.
  bool RequestContactInfo();

  // Invokes the appropriate payment app for the selected payment method.
  void InvokePaymentApp(id<PaymentResponseHelperConsumer> consumer);

  // Returns whether the payment app has been invoked.
  bool IsPaymentAppInvoked() const;

  // Record the use of the data models that were used in the Payment Request.
  void RecordUseStats();

 protected:
  // Returns the first applicable modifier in the Payment Request for the
  // |selected_instrument|.
  const PaymentDetailsModifier* GetApplicableModifier(
      PaymentApp* selected_instrument) const;

  // Fetches the autofill profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this PaymentRequest, in profile_cache_.
  void PopulateProfileCache();

  // Sets the available shipping and contact profiles as references to the
  // cached profiles ordered by completeness.
  void PopulateAvailableProfiles();

  // Parses the accepted payment method types and card networks requested by
  // the merchant.
  void ParsePaymentMethodData();

  // Starts creating the native app payment methods asynchronously.
  void CreateNativeAppPaymentMethods();

  // Stores a copy of |native_app_instruments| and autofill payment instruments
  // that match the supported types specified in |web_payment_request_|. Sets
  // |selected_payment_method_| and notifies the UI delegate that the payment
  // methods are ready. This serves as a callback for when all native app
  // payment instruments are ready.
  void PopulatePaymentMethodCache(
      std::vector<std::unique_ptr<IOSPaymentInstrument>>
          native_app_instruments);

  // Sets the available payment methods as references to the cached payment
  // methods.
  void PopulateAvailablePaymentMethods();

  // Creates and adds an AutofillPaymentApp to the list of payment
  // instruments by making a copy of |credit_card|. Updates PersonalDataManager
  // if not in incognito mode and |may_update_personal_data_manager| is true.
  AutofillPaymentApp* CreateAndAddAutofillPaymentInstrument(
      const autofill::CreditCard& credit_card,
      bool may_update_personal_data_manager);

  // Sets the available shipping options as references to the shipping options
  // in |web_payment_request_|.
  void PopulateAvailableShippingOptions();

  // Sets the selected shipping option and profile, if any.
  void SetSelectedShippingOptionAndProfile();

  // Records the number of suggestions shown for contact, shipping and payment
  // instrument in the JourneyLogger.
  void RecordNumberOfSuggestionsShown();

  // Records the Contact Info that is requested, and the payment method types.
  void RecordRequestedInformation();

  // The current state of the payment request.
  State state_;

  // Whether there is a pending updateWith() call to update the payment request.
  bool updating_;

  // The payments::WebPaymentRequest object as provided by the page invoking the
  // Payment Request API, owned by this object.
  payments::WebPaymentRequest web_payment_request_;

  // Never null and outlives this object.
  ios::ChromeBrowserState* browser_state_;

  // Never null and outlives this object.
  web::WebState* web_state_;

  // Never null and outlives this object.
  autofill::PersonalDataManager* personal_data_manager_;

  // The PaymentRequestUIDelegate as provided by the UI object that originally
  // created this PaymentRequest object.
  __weak id<PaymentRequestUIDelegate> payment_request_ui_delegate_;

  // Used to normalize the shipping address and the contact info.
  std::unique_ptr<autofill::AddressNormalizationManager>
      address_normalization_manager_;

  // The currency formatter instance for this PaymentRequest flow.
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  // Profiles returned by the Data Manager may change due to (e.g.) sync events,
  // meaning PaymentRequest may outlive them. Therefore, profiles are fetched
  // once and their copies are cached here. Whenever profiles are requested a
  // vector of pointers to these copies are returned.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;

  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  autofill::AutofillProfile* selected_shipping_profile_;

  std::vector<autofill::AutofillProfile*> contact_profiles_;
  autofill::AutofillProfile* selected_contact_profile_;

  // Some payment methods, such as credit cards returned by the Data Manager,
  // may change due to (e.g.) sync events, meaning PaymentRequest may outlive
  // them. Therefore, payment methods are fetched once and their copies are
  // cached here. Whenever payment methods are requested a vector of pointers to
  // these copies are returned.
  std::vector<std::unique_ptr<PaymentApp>> payment_method_cache_;

  std::vector<PaymentApp*> payment_methods_;
  PaymentApp* selected_payment_method_;

  // A vector of supported basic card networks.
  std::vector<std::string> supported_card_networks_;
  std::set<std::string> supported_card_networks_set_;
  // A subset of |supported_card_networks_| which is only the networks that have
  // been specified as part of the "basic-card" supported method. Callers should
  // use |supported_card_networks_| for merchant support checks.
  std::set<std::string> basic_card_specified_networks_;

  // A vector of url-based payment method identifiers supported by the merchant
  // which encompasses one of the two types of payment method identifiers, the
  // other being standardized payment method identifiers i.e., basic-card.
  std::vector<GURL> url_payment_method_identifiers_;

  // A mapping of the payment method names to the corresponding JSON-stringified
  // payment method specific data.
  std::map<std::string, std::set<std::string>> stringified_method_data_;

  // The set of supported card types (e.g., credit, debit, prepaid).
  std::set<autofill::CreditCard::CardType> supported_card_types_set_;

  // A vector of pointers to the shipping options in |web_payment_request_|.
  std::vector<PaymentShippingOption*> shipping_options_;
  PaymentShippingOption* selected_shipping_option_;

  PaymentsProfileComparator profile_comparator_;

  // Keeps track of different stats during the lifetime of this object.
  JourneyLogger journey_logger_;

  std::unique_ptr<PaymentResponseHelper> response_helper_;

  // Boolean to track if payment instruments are still being fetched.
  bool payment_instruments_ready_;

  // Finds all iOS payment instruments for the url payment methods requested by
  // the merchant.
  IOSPaymentInstrumentFinder ios_instrument_finder_;

  base::WeakPtrFactory<PaymentRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_H_
