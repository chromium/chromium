// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"

#import <memory>
#import <string>

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#import "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_suggestion_controller_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using autofill::FieldRendererId;
using autofill::FormActivityParams;
using autofill::FormRendererId;
using autofill::MockAutofillSuggestionDelegate;
using autofill::SuggestionHidingReason;
using testing::NiceMock;

namespace {

constexpr char kTestUrl[] = "https://foo.com";
constexpr FormRendererId kShowingFormId(1);
constexpr FieldRendererId kShowingFieldId(1);
constexpr FieldRendererId kOtherFieldId(2);

// `FakeWebFramesManager` exposing observer registration for assertions.
class TestWebFramesManager : public web::FakeWebFramesManager {
 public:
  // Returns true if `observer` is registered on this manager.
  bool HasObserver(const Observer* observer) const {
    return observers_.HasObserver(observer);
  }
};

}  // namespace

class AutofillTabHelperTest : public PlatformTest {
 protected:
  explicit AutofillTabHelperTest(bool enable_bam_suggestion_flow = true) {
    if (enable_bam_suggestion_flow) {
      feature_list_.InitAndEnableFeatureWithParameters(
          kAutofillSuggestionPipelineIOS,
          {{kAutofillSuggestionPipelineStageParamName, "1"}});
    } else {
      feature_list_.InitAndDisableFeature(kAutofillSuggestionPipelineIOS);
    }
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = CreateWebState();
  }

  // Returns a realized `FakeWebState` with an `AutofillTabHelper`.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(GURL(kTestUrl));
    for (web::ContentWorld content_world :
         {web::ContentWorld::kPageContentWorld,
          web::ContentWorld::kIsolatedWorld}) {
      web_state->SetWebFramesManager(content_world,
                                     std::make_unique<TestWebFramesManager>());
    }
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->WasShown();
    InfoBarManagerImpl::CreateForWebState(web_state.get());
    AutofillTabHelper::CreateForWebState(web_state.get());
    return web_state;
  }

  // Returns the `ChromeAutofillClientIOS` bound to `web_state`.
  autofill::ChromeAutofillClientIOS& client(web::WebState* web_state) {
    return *AutofillTabHelper::FromWebState(web_state)
                ->chrome_autofill_client();
  }

  // Shows address suggestions on `web_state` for the `kShowingFieldId` field
  // of the main frame.
  void ShowSuggestions(web::WebState* web_state,
                       MockAutofillSuggestionDelegate& delegate) {
    ShowSuggestionsInFrame(web_state, delegate, web::kMainFakeFrameId);
  }

  // Shows address suggestions on `web_state` for the `kShowingFieldId` field
  // in `field_frame_id`, belonging to the form `kShowingFormId` in
  // `form_frame_id`.
  void ShowSuggestionsInFrame(web::WebState* web_state,
                              MockAutofillSuggestionDelegate& delegate,
                              const std::string& field_frame_id,
                              const std::string& form_frame_id) {
    autofill::LocalFrameToken field_frame_token(
        *autofill::DeserializeJavaScriptFrameId(field_frame_id));
    autofill::LocalFrameToken form_frame_token(
        *autofill::DeserializeJavaScriptFrameId(form_frame_id));
    autofill::AutofillClient::PopupOpenArgs open_args;
    open_args.form_id =
        autofill::FormGlobalId(form_frame_token, kShowingFormId);
    open_args.field_id =
        autofill::FieldGlobalId(field_frame_token, kShowingFieldId);
    open_args.suggestions = {autofill::Suggestion(
        u"John Doe", autofill::SuggestionType::kAddressEntry)};
    client(web_state).ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());
    ASSERT_TRUE(client(web_state).suggestion_controller().is_showing());
  }

  // Shows address suggestions on `web_state` for the `kShowingFieldId` field
  // of the form `kShowingFormId`, both hosted in the frame `frame_id`.
  void ShowSuggestionsInFrame(web::WebState* web_state,
                              MockAutofillSuggestionDelegate& delegate,
                              const std::string& frame_id) {
    ShowSuggestionsInFrame(web_state, delegate, frame_id, frame_id);
  }

  // Returns the frames manager for the Autofill content world on `web_state_`.
  TestWebFramesManager* frames_manager() {
    return static_cast<TestWebFramesManager*>(
        autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state_.get()));
  }

  // Registers a form activity of `type` on `field_id` in `frame_id`.
  void RegisterFormActivity(
      FormActivityParams::ActivityType type,
      FieldRendererId field_id,
      const std::string& frame_id = web::kMainFakeFrameId) {
    FormActivityParams params;
    params.type = type;
    params.frame_id = frame_id;
    params.field_renderer_id = field_id;
    std::unique_ptr<web::FakeWebFrame> frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL(kTestUrl));
    autofill::TestFormActivityTabHelper(web_state_.get())
        .FormActivityRegistered(frame.get(), params);
  }

  // Finishes a navigation on `web_state_`.
  void FinishNavigation(bool same_document, bool committed = true) {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetHasCommitted(committed);
    navigation_context.SetIsSameDocument(same_document);
    web_state_->OnNavigationFinished(&navigation_context);
  }

  // Returns true if suggestions are currently showing on `web_state`.
  bool IsShowing(web::WebState* web_state) {
    return client(web_state).suggestion_controller().is_showing();
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  NiceMock<MockAutofillSuggestionDelegate> delegate_;
};

