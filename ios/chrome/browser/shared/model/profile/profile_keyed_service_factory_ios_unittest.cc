// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// A dummy KeyedService.
class DummyService final : public KeyedService {
 public:
  // KeyedService:
  void Shutdown() final {}
};

// A factory for DummyService which allow setting all the traits via the
// constructor (thus allowing the tests to use different traits with the same
// factory).
class DummyServiceFactory final : public ProfileKeyedServiceFactoryIOS {
 public:
  template <typename... Traits>
  DummyServiceFactory(Traits... traits)
      : ProfileKeyedServiceFactoryIOS("DummyService",
                                      std::forward<Traits>(traits)...) {}

  DummyService* GetForProfile(ProfileIOS* profile) {
    return GetServiceForProfileAs<DummyService>(profile, true);
  }

  DummyService* GetForProfileIfExists(ProfileIOS* profile) {
    return GetServiceForProfileAs<DummyService>(profile, false);
  }

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final {
    return std::make_unique<DummyService>();
  }
};

// Parameter for ProfileKeyedServiceFactoryIOSTest.
using Param = std::tuple<ProfileSelection, ServiceCreation, TestingCreation>;

// Used to generate test name from Param.
struct PrintToStringParamName {
  std::string operator()(const testing::TestParamInfo<Param>& info) const {
    std::string_view profile_selection;
    switch (std::get<ProfileSelection>(info.param)) {
      case ProfileSelection::kNoInstanceInIncognito:
        profile_selection = "NoInstanceInIncognito";
        break;

      case ProfileSelection::kRedirectedInIncognito:
        profile_selection = "RedirectedInIncognito";
        break;

      case ProfileSelection::kOwnInstanceInIncognito:
        profile_selection = "OwnInstanceInIncognito";
        break;
    }

    std::string_view service_creation;
    switch (std::get<ServiceCreation>(info.param)) {
      case ServiceCreation::kCreateLazily:
        service_creation = "CreateLazily";
        break;

      case ServiceCreation::kCreateWithProfile:
        service_creation = "CreateWithProfile";
        break;
    }

    std::string_view testing_creation;
    switch (std::get<TestingCreation>(info.param)) {
      case TestingCreation::kCreateService:
        testing_creation = "CreateService";
        break;

      case TestingCreation::kNoServiceForTests:
        testing_creation = "NoServiceForTests";
        break;
    }

    return base::JoinString(
        {profile_selection, service_creation, testing_creation}, "_");
  }
};

}  // namespace

class ProfileKeyedServiceFactoryIOSTest : public testing::TestWithParam<Param> {
 public:
  ProfileKeyedServiceFactoryIOSTest()
      : dummy_service_factory_(std::get<ProfileSelection>(GetParam()),
                               std::get<ServiceCreation>(GetParam()),
                               std::get<TestingCreation>(GetParam())) {
    test_profile_ = TestProfileIOS::Builder().Build();
    test_profile_->CreateOffTheRecordBrowserStateWithTestingFactories();
  }

  DummyServiceFactory& factory() { return dummy_service_factory_; }

  ProfileIOS* GetRegularProfile() { return test_profile_.get(); }

  ProfileIOS* GetOffTheRecordProfile() {
    return test_profile_->GetOffTheRecordProfile();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DummyServiceFactory dummy_service_factory_;
  std::unique_ptr<TestProfileIOS> test_profile_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileKeyedServiceFactoryIOSTest,
    ::testing::Combine(
        ::testing::Values(ProfileSelection::kNoInstanceInIncognito,
                          ProfileSelection::kRedirectedInIncognito,
                          ProfileSelection::kOwnInstanceInIncognito),
        ::testing::Values(ServiceCreation::kCreateLazily,
                          ServiceCreation::kCreateWithProfile),
        ::testing::Values(TestingCreation::kCreateService,
                          TestingCreation::kNoServiceForTests)),
    PrintToStringParamName());

// Tests that ProfileKeyedServiceFactoryIOS behaves correctly for regular
// profiles.
TEST_P(ProfileKeyedServiceFactoryIOSTest, RegularProfile) {
  ProfileIOS* profile = GetRegularProfile();

  // If the service should not be created during testing, check it is not
  // created and then terminate the test successfully.
  switch (std::get<TestingCreation>(GetParam())) {
    case TestingCreation::kCreateService:
      break;

    case TestingCreation::kNoServiceForTests:
      EXPECT_EQ(nullptr, factory().GetForProfile(profile));
      return;
  }

  // If the service should be created with the profile, check it has been
  // already been created without calling GetForProfile(...).
  switch (std::get<ServiceCreation>(GetParam())) {
    case ServiceCreation::kCreateLazily:
      EXPECT_EQ(nullptr, factory().GetForProfileIfExists(profile));
      break;

    case ServiceCreation::kCreateWithProfile:
      EXPECT_NE(nullptr, factory().GetForProfileIfExists(profile));
      break;
  }

  // Check that calling GetForProfile(...) unconditionally create the service.
  EXPECT_NE(nullptr, factory().GetForProfile(profile));
}

// Tests that ProfileKeyedServiceFactoryIOS behaves correctly for off-the-record
// profiles.
TEST_P(ProfileKeyedServiceFactoryIOSTest, OffTheRecordProfile) {
  ProfileIOS* otr_profile = GetOffTheRecordProfile();

  // If the service should not be created during testing, check it is not
  // created and then terminate the test successfully.
  switch (std::get<TestingCreation>(GetParam())) {
    case TestingCreation::kCreateService:
      break;

    case TestingCreation::kNoServiceForTests:
      EXPECT_EQ(nullptr, factory().GetForProfile(otr_profile));
      return;
  }

  // If the service should not be created for off-the-record profiles, check it
  // is not created and then terminate the test successfully.
  switch (std::get<ProfileSelection>(GetParam())) {
    case ProfileSelection::kNoInstanceInIncognito:
      EXPECT_EQ(nullptr, factory().GetForProfile(otr_profile));
      return;

    case ProfileSelection::kRedirectedInIncognito:
    case ProfileSelection::kOwnInstanceInIncognito:
      break;
  }

  // If the service should be created with the profile, check it has been
  // already been created without calling GetForProfile(...).
  switch (std::get<ServiceCreation>(GetParam())) {
    case ServiceCreation::kCreateLazily:
      EXPECT_EQ(nullptr, factory().GetForProfileIfExists(otr_profile));
      break;

    case ServiceCreation::kCreateWithProfile:
      EXPECT_NE(nullptr, factory().GetForProfileIfExists(otr_profile));
      break;
  }

  // Check that calling GetForProfile(...) unconditionally create the service.
  EXPECT_NE(nullptr, factory().GetForProfile(otr_profile));

  // Get the service for the regular profile to compare it to the service of the
  // off-the-record profile.
  DummyService* regular_service = factory().GetForProfile(GetRegularProfile());
  ASSERT_NE(nullptr, regular_service);

  // Check that the regular and off-the-record profile have either the same or a
  // different service depending on the ProfileSelection value.
  switch (std::get<ProfileSelection>(GetParam())) {
    case ProfileSelection::kNoInstanceInIncognito:
      NOTREACHED();

    case ProfileSelection::kRedirectedInIncognito:
      EXPECT_EQ(regular_service, factory().GetForProfile(otr_profile));
      break;

    case ProfileSelection::kOwnInstanceInIncognito:
      EXPECT_NE(regular_service, factory().GetForProfile(otr_profile));
      break;
  }
}
