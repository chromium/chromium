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

EntityInstanceBuilder& EntityInstanceBuilder::SetRecordTypeData(
    EntityInstance::RecordTypeData record_type_data) {
  record_type_data_ = std::move(record_type_data);
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
  AttributeTypeName primary_attribute_name = AttributeTypeName::kPassportName;
  switch (type_.name()) {
    case EntityTypeName::kPassport:
      primary_attribute_name = AttributeTypeName::kPassportName;
      break;
    case EntityTypeName::kDriversLicense:
      primary_attribute_name = AttributeTypeName::kDriversLicenseName;
      break;
    case EntityTypeName::kNationalIdCard:
      primary_attribute_name = AttributeTypeName::kNationalIdCardName;
      break;
    case EntityTypeName::kVehicle:
      primary_attribute_name = AttributeTypeName::kVehicleMake;
      break;
    case EntityTypeName::kFlightReservation:
      primary_attribute_name =
          AttributeTypeName::kFlightReservationFlightNumber;
      break;
    case EntityTypeName::kKnownTravelerNumber:
      primary_attribute_name = AttributeTypeName::kKnownTravelerNumberName;
      break;
    case EntityTypeName::kRedressNumber:
      primary_attribute_name = AttributeTypeName::kRedressNumberName;
      break;
    case EntityTypeName::kOrder:
      primary_attribute_name = AttributeTypeName::kOrderId;
      break;
    case EntityTypeName::kShipment:
      primary_attribute_name = AttributeTypeName::kShipmentTrackingNumber;
      break;
    default:
      NOTREACHED();
  }
  return AddAttribute(AttributeInstance(AttributeType(primary_attribute_name)));
}

EntityInstance EntityInstanceBuilder::Build() {
  CHECK(!attributes_.empty());
  return EntityInstance(type_, std::move(attributes_), guid_,
                        std::move(nickname_), date_modified_, use_count_,
                        use_date_, record_type_data_, are_attributes_read_only_,
                        std::move(frecency_override_));
}

}  // namespace autofill
