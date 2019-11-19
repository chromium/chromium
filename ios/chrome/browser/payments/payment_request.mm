// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/region_data_loader_impl.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/web_payment_request.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/payments/ios_payment_instrument.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {
namespace {

std::unique_ptr<::i18n::addressinput::Source> GetAddressInputSource() {
  return std::unique_ptr<::i18n::addressinput::Source>(
      new autofill::ChromeMetadataSource(
          I18N_ADDRESS_VALIDATION_DATA_URL,
          GetApplicationContext()->GetSharedURLLoaderFactory()));
}

std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage() {
  return autofill::ValidationRulesStorageFactory::CreateStorage();
}

// Validates the |method_data| and fills the output parameters.
void PopulateValidatedMethodData(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* supported_card_networks,
    std::set<std::string>* basic_card_specified_networks,
    std::set<std::string>* supported_card_networks_set,
    std::set<autofill::CreditCard::CardType>* supported_card_types,
    std::vector<GURL>* url_payment_method_identifiers,
    std::set<std::string>* payment_method_identifiers) {
  data_util::ParseSupportedMethods(
      method_data, supported_card_networks, basic_card_specified_networks,
      url_payment_method_identifiers, payment_method_identifiers);
  supported_card_networks_set->insert(supported_card_networks->begin(),
                                      supported_card_networks->end());

  data_util::ParseSupportedCardTypes(method_data, supported_card_types);
}

}  // namespace

PaymentRequest::PaymentRequest(
    const payments::WebPaymentRequest& web_payment_request,
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    autofill::PersonalDataManager* personal_data_manager,
    id<PaymentRequestUIDelegate> payment_request_ui_delegate)
    : state_(State::CREATED),
      updating_(false),
      web_payment_request_(web_payment_request),
      browser_state_(browser_state),
      web_state_(web_state),
      personal_data_manager_(personal_data_manager),
      payment_request_ui_delegate_(payment_request_ui_delegate),
      selected_shipping_profile_(nullptr),
      selected_contact_profile_(nullptr),
      selected_payment_method_(nullptr),
      selected_shipping_option_(nullptr),
      profile_comparator_(GetApplicationLocale(), *this),
      journey_logger_(IsIncognito(),
                      ukm::GetSourceIdForWebStateDocument(web_state)),
      payment_instruments_ready_(false),
      ios_instrument_finder_(
          GetApplicationContext()->GetSharedURLLoaderFactory(),
          payment_request_ui_delegate_) {
  PopulateAvailableShippingOptions();
  PopulateProfileCache();
  PopulateAvailableProfiles();

  ParsePaymentMethodData();
  CreateNativeAppPaymentMethods();

  SetSelectedShippingOptionAndProfile();

  if (request_payer_name() || request_payer_email() || request_payer_phone()) {
    // If the highest-ranking contact profile is usable, select it. Otherwise,
    // select none.
    if (!contact_profiles_.empty() &&
        profile_comparator_.IsContactInfoComplete(contact_profiles_[0])) {
      selected_contact_profile_ = contact_profiles_[0];
    }
  }

  RecordNumberOfSuggestionsShown();
  RecordRequestedInformation();
}

PaymentRequest::~PaymentRequest() {}

bool PaymentRequest::Compare::operator()(
    const std::unique_ptr<PaymentRequest>& lhs,
    const std::unique_ptr<PaymentRequest>& rhs) const {
  return lhs->web_payment_request().payment_request_id !=
         rhs->web_payment_request().payment_request_id;
}

autofill::PersonalDataManager* PaymentRequest::GetPersonalDataManager() {
  return personal_data_manager_;
}

const std::string& PaymentRequest::GetApplicationLocale() const {
  return GetApplicationContext()->GetApplicationLocale();
}

bool PaymentRequest::IsIncognito() const {
  return browser_state_->IsOffTheRecord();
}

const GURL& PaymentRequest::GetLastCommittedURL() const {
  return web_state_->GetLastCommittedURL();
}

void PaymentRequest::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  [payment_request_ui_delegate_ paymentRequest:this
                         requestFullCreditCard:credit_card
                                resultDelegate:result_delegate];
}

autofill::AddressNormalizer* PaymentRequest::GetAddressNormalizer() {
  return autofill::AddressNormalizerFactory::GetInstance();
}

autofill::RegionDataLoader* PaymentRequest::GetRegionDataLoader() {
  return new autofill::RegionDataLoaderImpl(GetAddressInputSource().release(),
                                            GetAddressInputStorage().release(),
                                            GetApplicationLocale());
}

