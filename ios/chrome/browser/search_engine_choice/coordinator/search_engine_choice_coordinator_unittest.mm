// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/command_line.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_mediator.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_constants.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_view_controller.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

// Empty implementation of the FirstRunScreenDelegate used to check that the
// screen does get dismissed.
@interface FirstRunScreenTestDelegate : NSObject <FirstRunScreenDelegate>

@property(nonatomic, readonly) BOOL wasDismissed;

@end

@implementation FirstRunScreenTestDelegate

@synthesize wasDismissed = _wasDismissed;

- (void)screenWillFinishPresenting {
  _wasDismissed = YES;
}

@end

// Empty implementation of the SearchEngineChoiceCoordinatorDelegate used to
// check that the screen gets dismissed.
@interface SearchEngineChoiceCoordinatorTestDelegate
    : NSObject <SearchEngineChoiceCoordinatorDelegate>

@property(nonatomic, readonly) BOOL wasDismissed;

@end

@implementation SearchEngineChoiceCoordinatorTestDelegate

@synthesize wasDismissed = _wasDismissed;

- (void)choiceScreenWasDismissed:(SearchEngineChoiceCoordinator*)coordinator {
  _wasDismissed = YES;
}

@end

class SearchEngineChoiceCoordinatorTest : public PlatformTest {
 protected:
  SearchEngineChoiceCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;

    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.SetPolicyConnector(
        std::make_unique<ProfilePolicyConnectorMock>(CreateMockPolicyService(),
                                                     &schema_registry_));
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceSearchEngineChoiceScreen);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");
  }

  ~SearchEngineChoiceCoordinatorTest() override {
    coordinator_ = nil;
    EXPECT_OCMOCK_VERIFY((id)search_engine_choice_view_controller_mock_);
    EXPECT_OCMOCK_VERIFY((id)search_engine_choice_mediator_mock_);
  }

  // Creates PolicyService mock.
  std::unique_ptr<policy::MockPolicyService> CreateMockPolicyService() {
    auto policy_service = std::make_unique<policy::MockPolicyService>();

    EXPECT_CALL(*policy_service.get(),
                GetPolicies(::testing::Eq(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string()))))
        .Times(::testing::AnyNumber());
    ON_CALL(*policy_service.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));

    return policy_service;
  }

  // Creates SearchEngineChoiceViewController mock. This mock is used by
  // `SearchEngineChoiceCoordinator`.
  void CreateViewControllerMock() {
    id view_controller_class_mock =
        OCMStrictClassMock([SearchEngineChoiceViewController class]);
    search_engine_choice_view_controller_mock_ = view_controller_class_mock;
    OCMExpect([view_controller_class_mock alloc])
        .andReturn(search_engine_choice_view_controller_mock_);
    OCMExpect([search_engine_choice_view_controller_mock_
                  initWithFirstRunMode:first_run_])
        .andReturn(search_engine_choice_view_controller_mock_);
    OCMStub(
        [search_engine_choice_view_controller_mock_ presentingViewController])
        .andReturn(base_view_controller_);
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE &&
        !first_run_) {
      OCMExpect([search_engine_choice_view_controller_mock_
          setModalPresentationStyle:UIModalPresentationFormSheet]);
      CGSize size = CGSizeMake(kIPadSearchEngineChoiceScreenPreferredWidth,
                               kIPadSearchEngineChoiceScreenPreferredHeight);
      OCMExpect([search_engine_choice_view_controller_mock_
          setPreferredContentSize:size]);
    }
  }

  // Creates SearchEngineChoiceMediator mock. This mock is used by
  // `SearchEngineChoiceCoordinator`.
  void CreateMediatorMock() {
    id search_engine_choice_mediator_class_mock =
        OCMStrictClassMock([SearchEngineChoiceMediator class]);
    search_engine_choice_mediator_mock_ =
        search_engine_choice_mediator_class_mock;
    OCMExpect([search_engine_choice_mediator_class_mock alloc])
        .andReturn(search_engine_choice_mediator_mock_);
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    search_engines::SearchEngineChoiceService* search_engine_choice_service =
        ios::SearchEngineChoiceServiceFactory::GetForProfile(profile_.get());
    regional_capabilities::RegionalCapabilitiesService*
        regional_capabilities_service =
            ios::RegionalCapabilitiesServiceFactory::GetForProfile(
                profile_.get());
    OCMExpect([search_engine_choice_mediator_mock_
                   initWithTemplateURLService:template_url_service
                    searchEngineChoiceService:search_engine_choice_service
                  regionalCapabilitiesService:regional_capabilities_service])
        .andReturn(search_engine_choice_mediator_mock_);
  }

  // Creates SearchEngineChoiceCoordinator instance with all the required mocks,
  // for the non FRE case. And starts the coordinator.
  void CreateAndStartCoordinatorWithoutFre() {
    first_run_ = false;
    base_view_controller_ = [[FakeUIViewController alloc] init];
    coordinator_ = [[SearchEngineChoiceCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    CreateViewControllerMock();
    CreateMediatorMock();
    StartCoordinator();
  }

  // Creates SearchEngineChoiceCoordinator instance with all the required mocks,
  // for the FRE case. And starts the coordinator.
  void CreateAndStartCoordinatorForFre() {
    first_run_ = true;
    UINavigationController* base_navigation_controller =
        [[FakeUINavigationController alloc] init];
    first_run_delegate_ = [[FirstRunScreenTestDelegate alloc] init];
    coordinator_ = [[SearchEngineChoiceCoordinator alloc]
        initForFirstRunWithBaseNavigationController:base_navigation_controller
                                            browser:browser_.get()
                                   firstRunDelegate:first_run_delegate_];

    CreateViewControllerMock();
    CreateMediatorMock();
    StartCoordinator();
  }

  // Starts `coordinator_`. The view controller, mediator and coordinator must
  // have been created before calling this method.
  void StartCoordinator() {
    coordinator_delegate_ =
        [[SearchEngineChoiceCoordinatorTestDelegate alloc] init];
    coordinator_.delegate = coordinator_delegate_;
    OCMExpect([search_engine_choice_view_controller_mock_
        setActionDelegate:AssignValueToVariable(
                              search_engine_choice_action_delegate_)]);
    OCMExpect([search_engine_choice_mediator_mock_
        setConsumer:search_engine_choice_view_controller_mock_]);
    OCMExpect([search_engine_choice_view_controller_mock_
        setMutator:search_engine_choice_mediator_mock_]);
    OCMExpect([search_engine_choice_view_controller_mock_
        setModalInPresentation:YES]);
    [coordinator_ start];
    ExpectNotDismissed();
  }

  // Expects the view was not dismissed.
  void ExpectNotDismissed() {
    EXPECT_FALSE(first_run_delegate_.wasDismissed);
    EXPECT_FALSE(coordinator_delegate_.wasDismissed);
  }

  // Simulates the user selecting a search engine and tapping on the primary
  // button.
  void SelectAndSetAsDefault() {
    OCMExpect([search_engine_choice_mediator_mock_ saveDefaultSearchEngine]);
    ExpectViewControllerDismissed();
    [search_engine_choice_action_delegate_ didTapPrimaryButton];
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    if (first_run_) {
      EXPECT_TRUE(first_run_delegate_.wasDismissed);
      EXPECT_FALSE(coordinator_delegate_.wasDismissed);
    } else {
      EXPECT_FALSE(first_run_delegate_.wasDismissed);
      EXPECT_TRUE(coordinator_delegate_.wasDismissed);
    }
  }

  // Expect the view controller to be dismissed.
  void ExpectViewControllerDismissed() {
    OCMExpect([search_engine_choice_view_controller_mock_ setMutator:nil]);
  }

  void StopCoordinator(bool view_controller_already_dismissed) {
    if (!view_controller_already_dismissed) {
      ExpectViewControllerDismissed();
    }
    OCMExpect([search_engine_choice_mediator_mock_ disconnect]);
    OCMExpect([search_engine_choice_mediator_mock_ setConsumer:nil]);
    [coordinator_ stop];
    coordinator_ = nil;
  }

  // Simulates the user openning the Learn-More dialog.
  void ShowLearnMore() {
    OCMExpect([search_engine_choice_view_controller_mock_
        presentViewController:[OCMArg any]
                     animated:YES
                   completion:nil]);
    [search_engine_choice_action_delegate_ showLearnMore];
  }

  void ExpectChoiceScreenWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed,
        1);
  }

  void ExpectFreChoiceScreenWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kFreChoiceScreenWasDisplayed,
        1);
  }

  void ExpectDefaultWasSetRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);
  }

  void ExpectFreDefaultWasSetRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);
  }

  void ExpectLearnMoreWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
        1);
  }

  void ExpectFreLearnMoreWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kFreLearnMoreWasDisplayed,
        1);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  policy::SchemaRegistry schema_registry_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;

  policy::PolicyMap policy_map_;
  SearchEngineChoiceViewController* search_engine_choice_view_controller_mock_ =
      nil;
  SearchEngineChoiceMediator* search_engine_choice_mediator_mock_ = nil;
  id<SearchEngineChoiceActionDelegate> search_engine_choice_action_delegate_ =
      nil;
  bool first_run_ = false;
  SearchEngineChoiceCoordinator* coordinator_ = nil;
  UIViewController* base_view_controller_ = nil;
  base::HistogramTester histogram_tester_;
  FirstRunScreenTestDelegate* first_run_delegate_ = nil;
  SearchEngineChoiceCoordinatorTestDelegate* coordinator_delegate_ = nil;
};