// Tests that focusing another field hides the suggestions as `kFocusChanged`.
TEST_F(AutofillTabHelperTest, FocusOnOtherFieldHidesSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kFocusChanged));
  RegisterFormActivity(FormActivityParams::ActivityType::kFocus, kOtherFieldId);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that refocusing the field holding the suggestions (e.g. after "Use
// Keyboard" on a bottom sheet) keeps them.
TEST_F(AutofillTabHelperTest, FocusOnShowingFieldKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  RegisterFormActivity(FormActivityParams::ActivityType::kFocus,
                       kShowingFieldId);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that blurring the field holding the suggestions (e.g. synthetic blur
// when presenting a bottom sheet) keeps them so dismissing the sheet to the
// keyboard can still render them.
TEST_F(AutofillTabHelperTest, BlurOfShowingFieldKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  RegisterFormActivity(FormActivityParams::ActivityType::kBlur,
                       kShowingFieldId);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that blurring another field keeps the suggestions.
TEST_F(AutofillTabHelperTest, BlurOfOtherFieldKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  RegisterFormActivity(FormActivityParams::ActivityType::kBlur, kOtherFieldId);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that a cross-document navigation hides the suggestions.
TEST_F(AutofillTabHelperTest, CrossDocumentNavigationHidesSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kRendererEvent));
  FinishNavigation(/*same_document=*/false);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that a same-document navigation keeps the suggestions.
TEST_F(AutofillTabHelperTest, SameDocumentNavigationKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  FinishNavigation(/*same_document=*/true);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that an uncommitted navigation keeps the suggestions.
TEST_F(AutofillTabHelperTest, UncommittedNavigationKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  FinishNavigation(/*same_document=*/false, /*committed=*/false);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that an unrelated subframe going away (e.g. an ad iframe navigating
// cross-document or being detached) keeps the main frame suggestions.
TEST_F(AutofillTabHelperTest, UnrelatedFrameUnavailableKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  frames_manager()->RemoveWebFrame(web::kChildFakeFrameId);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that the subframe holding the suggestions going away hides them as
// `kRendererEvent`.
TEST_F(AutofillTabHelperTest, ShowingFrameUnavailableHidesSuggestions) {
  ShowSuggestionsInFrame(web_state_.get(), delegate_, web::kChildFakeFrameId);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kRendererEvent));
  frames_manager()->RemoveWebFrame(web::kChildFakeFrameId);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that another subframe going away keeps the suggestions of the subframe
// holding them.
TEST_F(AutofillTabHelperTest, OtherFrameUnavailableKeepsSubframeSuggestions) {
  ShowSuggestionsInFrame(web_state_.get(), delegate_, web::kChildFakeFrameId);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  frames_manager()->RemoveWebFrame(web::kChildFakeFrameId2);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that hiding the `WebState` hides the suggestions as `kTabGone`.
TEST_F(AutofillTabHelperTest, WasHiddenHidesSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden(SuggestionHidingReason::kTabGone));
  web_state_->WasHidden();

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that hiding one `WebState` doesn't hide the suggestions of another.
TEST_F(AutofillTabHelperTest, HidingOneWebStateKeepsOtherSuggestions) {
  std::unique_ptr<web::FakeWebState> other_web_state = CreateWebState();
  NiceMock<MockAutofillSuggestionDelegate> other_delegate;
  ShowSuggestions(web_state_.get(), delegate_);
  ShowSuggestions(other_web_state.get(), other_delegate);

  EXPECT_CALL(other_delegate, OnSuggestionsHidden).Times(0);
  web_state_->WasHidden();

  EXPECT_FALSE(IsShowing(web_state_.get()));
  EXPECT_TRUE(IsShowing(other_web_state.get()));
}

// Tests that focusing a field with the same `FieldRendererId` in a different
// frame hides the showing suggestions as `kFocusChanged`.
TEST_F(AutofillTabHelperTest,
       FocusOnSameRendererIdInOtherFrameHidesSuggestions) {
  ASSERT_TRUE(frames_manager()->HasObserver(
      AutofillTabHelper::FromWebState(web_state_.get())));
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kFocusChanged));
  RegisterFormActivity(FormActivityParams::ActivityType::kFocus,
                       kShowingFieldId, web::kChildFakeFrameId);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that when a child-frame field belongs to a main-frame form, the host
// form's frame going away hides the suggestions as `kRendererEvent`.
TEST_F(AutofillTabHelperTest,
       HostFormFrameUnavailableHidesCrossFrameSuggestions) {
  ShowSuggestionsInFrame(web_state_.get(), delegate_,
                         /*field_frame_id=*/web::kChildFakeFrameId,
                         /*form_frame_id=*/web::kMainFakeFrameId);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kRendererEvent));
  frames_manager()->RemoveWebFrame(web::kMainFakeFrameId);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that focusing a field with an unparseable `frame_id` hides the showing
// suggestions as `kFocusChanged`.
TEST_F(AutofillTabHelperTest, FocusWithInvalidFrameIdHidesSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kFocusChanged));
  RegisterFormActivity(FormActivityParams::ActivityType::kFocus,
                       kShowingFieldId, web::kInvalidFrameId);

  EXPECT_FALSE(IsShowing(web_state_.get()));
}

// Tests that an unparseable `frame_id` becoming unavailable keeps the showing
// suggestions.
TEST_F(AutofillTabHelperTest, InvalidFrameUnavailableKeepsSuggestions) {
  ShowSuggestions(web_state_.get(), delegate_);

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  frames_manager()->RemoveWebFrame(web::kInvalidFrameId);

  EXPECT_TRUE(IsShowing(web_state_.get()));
}

class AutofillTabHelperPipelineDisabledTest : public AutofillTabHelperTest {
 protected:
  AutofillTabHelperPipelineDisabledTest()
      : AutofillTabHelperTest(/*enable_bam_suggestion_flow=*/false) {}
};

// Tests that when `kAutofillSuggestionPipelineIOS` is disabled, frame
// observers are not registered and lifecycle events do not hide suggestions.
TEST_F(AutofillTabHelperPipelineDisabledTest,
       LifecycleEventsIgnoredWhenDisabled) {
  EXPECT_FALSE(frames_manager()->HasObserver(
      AutofillTabHelper::FromWebState(web_state_.get())));

  autofill::LocalFrameToken main_frame_token(
      *autofill::DeserializeJavaScriptFrameId(web::kMainFakeFrameId));
  autofill::AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = autofill::FormGlobalId(main_frame_token, kShowingFormId);
  open_args.field_id =
      autofill::FieldGlobalId(main_frame_token, kShowingFieldId);
  open_args.suggestions = {autofill::Suggestion(
      u"John Doe", autofill::SuggestionType::kAddressEntry)};

  client(web_state_.get())
      .ShowAutofillSuggestions(open_args, delegate_.GetWeakPtr());
  ASSERT_TRUE(client(web_state_.get())
                  .GetSessionIdForCurrentAutofillSuggestions()
                  .has_value());
  ASSERT_FALSE(IsShowing(web_state_.get()));

  // Seed `suggestion_controller()` directly as well to verify none of the
  // lifecycle events clear it when the feature is disabled.
  client(web_state_.get())
      .suggestion_controller()
      .Show(
          open_args.suggestions,
          autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
          delegate_.GetWeakPtr(), open_args.form_id, open_args.field_id);
  ASSERT_TRUE(IsShowing(web_state_.get()));

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  RegisterFormActivity(FormActivityParams::ActivityType::kFocus, kOtherFieldId);
  frames_manager()->RemoveWebFrame(web::kMainFakeFrameId);
  FinishNavigation(/*same_document=*/false);
  web_state_->WasHidden();

  EXPECT_TRUE(client(web_state_.get())
                  .GetSessionIdForCurrentAutofillSuggestions()
                  .has_value());
  EXPECT_TRUE(IsShowing(web_state_.get()));
}

// Tests that invoking the `AutofillTabHelper` observer callbacks directly when
// `kAutofillSuggestionPipelineIOS` is disabled is a no-op.
TEST_F(AutofillTabHelperPipelineDisabledTest,
       ObserverCallbacksNoOpWhenDisabled) {
  autofill::LocalFrameToken main_frame_token(
      *autofill::DeserializeJavaScriptFrameId(web::kMainFakeFrameId));
  client(web_state_.get())
      .suggestion_controller()
      .Show(
          {autofill::Suggestion(u"John Doe",
                                autofill::SuggestionType::kAddressEntry)},
          autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
          delegate_.GetWeakPtr(),
          autofill::FormGlobalId(main_frame_token, kShowingFormId),
          autofill::FieldGlobalId(main_frame_token, kShowingFieldId));
  ASSERT_TRUE(IsShowing(web_state_.get()));

  AutofillTabHelper* tab_helper =
      AutofillTabHelper::FromWebState(web_state_.get());
  ASSERT_TRUE(tab_helper);

  FormActivityParams focus_params;
  focus_params.type = FormActivityParams::ActivityType::kFocus;
  focus_params.frame_id = web::kMainFakeFrameId;
  focus_params.field_renderer_id = kOtherFieldId;
  std::unique_ptr<web::FakeWebFrame> frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL(kTestUrl));

  EXPECT_CALL(delegate_, OnSuggestionsHidden).Times(0);
  static_cast<autofill::FormActivityObserver*>(tab_helper)
      ->FormActivityRegistered(web_state_.get(), frame.get(), focus_params);
  static_cast<web::WebFramesManager::Observer*>(tab_helper)
      ->WebFrameBecameUnavailable(frames_manager(), web::kMainFakeFrameId);
  FinishNavigation(/*same_document=*/false);
  web_state_->WasHidden();

  EXPECT_TRUE(IsShowing(web_state_.get()));
}
