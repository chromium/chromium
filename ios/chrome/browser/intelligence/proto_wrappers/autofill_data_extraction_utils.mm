// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/autofill_data_extraction_utils.h"

#import "base/logging.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "components/autofill/core/browser/autofill_field.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/intelligence/features/features.h"

// TODO(crbug.com/490114734): Share the logic in
// components/autofill/content/browser/integrators/actor/autofill_annotations_provider_impl.cc
// instead of reimplementing it here.

namespace {

AutofillFieldRedactionReason GetRedactionReason(
    autofill::FieldType field_type) {
  switch (field_type) {
    // Names are generally not redacted.
    case autofill::NO_SERVER_DATA:
    case autofill::UNKNOWN_TYPE:
    case autofill::NAME_FIRST:
    case autofill::NAME_MIDDLE:
    case autofill::NAME_MIDDLE_INITIAL:
    case autofill::NAME_FULL:
    case autofill::NAME_SUFFIX:
    case autofill::NAME_LAST:
    case autofill::NAME_LAST_FIRST:
    case autofill::NAME_LAST_CONJUNCTION:
    case autofill::NAME_LAST_SECOND:
    case autofill::NAME_HONORIFIC_PREFIX:
    case autofill::ALTERNATIVE_FULL_NAME:
    case autofill::ALTERNATIVE_GIVEN_NAME:
    case autofill::ALTERNATIVE_FAMILY_NAME:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Email address is not redacted.
    case autofill::EMAIL_ADDRESS:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Cardholder name is not redacted.
    case autofill::CREDIT_CARD_NAME_FULL:
    case autofill::CREDIT_CARD_NAME_FIRST:
    case autofill::CREDIT_CARD_NAME_LAST:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Other credit card data is redacted.
    case autofill::CREDIT_CARD_NUMBER:
    case autofill::CREDIT_CARD_EXP_MONTH:
    case autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case autofill::CREDIT_CARD_TYPE:
    case autofill::CREDIT_CARD_VERIFICATION_CODE:
    case autofill::CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return AutofillFieldRedactionReason::kShouldRedactForPayments;

    // Password fields have already been redacted on the renderer side.
    case autofill::PASSWORD:
    case autofill::ACCOUNT_CREATION_PASSWORD:
    case autofill::NOT_ACCOUNT_CREATION_PASSWORD:
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
    case autofill::NEW_PASSWORD:
    case autofill::PROBABLY_NEW_PASSWORD:
    case autofill::NOT_NEW_PASSWORD:
    case autofill::CONFIRMATION_PASSWORD:
    case autofill::SINGLE_USERNAME:
    case autofill::SINGLE_USERNAME_FORGOT_PASSWORD:
    case autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // The following fields have not yet been decided upon. By default they are
    // considered non-sensitive to avoid over-redacting. Any item here may
    // change from `AutofillFieldRedactionReason::kNoRedactionNeeded` to
    // `AutofillFieldRedactionReason::kShouldRedactForPayments` in the future.
    case autofill::ADDRESS_HOME_LINE1:
    case autofill::ADDRESS_HOME_LINE2:
    case autofill::ADDRESS_HOME_APT_NUM:
    case autofill::ADDRESS_HOME_CITY:
    case autofill::ADDRESS_HOME_STATE:
    case autofill::ADDRESS_HOME_ZIP:
    case autofill::ADDRESS_HOME_COUNTRY:
    case autofill::ADDRESS_HOME_STREET_ADDRESS:
    case autofill::ADDRESS_HOME_SORTING_CODE:
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY:
    case autofill::ADDRESS_HOME_LINE3:
    case autofill::ADDRESS_HOME_STREET_NAME:
    case autofill::ADDRESS_HOME_HOUSE_NUMBER:
    case autofill::ADDRESS_HOME_SUBPREMISE:
    case autofill::ADDRESS_HOME_OTHER_SUBUNIT:
    case autofill::ADDRESS_HOME_ADDRESS:
    case autofill::ADDRESS_HOME_ADDRESS_WITH_NAME:
    case autofill::ADDRESS_HOME_FLOOR:
    case autofill::ADDRESS_HOME_OVERFLOW:
    case autofill::ADDRESS_HOME_LANDMARK:
    case autofill::ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case autofill::ADDRESS_HOME_ADMIN_LEVEL2:
    case autofill::ADDRESS_HOME_STREET_LOCATION:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_1:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_2:
    case autofill::ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case autofill::ADDRESS_HOME_APT:
    case autofill::ADDRESS_HOME_APT_TYPE:
    case autofill::ADDRESS_HOME_ZIP_AND_CITY:
    case autofill::ADDRESS_HOME_ZIP_PREFIX:
    case autofill::ADDRESS_HOME_ZIP_SUFFIX:
    case autofill::PHONE_HOME_NUMBER:
    case autofill::PHONE_HOME_CITY_CODE:
    case autofill::PHONE_HOME_COUNTRY_CODE:
    case autofill::PHONE_HOME_CITY_AND_NUMBER:
    case autofill::PHONE_HOME_WHOLE_NUMBER:
    case autofill::PHONE_HOME_EXTENSION:
    case autofill::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case autofill::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case autofill::PHONE_HOME_NUMBER_PREFIX:
    case autofill::PHONE_HOME_NUMBER_SUFFIX:
    case autofill::COMPANY_NAME:
    case autofill::MERCHANT_EMAIL_SIGNUP:
    case autofill::MERCHANT_PROMO_CODE:
    case autofill::AMBIGUOUS_TYPE:
    case autofill::SEARCH_TERM:
    case autofill::PRICE:
    case autofill::NOT_PASSWORD:
    case autofill::NOT_USERNAME:
    case autofill::IBAN_VALUE:
    case autofill::NUMERIC_QUANTITY:
    case autofill::ONE_TIME_CODE:
    case autofill::DELIVERY_INSTRUCTIONS:
    case autofill::LOYALTY_MEMBERSHIP_ID:
    case autofill::PASSPORT_NUMBER:
    case autofill::PASSPORT_ISSUING_COUNTRY:
    case autofill::PASSPORT_EXPIRATION_DATE:
    case autofill::PASSPORT_ISSUE_DATE:
    case autofill::LOYALTY_MEMBERSHIP_PROGRAM:
    case autofill::LOYALTY_MEMBERSHIP_PROVIDER:
    case autofill::VEHICLE_LICENSE_PLATE:
    case autofill::VEHICLE_VIN:
    case autofill::VEHICLE_MAKE:
    case autofill::VEHICLE_MODEL:
    case autofill::DRIVERS_LICENSE_REGION:
    case autofill::DRIVERS_LICENSE_NUMBER:
    case autofill::DRIVERS_LICENSE_EXPIRATION_DATE:
    case autofill::DRIVERS_LICENSE_ISSUE_DATE:
    case autofill::VEHICLE_YEAR:
    case autofill::VEHICLE_PLATE_STATE:
    case autofill::EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
    case autofill::NATIONAL_ID_CARD_NUMBER:
    case autofill::NATIONAL_ID_CARD_EXPIRATION_DATE:
    case autofill::NATIONAL_ID_CARD_ISSUE_DATE:
    case autofill::NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case autofill::KNOWN_TRAVELER_NUMBER:
    case autofill::KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case autofill::REDRESS_NUMBER:
    case autofill::FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case autofill::FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case autofill::FLIGHT_RESERVATION_TICKET_NUMBER:
    case autofill::FLIGHT_RESERVATION_DEPARTURE_DATE:
    case autofill::ORDER_ID:
    case autofill::ORDER_DATE:
    case autofill::ORDER_MERCHANT_NAME:
    case autofill::SHIPMENT_TRACKING_NUMBER:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // These cases are not produced by field classification, but have to be
    // handled so that the switch is complete.
    case autofill::EMPTY_TYPE:
    case autofill::MAX_VALID_FIELD_TYPE:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;
  }
  return AutofillFieldRedactionReason::kNoRedactionNeeded;
}

// Generates a unique key for an Autofill section based on the form's global ID
// and the field's section name.
std::string GetSectionIdKey(const autofill::FormStructure& form,
                            const autofill::AutofillField& field) {
  return base::StrCat(
      {form.global_id().frame_token.ToString(),
       base::NumberToString(form.global_id().renderer_id.value()),
       field.section().ToString()});
}

}  // namespace

