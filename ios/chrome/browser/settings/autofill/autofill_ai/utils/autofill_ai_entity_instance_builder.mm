// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"

#import "base/uuid.h"

namespace autofill {

EntityInstanceBuilder::EntityInstanceBuilder(EntityType type)
    : type_(type),
      guid_(EntityInstance::EntityId(base::Uuid::GenerateRandomV4())),
      date_modified_(base::Time::Now()) {}

EntityInstanceBuilder::~EntityInstanceBuilder() = default;

EntityInstanceBuilder& EntityInstanceBuilder::SetGUID(
    EntityInstance::EntityId guid) {
  guid_ = std::move(guid);
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetNickname(
    std::string nickname) {
  nickname_ = std::move(nickname);
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetDateModified(
    base::Time date_modified) {
  date_modified_ = date_modified;
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetUseCount(int64_t use_count) {
  use_count_ = use_count;
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetUseDate(base::Time use_date) {
  use_date_ = use_date;
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetRecordType(
    EntityInstance::RecordType record_type) {
  record_type_ = record_type;
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetAreAttributesReadOnly(
    EntityInstance::AreAttributesReadOnly are_attributes_read_only) {
  are_attributes_read_only_ = are_attributes_read_only;
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::SetFrecencyOverride(
    std::string frecency_override) {
  frecency_override_ = std::move(frecency_override);
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::AddAttribute(
    AttributeInstance attribute) {
  attributes_.erase(attribute);
  attributes_.insert(std::move(attribute));
  return *this;
}

EntityInstanceBuilder& EntityInstanceBuilder::AddPrimaryAttribute() {
  autofill::AttributeTypeName primary_attribute_name =
      autofill::AttributeTypeName::kPassportName;
  switch (type_.name()) {
    case autofill::EntityTypeName::kPassport:
      primary_attribute_name = autofill::AttributeTypeName::kPassportName;
      break;
    case autofill::EntityTypeName::kDriversLicense:
      primary_attribute_name = autofill::AttributeTypeName::kDriversLicenseName;
      break;
    case autofill::EntityTypeName::kNationalIdCard:
      primary_attribute_name = autofill::AttributeTypeName::kNationalIdCardName;
      break;
    case autofill::EntityTypeName::kVehicle:
      primary_attribute_name = autofill::AttributeTypeName::kVehicleMake;
      break;
    case autofill::EntityTypeName::kFlightReservation:
      primary_attribute_name =
          autofill::AttributeTypeName::kFlightReservationFlightNumber;
      break;
    case autofill::EntityTypeName::kKnownTravelerNumber:
      primary_attribute_name =
          autofill::AttributeTypeName::kKnownTravelerNumberName;
      break;
    case autofill::EntityTypeName::kRedressNumber:
      primary_attribute_name = autofill::AttributeTypeName::kRedressNumberName;
      break;
    case autofill::EntityTypeName::kOrder:
      primary_attribute_name = autofill::AttributeTypeName::kOrderId;
      break;
    case autofill::EntityTypeName::kShipment:
      primary_attribute_name =
          autofill::AttributeTypeName::kShipmentTrackingNumber;
      break;
    default:
      NOTREACHED();
  }
  return AddAttribute(autofill::AttributeInstance(
      autofill::AttributeType(primary_attribute_name)));
}

EntityInstance EntityInstanceBuilder::Build() {
  CHECK(!attributes_.empty());
  return EntityInstance(type_, std::move(attributes_), guid_,
                        std::move(nickname_), date_modified_, use_count_,
                        use_date_, record_type_, are_attributes_read_only_,
                        std::move(frecency_override_));
}

}  // namespace autofill