// Checks that the correct metrics are recorded and that the screen gets
// dismissed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestHistograms) {
  CreateAndStartCoordinatorWithoutFre();
  ExpectChoiceScreenWasDisplayedRecorded();

  SelectAndSetAsDefault();
  ExpectDefaultWasSetRecorded();

  StopCoordinator(/*view_controller_already_dismissed=*/true);
}

// Checks that the correct metrics are recorded when the Learn More screen is
// displayed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestLearnMoreRecorded) {
  CreateAndStartCoordinatorWithoutFre();

  ShowLearnMore();
  ExpectLearnMoreWasDisplayedRecorded();

  StopCoordinator(/*view_controller_already_dismissed=*/false);
}

// Checks that the correct metrics are recorded for the FRE and that the screen
// is dismissed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestFreHistograms) {
  CreateAndStartCoordinatorForFre();
  ExpectFreChoiceScreenWasDisplayedRecorded();

  SelectAndSetAsDefault();
  ExpectFreDefaultWasSetRecorded();
  StopCoordinator(/*view_controller_already_dismissed=*/true);
}

// Checks that the correct metrics are recorded in the FRE when the Learn More
// screen is displayed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestFreLearnMoreRecorded) {
  CreateAndStartCoordinatorForFre();

  ShowLearnMore();
  ExpectFreLearnMoreWasDisplayedRecorded();
  StopCoordinator(/*view_controller_already_dismissed=*/false);
}
