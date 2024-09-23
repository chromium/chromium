// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Tests the SetUpListView and subviews.
class VirtualCardEnrollmentBottomSheetCoordinatorTest : public PlatformTest {
 public:
  VirtualCardEnrollmentBottomSheetCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Create a FakeWebState.
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());

    // Create a FakeWebFramesmanager with AutofillBottomSheetJavaScriptFeature.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    fake_web_state->SetWebFramesManager(content_world,
                                        std::move(frames_manager));

    // Insert and activete FakeWebState.
    int web_state_index =
        browser_->GetWebStateList()->InsertWebState(std::move(fake_web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(web_state_index);
    web::WebState* web_state =
        browser_->GetWebStateList()->GetWebStateAt(web_state_index);

    // Create the tab helper that will hold the callbacks.
    AutofillBottomSheetTabHelper::CreateForWebState(web_state);
    AutofillBottomSheetTabHelper::FromWebState(web_state)
        ->ShowVirtualCardEnrollmentBottomSheet(
            std::make_unique<autofill::VirtualCardEnrollUiModel>(
                autofill::VirtualCardEnrollmentFields()),
            autofill::VirtualCardEnrollmentCallbacks(
                /*accept_callback=*/base::BindOnce(
                    &VirtualCardEnrollmentBottomSheetCoordinatorTest::
                        OnAcceptVirtualCard,
                    weak_factory_.GetWeakPtr()),
                /*cancel_callback=*/base::BindOnce(
                    &VirtualCardEnrollmentBottomSheetCoordinatorTest::
                        OnDeclineVirtualCard,
                    weak_factory_.GetWeakPtr())));

    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    autofill::VirtualCardEnrollmentFields enrollment_fields;
    enrollment_fields.virtual_card_enrollment_source =
        autofill::VirtualCardEnrollmentSource::kDownstream;
    std::unique_ptr<autofill::VirtualCardEnrollUiModel> model =
        std::make_unique<autofill::VirtualCardEnrollUiModel>(enrollment_fields);

    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    id<BrowserCoordinatorCommands> browserCoordinatorCommands =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:browserCoordinatorCommands
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    coordinator_ = [[VirtualCardEnrollmentBottomSheetCoordinator alloc]
           initWithUIModel:std::move(model)
        baseViewController:window_.rootViewController
                   browser:browser_.get()];
  }

 protected:
  void OnAcceptVirtualCard() { times_accept_virtual_card_called_++; }
  void OnDeclineVirtualCard() { times_decline_virtual_card_called_++; }

  int times_accept_virtual_card_called_ = 0;
  int times_decline_virtual_card_called_ = 0;
  base::HistogramTester histogram_tester_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id<ApplicationCommands> application_handler_;
  UIWindow* window_;
  VirtualCardEnrollmentBottomSheetCoordinator* coordinator_;
  base::WeakPtrFactory<VirtualCardEnrollmentBottomSheetCoordinatorTest>
      weak_factory_{this};
};

#pragma mark - Tests

TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, OpensNewTabForLinks) {
  [coordinator_ start];
  CrURL* url = [[CrURL alloc]
      initWithNSURL:[NSURL URLWithString:@"https://example.test"]];

  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return command.URL == url.gurl;
      }]]);

  [coordinator_ didTapLinkURL:url text:nil];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}
