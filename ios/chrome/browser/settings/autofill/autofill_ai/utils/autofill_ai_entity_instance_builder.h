// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_ENTITY_INSTANCE_BUILDER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_ENTITY_INSTANCE_BUILDER_H_

#import <string>
#import <vector>

#import "base/containers/flat_set.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

// Builder for EntityInstance. For detailed information about the fields, see
// `EntityInstance`.
class EntityInstanceBuilder {
 public:
  explicit EntityInstanceBuilder(EntityType type);
  ~EntityInstanceBuilder();

  EntityInstanceBuilder& SetGUID(EntityInstance::EntityId guid);
  EntityInstanceBuilder& SetNickname(std::string nickname);
  EntityInstanceBuilder& SetDateModified(base::Time date_modified);
  EntityInstanceBuilder& SetUseCount(int64_t use_count);
  EntityInstanceBuilder& SetUseDate(base::Time use_date);
  EntityInstanceBuilder& SetRecordType(EntityInstance::RecordType record_type);
  EntityInstanceBuilder& SetAreAttributesReadOnly(
      EntityInstance::AreAttributesReadOnly are_attributes_read_only);
  EntityInstanceBuilder& SetFrecencyOverride(std::string frecency_override);

  EntityInstanceBuilder& AddAttribute(AttributeInstance attribute);
  EntityInstanceBuilder& AddPrimaryAttribute();

  // Returns the final EntityInstance. The builder is not usable after this
  // call.
  EntityInstance Build();

 private:
  EntityType type_;
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes_;
  EntityInstance::EntityId guid_;
  std::string nickname_;
  base::Time date_modified_;
  int64_t use_count_ = 0;
  base::Time use_date_;
  EntityInstance::RecordType record_type_ = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only_ =
      EntityInstance::AreAttributesReadOnly(false);
  std::string frecency_override_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UTILS_AUTOFILL_AI_ENTITY_INSTANCE_BUILDER_H_