AutofillExtractionContext::AutofillExtractionContext() = default;

AutofillExtractionContext::AutofillExtractionContext(
    base::WeakPtr<web::WebState> web_state,
    std::optional<autofill::LocalFrameToken> frame_token,
    bool extract_autofill_credit_card_redactions,
    raw_ptr<base::flat_map<std::string, uint32_t>> section_numbers)
    : web_state(std::move(web_state)),
      frame_token(std::move(frame_token)),
      section_numbers(section_numbers),
      extract_autofill_credit_card_redactions(
          extract_autofill_credit_card_redactions) {
  CHECK(section_numbers);
}
AutofillExtractionContext::~AutofillExtractionContext() = default;

AutofillFieldRedactionReason GetRedactionReason(
    const autofill::FieldTypeSet& field_types) {
  for (const autofill::FieldType field_type : field_types) {
    AutofillFieldRedactionReason redaction_reason =
        GetRedactionReason(field_type);
    switch (redaction_reason) {
      case AutofillFieldRedactionReason::kShouldRedactForPayments:
        return redaction_reason;
      case AutofillFieldRedactionReason::kNoRedactionNeeded:
        continue;
    }
  }

  return AutofillFieldRedactionReason::kNoRedactionNeeded;
}

optimization_guide::proto::RedactionDecision
ConvertAutofillFieldRedactionReason(
    const optimization_guide::proto::FormControlData& form_control_data,
    AutofillFieldRedactionReason redaction_reason) {
  switch (redaction_reason) {
    case AutofillFieldRedactionReason::kNoRedactionNeeded:
      return optimization_guide::proto::
          REDACTION_DECISION_NO_REDACTION_NECESSARY;
    case AutofillFieldRedactionReason::kShouldRedactForPayments:
      return form_control_data.field_value().empty()
                 ? optimization_guide::proto::
                       REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD
                 : optimization_guide::proto::
                       REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD;
  }
}

