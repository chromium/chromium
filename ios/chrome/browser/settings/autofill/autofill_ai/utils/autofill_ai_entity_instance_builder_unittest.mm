// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {
namespace {

class EntityInstanceBuilderTest : public PlatformTest {
 public:
  EntityInstanceBuilderTest() {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillAiCreateEntityDataManager},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
  }

 protected:
  // Verifies that an entity instance can be saved to and fetched from the
  // EntityDataManager.
  void VerifySaveAndFetch(const EntityInstance& instance) {
    autofill::EntityDataManager* entity_data_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    entity_data_manager->AddOrUpdateEntityInstance(instance);

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, true, ^{
          return entity_data_manager->GetEntityInstance(instance.guid())
              .has_value();
        }));

    auto fetched_instance =
        entity_data_manager->GetEntityInstance(instance.guid());
    EXPECT_TRUE(fetched_instance.has_value());
    EXPECT_EQ(fetched_instance->guid(), instance.guid());
    EXPECT_EQ(fetched_instance->type().name(), instance.type().name());
    EXPECT_EQ(fetched_instance->attributes().size(),
              instance.attributes().size());
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verify that a Passport entity is built with the Passport Name primary
// attribute.
TEST_F(EntityInstanceBuilderTest, BuildPassportWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kPassport))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kPassport);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(
      instance.attribute(AttributeType(AttributeTypeName::kPassportName))
          .has_value());

  VerifySaveAndFetch(instance);
}

// Verify that a Driver's License entity is built with the Driver's License Name
// primary attribute.
TEST_F(EntityInstanceBuilderTest, BuildDriversLicenseWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kDriversLicense))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kDriversLicense);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(
      instance.attribute(AttributeType(AttributeTypeName::kDriversLicenseName))
          .has_value());

  VerifySaveAndFetch(instance);
}

// Verify that a National ID Card entity is built with the National ID Card Name
// primary attribute.
TEST_F(EntityInstanceBuilderTest, BuildNationalIdCardWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kNationalIdCard))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kNationalIdCard);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(
      instance.attribute(AttributeType(AttributeTypeName::kNationalIdCardName))
          .has_value());

  VerifySaveAndFetch(instance);
}

// Verify that a Vehicle entity is built with the Vehicle Make primary
// attribute.
TEST_F(EntityInstanceBuilderTest, BuildVehicleWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kVehicle))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kVehicle);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(instance.attribute(AttributeType(AttributeTypeName::kVehicleMake))
                  .has_value());

  VerifySaveAndFetch(instance);
}

// Verify that a Known Traveler Number entity is built with the Known Traveler
// Number Name primary attribute.
TEST_F(EntityInstanceBuilderTest,
       BuildKnownTravelerNumberWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kKnownTravelerNumber))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kKnownTravelerNumber);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(
      instance
          .attribute(AttributeType(AttributeTypeName::kKnownTravelerNumberName))
          .has_value());

  VerifySaveAndFetch(instance);
}

// Verify that a Redress Number entity is built with the Redress Number Name
// primary attribute.
TEST_F(EntityInstanceBuilderTest, BuildRedressNumberWithPrimaryAttribute) {
  EntityInstance instance =
      EntityInstanceBuilder(EntityType(EntityTypeName::kRedressNumber))
          .AddPrimaryAttribute()
          .Build();

  EXPECT_EQ(instance.type().name(), EntityTypeName::kRedressNumber);
  EXPECT_EQ(instance.attributes().size(), 1u);
  EXPECT_TRUE(
      instance.attribute(AttributeType(AttributeTypeName::kRedressNumberName))
          .has_value());

  VerifySaveAndFetch(instance);
}

}  // namespace
}  // namespace autofill