ukm::UkmRecorder* PaymentRequest::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
}

std::string PaymentRequest::GetAuthenticatedEmail() const {
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserStateIfExists(browser_state_);
  if (identity_manager && identity_manager->HasPrimaryAccount())
    return identity_manager->GetPrimaryAccountInfo().email;
  else
    return std::string();
}

PrefService* PaymentRequest::GetPrefService() {
  return browser_state_->GetPrefs();
}

const PaymentItem& PaymentRequest::GetTotal(
    PaymentApp* selected_instrument) const {
  const PaymentDetailsModifier* modifier =
      GetApplicableModifier(selected_instrument);
  if (modifier && modifier->total) {
    return *modifier->total;
  } else {
    DCHECK(web_payment_request_.details.total);
    return *web_payment_request_.details.total;
  }
}

std::vector<PaymentItem> PaymentRequest::GetDisplayItems(
    PaymentApp* selected_instrument) const {
  std::vector<PaymentItem> display_items =
      web_payment_request_.details.display_items;

  const PaymentDetailsModifier* modifier =
      GetApplicableModifier(selected_instrument);
  if (modifier) {
    display_items.insert(display_items.end(),
                         modifier->additional_display_items.begin(),
                         modifier->additional_display_items.end());
  }
  return display_items;
}

void PaymentRequest::UpdatePaymentDetails(const PaymentDetails& details) {
  DCHECK(web_payment_request_.details.total);
  std::unique_ptr<PaymentItem> old_total =
      std::move(web_payment_request_.details.total);
  web_payment_request_.details = details;
  // Restore the old total amount if the PaymentDetails passed to updateWith()
  // is missing a total value.
  if (!web_payment_request_.details.total)
    web_payment_request_.details.total = std::move(old_total);

  PopulateAvailableShippingOptions();
  SetSelectedShippingOptionAndProfile();
}

bool PaymentRequest::request_shipping() const {
  return web_payment_request_.options.request_shipping;
}

bool PaymentRequest::request_payer_name() const {
  return web_payment_request_.options.request_payer_name;
}

bool PaymentRequest::request_payer_phone() const {
  return web_payment_request_.options.request_payer_phone;
}

bool PaymentRequest::request_payer_email() const {
  return web_payment_request_.options.request_payer_email;
}

PaymentShippingType PaymentRequest::shipping_type() const {
  return web_payment_request_.options.shipping_type;
}

CurrencyFormatter* PaymentRequest::GetOrCreateCurrencyFormatter() {
  if (!currency_formatter_) {
    DCHECK(web_payment_request_.details.total);
    currency_formatter_ = std::make_unique<CurrencyFormatter>(
        web_payment_request_.details.total->amount->currency,
        GetApplicationLocale());
  }
  return currency_formatter_.get();
}

autofill::AddressNormalizationManager*
PaymentRequest::GetAddressNormalizationManager() {
  if (!address_normalization_manager_) {
    address_normalization_manager_ =
        std::make_unique<autofill::AddressNormalizationManager>(
            GetAddressNormalizer(),
            GetApplicationContext()->GetApplicationLocale());
  }
  return address_normalization_manager_.get();
}

autofill::AutofillProfile* PaymentRequest::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));

  contact_profiles_.push_back(profile_cache_.back().get());
  shipping_profiles_.push_back(profile_cache_.back().get());

  if (!IsIncognito())
    personal_data_manager_->AddProfile(profile);

  return profile_cache_.back().get();
}

void PaymentRequest::UpdateAutofillProfile(
    const autofill::AutofillProfile& profile) {
  // Cached profile must be invalidated once the profile is modified.
  profile_comparator()->Invalidate(profile);

  if (!IsIncognito())
    personal_data_manager_->UpdateProfile(profile);
}

