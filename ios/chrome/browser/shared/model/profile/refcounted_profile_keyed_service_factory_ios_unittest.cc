// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// A dummy RefcountedKeyedService.
class DummyService final : public RefcountedKeyedService {
 public:
  // RefcountedKeyedService:
  void ShutdownOnUIThread() final {}

 private:
  // Destructor needs to be explicitly declared and private to please the
  // Chromium style checker as this class is ref-counted.
  ~DummyService() final = default;
};

// A factory for DummyService which allow setting all the traits via the
// constructor (thus allowing the tests to use different traits with the same
// factory).
class DummyServiceFactory final
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  template <typename... Traits>
  DummyServiceFactory(Traits... traits)
      : RefcountedProfileKeyedServiceFactoryIOS(
            "DummyService",
            std::forward<Traits>(traits)...) {}

  scoped_refptr<DummyService> GetForProfile(ProfileIOS* profile) {
    return GetServiceForProfileAs<DummyService>(profile, true);
  }

  scoped_refptr<DummyService> GetForProfileIfExists(ProfileIOS* profile) {
    return GetServiceForProfileAs<DummyService>(profile, false);
  }

  // RefcountedBrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final {
    return base::MakeRefCounted<DummyService>();
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

class RefcountedProfileKeyedServiceFactoryIOSTest
    : public testing::TestWithParam<Param> {
 public:
  RefcountedProfileKeyedServiceFactoryIOSTest()
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
    RefcountedProfileKeyedServiceFactoryIOSTest,
    ::testing::Combine(
        ::testing::Values(ProfileSelection::kNoInstanceInIncognito,
                          ProfileSelection::kRedirectedInIncognito,
                          ProfileSelection::kOwnInstanceInIncognito),
        ::testing::Values(ServiceCreation::kCreateLazily,
                          ServiceCreation::kCreateWithProfile),
        ::testing::Values(TestingCreation::kCreateService,
                          TestingCreation::kNoServiceForTests)),
    PrintToStringParamName());

// Tests that RefcountedProfileKeyedServiceFactoryIOS behaves correctly for
// regular profiles.
TEST_P(RefcountedProfileKeyedServiceFactoryIOSTest, RegularProfile) {
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

// Tests that RefcountedProfileKeyedServiceFactoryIOS behaves correctly for
// off-the-record profiles.
TEST_P(RefcountedProfileKeyedServiceFactoryIOSTest, OffTheRecordProfile) {
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
  scoped_refptr<DummyService> regular_service =
      factory().GetForProfile(GetRegularProfile());
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