bool ShouldRedactContent(
    optimization_guide::proto::RedactionDecision redaction_decision,
    const AutofillExtractionContext& autofill_context) {
  switch (redaction_decision) {
    case optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY:
    case optimization_guide::proto::
        REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD:
    case optimization_guide::proto::
        REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD:
      return false;

    case optimization_guide::proto::
        REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD:
      return true;

    case optimization_guide::proto::
        REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD:
      return autofill_context.extract_autofill_credit_card_redactions;

    default:
      // We cannot exhaustively switch nor static_assert on the proto values, as
      // otherwise automatic syncing of new enum values will break compilation.
      // Instead, we default to not redacting and just best-effort log.
      DUMP_WILL_BE_CHECK(false)
          << "Missing case statement in ShouldRedactContent";
      return false;
  }
}

std::optional<AutofillFieldMetadata> GetAutofillFieldData(
    int32_t dom_node_id,
    AutofillExtractionContext& autofill_context) {
  if (!autofill_context.web_state || !autofill_context.frame_token) {
    return std::nullopt;
  }

  autofill::AutofillDriverIOS* autofill_driver =
      autofill::AutofillDriverIOS::FromWebStateAndLocalFrameToken(
          autofill_context.web_state.get(), *autofill_context.frame_token);
  if (!autofill_driver) {
    return std::nullopt;
  }

  autofill::BrowserAutofillManager& autofill_manager =
      autofill_driver->GetAutofillManager();
  autofill::FieldGlobalId field_global_id = {
      *autofill_context.frame_token, autofill::FieldRendererId(dom_node_id)};
  const autofill::FormStructure* form =
      autofill_manager.FindCachedFormById(field_global_id);
  if (!form) {
    return std::nullopt;
  }

  const autofill::AutofillField* field = form->GetFieldById(field_global_id);
  if (!field) {
    return std::nullopt;
  }

  AutofillFieldMetadata metadata;

  // Get the section ID for the key, create it if it doesn't exist.
  std::string section_id_key = GetSectionIdKey(*form, *field);
  auto iter = autofill_context.section_numbers->find(section_id_key);
  if (iter == autofill_context.section_numbers->end()) {
    iter =
        autofill_context.section_numbers
            ->emplace(section_id_key, autofill_context.section_numbers->size())
            .first;
  }
  metadata.section_id = iter->second;

  const autofill::DenseSet<autofill::FormType>& form_types =
      field->Type().GetFormTypes();
  const autofill::FieldTypeSet& field_types = field->Type().GetTypes();

  if (form_types.contains(autofill::FormType::kAddressForm)) {
    metadata.coarse_field_type =
        optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS;
  } else if (form_types.contains(autofill::FormType::kCreditCardForm) ||
             form_types.contains(autofill::FormType::kStandaloneCvcForm)) {
    metadata.coarse_field_type =
        optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD;
  } else {
    metadata.coarse_field_type =
        optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_UNSUPPORTED;
  }

  metadata.redaction_reason = GetRedactionReason(field_types);

  return metadata;
}