const PaymentDetailsModifier* PaymentRequest::GetApplicableModifier(
    PaymentApp* selected_instrument) const {
  if (!selected_instrument ||
      !base::FeatureList::IsEnabled(features::kWebPaymentsModifiers)) {
    return nullptr;
  }

  for (const auto& modifier : web_payment_request_.details.modifiers) {
    std::set<std::string> supported_card_networks_set;
    std::set<autofill::CreditCard::CardType> supported_card_types_set;
    // The following 4 variables are unused.
    std::set<std::string> unused_basic_card_specified_networks;
    std::vector<std::string> unused_supported_card_networks;
    std::vector<GURL> unused_url_payment_method_identifiers;
    std::set<std::string> unused_payment_method_identifiers;
    PopulateValidatedMethodData(
        {modifier.method_data}, &unused_supported_card_networks,
        &unused_basic_card_specified_networks, &supported_card_networks_set,
        &supported_card_types_set, &unused_url_payment_method_identifiers,
        &unused_payment_method_identifiers);

    if (selected_instrument->IsValidForModifier(
            modifier.method_data.supported_method,
            !modifier.method_data.supported_networks.empty(),
            supported_card_networks_set,
            !modifier.method_data.supported_types.empty(),
            supported_card_types_set)) {
      return &modifier;
    }
  }
  return nullptr;
}

void PaymentRequest::PopulateProfileCache() {
  const std::vector<autofill::AutofillProfile*>& profiles_to_suggest =
      personal_data_manager_->GetProfilesToSuggest();
  // Return early if the user has no stored Autofill profiles.
  if (profiles_to_suggest.empty())
    return;

  profile_cache_.clear();
  profile_cache_.reserve(profiles_to_suggest.size());

  for (const auto* profile : profiles_to_suggest) {
    profile_cache_.push_back(
        std::make_unique<autofill::AutofillProfile>(*profile));
  }
}

void PaymentRequest::PopulateAvailableProfiles() {
  if (profile_cache_.empty())
    return;

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profile_cache_.size());

  for (auto const& profile : profile_cache_) {
    raw_profiles_for_filtering.push_back(profile.get());
  }

  // Contact profiles are deduped and ordered by completeness.
  contact_profiles_ =
      profile_comparator_.FilterProfilesForContact(raw_profiles_for_filtering);

  // Shipping profiles are ordered by completeness.
  shipping_profiles_ =
      profile_comparator_.FilterProfilesForShipping(raw_profiles_for_filtering);
}

AutofillPaymentApp* PaymentRequest::CreateAndAddAutofillPaymentInstrument(
    const autofill::CreditCard& credit_card) {
  return CreateAndAddAutofillPaymentInstrument(
      credit_card, /*may_update_personal_data_manager=*/true);
}

AutofillPaymentApp* PaymentRequest::CreateAndAddAutofillPaymentInstrument(
    const autofill::CreditCard& credit_card,
    bool may_update_personal_data_manager) {
  std::string basic_card_issuer_network =
      autofill::data_util::GetPaymentRequestData(credit_card.network())
          .basic_card_issuer_network;

  if (!supported_card_networks_set_.count(basic_card_issuer_network) ||
      !supported_card_types_set_.count(credit_card.card_type())) {
    return nullptr;
  }

  // If the merchant specified the card network as part of the "basic-card"
  // payment method, use "basic-card" as the method_name. Otherwise, use
  // the name of the network directly.
  std::string method_name = basic_card_issuer_network;
  if (basic_card_specified_networks_.count(basic_card_issuer_network)) {
    method_name = methods::kBasicCard;
  }

  // The total number of card types: credit, debit, prepaid, unknown.
  constexpr size_t kTotalNumberOfCardTypes = 4U;

  // Whether the card type (credit, debit, prepaid) matches the type that the
  // merchant has requested exactly. This should be false for unknown card
  // types, if the merchant cannot accept some card types.
  bool matches_merchant_card_type_exactly =
      credit_card.card_type() != autofill::CreditCard::CARD_TYPE_UNKNOWN ||
      supported_card_types_set_.size() == kTotalNumberOfCardTypes;

  // AutofillPaymentApp makes a copy of |credit_card| so it is
  // effectively owned by this object.
  payment_method_cache_.push_back(std::make_unique<AutofillPaymentApp>(
      method_name, credit_card, matches_merchant_card_type_exactly,
      billing_profiles(), GetApplicationLocale(), this));

  payment_methods_.push_back(payment_method_cache_.back().get());

  if (may_update_personal_data_manager && !IsIncognito())
    personal_data_manager_->AddCreditCard(credit_card);

  return static_cast<AutofillPaymentApp*>(payment_method_cache_.back().get());
}

void PaymentRequest::UpdateAutofillPaymentInstrument(
    const autofill::CreditCard& credit_card) {
  if (IsIncognito())
    return;

  if (autofill::IsCreditCardLocal(credit_card))
    personal_data_manager_->UpdateCreditCard(credit_card);
  else
    personal_data_manager_->UpdateServerCardMetadata(credit_card);
}

PaymentsProfileComparator* PaymentRequest::profile_comparator() {
  return &profile_comparator_;
}

const PaymentsProfileComparator* PaymentRequest::profile_comparator() const {
  // Return a const version of what the non-const |profile_comparator| method
  // returns.
  return const_cast<PaymentRequest*>(this)->profile_comparator();
}

bool PaymentRequest::CanMakePayment() const {
  for (PaymentApp* payment_method : payment_methods_) {
    if (payment_method->IsValidForCanMakePayment()) {
      return true;
    }
  }
  return false;
}

bool PaymentRequest::IsAbleToPay() {
  return selected_payment_method() != nullptr &&
         (selected_shipping_option() != nullptr || !request_shipping()) &&
         (selected_shipping_profile() != nullptr || !request_shipping()) &&
         (selected_contact_profile() != nullptr || !RequestContactInfo());
}

bool PaymentRequest::RequestContactInfo() {
  return request_payer_name() || request_payer_email() || request_payer_phone();
}

void PaymentRequest::InvokePaymentApp(
    id<PaymentResponseHelperConsumer> consumer) {
  DCHECK(selected_payment_method());
  response_helper_ = std::make_unique<PaymentResponseHelper>(consumer, this);
  selected_payment_method()->InvokePaymentApp(response_helper_.get());
}

bool PaymentRequest::IsPaymentAppInvoked() const {
  return !!response_helper_;
}

void PaymentRequest::RecordUseStats() {
  if (request_shipping()) {
    DCHECK(selected_shipping_profile_);
    personal_data_manager_->RecordUseOf(*selected_shipping_profile_);
  }

  if (request_payer_name() || request_payer_email() || request_payer_phone()) {
    DCHECK(selected_contact_profile_);
    // If the same address was used for both contact and shipping, the stats
    // should be updated only once.
    if (!request_shipping() || (selected_shipping_profile_->guid() !=
                                selected_contact_profile_->guid())) {
      personal_data_manager_->RecordUseOf(*selected_contact_profile_);
    }
  }

  selected_payment_method_->RecordUse();
}

void PaymentRequest::ParsePaymentMethodData() {
  for (const PaymentMethodData& method_data_entry :
       web_payment_request_.method_data) {
    stringified_method_data_[method_data_entry.supported_method].insert(
        method_data_entry.data);
  }

  std::set<std::string> unused_payment_method_identifiers;
  PopulateValidatedMethodData(
      web_payment_request_.method_data, &supported_card_networks_,
      &basic_card_specified_networks_, &supported_card_networks_set_,
      &supported_card_types_set_, &url_payment_method_identifiers_,
      &unused_payment_method_identifiers);
}

void PaymentRequest::CreateNativeAppPaymentMethods() {
  if (!base::FeatureList::IsEnabled(
          payments::features::kWebPaymentsNativeApps)) {
    PopulatePaymentMethodCache(
        std::vector<std::unique_ptr<IOSPaymentInstrument>>());
    return;
  }

  url_payment_method_identifiers_ =
      ios_instrument_finder_.CreateIOSPaymentInstrumentsForMethods(
          url_payment_method_identifiers_,
          base::BindOnce(&PaymentRequest::PopulatePaymentMethodCache,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PaymentRequest::PopulatePaymentMethodCache(
    std::vector<std::unique_ptr<IOSPaymentInstrument>> native_app_instruments) {
  const std::vector<autofill::CreditCard*>& credit_cards_to_suggest =
      personal_data_manager_->GetCreditCardsToSuggest(
          /*include_server_cards=*/base::FeatureList::IsEnabled(
              payments::features::kReturnGooglePayInBasicCard));

  // Return early if the user has no stored credit cards or installed payment
  // apps.
  if (native_app_instruments.empty() && credit_cards_to_suggest.empty()) {
    payment_instruments_ready_ = true;
    [payment_request_ui_delegate_ paymentRequestDidFetchPaymentMethods:this];
    return;
  }

  payment_method_cache_.clear();
  payment_method_cache_.reserve(native_app_instruments.size() +
                                credit_cards_to_suggest.size());

  for (auto& instrument : native_app_instruments)
    payment_method_cache_.push_back(std::move(instrument));

  for (const auto* credit_card : credit_cards_to_suggest) {
    // We only want to add the credit cards read from the PersonalDataManager to
    // the list of payment instrument. Don't re-add them to PersonalDataManager.
    CreateAndAddAutofillPaymentInstrument(
        *credit_card,
        /*may_update_personal_data_manager=*/false);
  }

  PopulateAvailablePaymentMethods();

  const auto first_complete_payment_method =
      std::find_if(payment_methods_.begin(), payment_methods_.end(),
                   [](PaymentApp* payment_method) {
                     return payment_method->IsCompleteForPayment() &&
                            payment_method->IsExactlyMatchingMerchantRequest();
                   });
  if (first_complete_payment_method != payment_methods_.end())
    selected_payment_method_ = *first_complete_payment_method;

  payment_instruments_ready_ = true;
  [payment_request_ui_delegate_ paymentRequestDidFetchPaymentMethods:this];
}

void PaymentRequest::PopulateAvailablePaymentMethods() {
  if (payment_method_cache_.empty())
    return;

  payment_methods_.clear();
  payment_methods_.reserve(payment_method_cache_.size());

  for (auto const& payment_method : payment_method_cache_)
    payment_methods_.push_back(payment_method.get());
}

void PaymentRequest::PopulateAvailableShippingOptions() {
  shipping_options_.clear();
  if (web_payment_request_.details.shipping_options.empty())
    return;

  shipping_options_.reserve(
      web_payment_request_.details.shipping_options.size());
  std::transform(std::begin(web_payment_request_.details.shipping_options),
                 std::end(web_payment_request_.details.shipping_options),
                 std::back_inserter(shipping_options_),
                 [](PaymentShippingOption& option) { return &option; });
}

void PaymentRequest::SetSelectedShippingOptionAndProfile() {
  // If more than one option has |selected| set, the last one in the sequence
  // should be treated as the selected item.
  selected_shipping_option_ = nullptr;
  for (auto* shipping_option : base::Reversed(shipping_options_)) {
    if (shipping_option->selected) {
      selected_shipping_option_ = shipping_option;
      break;
    }
  }

  selected_shipping_profile_ = nullptr;
  if (request_shipping()) {
    // If the merchant provided a default shipping option, and the
    // highest-ranking shipping profile is usable, select it.
    if (selected_shipping_option_ && !shipping_profiles_.empty() &&
        profile_comparator_.IsShippingComplete(shipping_profiles_[0])) {
      selected_shipping_profile_ = shipping_profiles_[0];
    }
  }
}

void PaymentRequest::RecordNumberOfSuggestionsShown() {
  if (request_payer_name() || request_payer_phone() || request_payer_email()) {
    const bool has_complete_contact = (selected_contact_profile_ != nullptr);
    journey_logger().SetNumberOfSuggestionsShown(
        payments::JourneyLogger::Section::SECTION_CONTACT_INFO,
        contact_profiles().size(), has_complete_contact);
  }

  if (request_shipping()) {
    const bool has_complete_shipping = (selected_shipping_profile_ != nullptr);
    journey_logger().SetNumberOfSuggestionsShown(
        payments::JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
        shipping_profiles().size(), has_complete_shipping);
  }

  const bool has_complete_instrument = (selected_payment_method_ != nullptr);
  journey_logger().SetNumberOfSuggestionsShown(
      payments::JourneyLogger::Section::SECTION_PAYMENT_METHOD,
      payment_methods().size(), has_complete_instrument);
}

void PaymentRequest::RecordRequestedInformation() {
  journey_logger().SetRequestedInformation(
      request_shipping(), request_payer_email(), request_payer_phone(),
      request_payer_name());

  // Log metrics around which payment methods are requested by the merchant.
  const GURL kGooglePayUrl(methods::kGooglePay);
  const GURL kAndroidPayUrl(methods::kAndroidPay);

  // Looking for payment methods that are NOT Google-related as well as the
  // Google-related ones.
  bool requestedMethodGoogle = false;
  bool requestedMethodOther = false;
  for (const GURL& url_payment_method : url_payment_method_identifiers()) {
    if (url_payment_method == kGooglePayUrl ||
        url_payment_method == kAndroidPayUrl) {
      requestedMethodGoogle = true;
    } else {
      requestedMethodOther = true;
    }
  }

  journey_logger().SetRequestedPaymentMethodTypes(
      /*requested_basic_card=*/!supported_card_networks().empty(),
      /*requested_method_google=*/requestedMethodGoogle,
      /*requested_method_other=*/requestedMethodOther);
}

}  // namespace payments
