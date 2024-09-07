// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_metrics_recorder.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/metrics/public/cpp/ukm_recorder.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

using password_manager::features_util::PasswordAccountStorageUserState::
    kSignedInAccountStoreUser;

NSString* const kUsernameValue = @"user1";
NSString* const kPasswordValue = @"pass1";
constexpr char kUrl[] = "https://example.com/path";
constexpr char kAccountToStorePassword[] = "account_to_store_password";
constexpr int kNavEntryId = 10;

}  // namespace

// Test fixture for IOSChromeSavePasswordInfoBarDelegateTest.
class IOSChromeSavePasswordInfoBarDelegateTest : public PlatformTest {
 public:
  void SetUp() override { InitializeDelegate(/*password_update=*/false); }

 protected:
  // Initializes that password infobar delegate and surrounding objects. Can be
  // called again in-test to reinitialize the delegate with different
  // parameters.
  void InitializeDelegate(bool password_update) {
    url_ = GURL(kUrl);

    auto form_manager =
        std::make_unique<password_manager::MockPasswordFormManagerForUI>();
    form_manager_ptr_ = form_manager.get();

    form_.url = url_;
    form_.signon_realm = "https://example.com/";
    form_.username_value = base::SysNSStringToUTF16(kUsernameValue);
    form_.password_value = base::SysNSStringToUTF16(kPasswordValue);
    form_.scheme = password_manager::PasswordForm::Scheme::kHtml;
    form_.type = password_manager::PasswordForm::Type::kApi;

    ON_CALL(testing::Const(*form_manager), GetPendingCredentials)
        .WillByDefault(testing::ReturnRef(form_));
    ON_CALL(testing::Const(*form_manager), GetURL)
        .WillByDefault(testing::ReturnRef(url_));
    ON_CALL(testing::Const(*form_manager), GetCredentialSource)
        .WillByDefault(
            testing::Return(password_manager::metrics_util::
                                CredentialSourceType::kPasswordManager));

    ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
    metrics_recorder_ =
        base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
            /*is_main_frame_secure=*/true, ukm_source_id_,
            /*pref_service=*/nullptr);

    ON_CALL(testing::Const(*form_manager), GetMetricsRecorder)
        .WillByDefault(testing::Return(metrics_recorder_.get()));

    delegate_ = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
        kAccountToStorePassword, password_update, kSignedInAccountStoreUser,
        std::move(form_manager),
        /*dispatcher=*/nullptr);
    const int different_nav_entry_id = kNavEntryId - 1;
    delegate_->set_nav_entry_id(different_nav_entry_id);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Password form metrics recorder.
  scoped_refptr<password_manager::PasswordFormMetricsRecorder>
      metrics_recorder_;
  // UKM source id.
  ukm::SourceId ukm_source_id_;
  // Form to save/update the credentials for.
  password_manager::PasswordForm form_;
  // URL of the page showing the infobar.
  GURL url_;
  // Navigation details that are set in way that ShouldExpire() returns true.
  infobars::InfoBarDelegate::NavigationDetails nav_details_that_expire_{
      .entry_id = kNavEntryId,
      .is_navigation_to_different_page = true,
      .did_replace_entry = false,
      .is_reload = true,
      .is_redirect = false,
      .is_form_submission = false,
      .has_user_gesture = true};
  // Infobar delegate to test.
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> delegate_;
  // Pointer to the infobar's form manager.
  raw_ptr<password_manager::MockPasswordFormManagerForUI> form_manager_ptr_;
};

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetUserNameText) {
  EXPECT_NSEQ(kUsernameValue, delegate_->GetUserNameText());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetPasswordText) {
  EXPECT_NSEQ(kPasswordValue, delegate_->GetPasswordText());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetURLHostText) {
  EXPECT_NSEQ(@"example.com", delegate_->GetURLHostText());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetAccountToStorePassword) {
  ASSERT_TRUE(delegate_->GetAccountToStorePassword());
  EXPECT_EQ(kAccountToStorePassword, *delegate_->GetAccountToStorePassword());
}

// Tests that the infobar expires when reloading the page and other conditions
// are true.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, ShouldExpire_True_WhenReload) {
  nav_details_that_expire_.is_reload = true;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when new navigation ID and other conditions
// are true.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_True_WhenDifferentNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  const int different_nav_id = kNavEntryId - 1;
  delegate_->set_nav_entry_id(different_nav_id);

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is disabled, having a user gesture isn't
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false shouldn't change the returned value.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_True_WhenNoStickyInfobarAndNoUserGesture) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAutofillStickyInfobarIos);

  nav_details_that_expire_.has_user_gesture = false;

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to true should return true.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_True_WhenStickyInfobarAndUserGesture) {
  nav_details_that_expire_.has_user_gesture = true;
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when the page is the same.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenNoDifferentPage) {
  nav_details_that_expire_.is_navigation_to_different_page = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when the page is the same.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenDidReplaceEntry) {
  nav_details_that_expire_.did_replace_entry = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when form submission.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenRedirect) {
  nav_details_that_expire_.is_redirect = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when no reload and the navigation entry ID
// didn't change.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenFormSubmission) {
  nav_details_that_expire_.is_form_submission = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenNoReloadAndSameNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false should return false.
TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       ShouldExpire_False_WhenStickyInfobar) {
  nav_details_that_expire_.has_user_gesture = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetMessageText_WhenSave) {
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
      delegate_->GetMessageText());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, GetMessageText_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD),
            delegate_->GetMessageText());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       GetButtonLabel_ButtonOk_WhenSave) {
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
            delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       GetButtonLabel_ButtonOk_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON),
            delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       GetButtonLabel_ButtonCancel_WhenSave) {
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_MODAL_BLOCK_BUTTON),
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       GetButtonLabel_ButtonCancel_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);
  EXPECT_THAT(delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
              testing::IsEmpty());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, Accept_WhenSave) {
  // Verify that the password manager handles the Accept action.
  EXPECT_CALL(*form_manager_ptr_, Save).Times(1);

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Tap on "Accept".
  delegate_->Accept();

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.SignedInAccountStoreUser",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kAccepted));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, Accept_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);

  // Verify that the password manager handles the Accept action.
  EXPECT_CALL(*form_manager_ptr_, Save).Times(1);

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Tap on "Accept".
  delegate_->Accept();

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UpdateUIDismissalReason",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UpdateUIDismissalReason",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kAccepted));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, Cancel_WhenSave) {
  // Verify that the password manager handles the Cancel action.
  EXPECT_CALL(*form_manager_ptr_, Blocklist).Times(1);

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Tap on "Cancel".
  delegate_->Cancel();

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::CLICKED_NEVER, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.SignedInAccountStoreUser",
      password_manager::metrics_util::CLICKED_NEVER, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kDeclined));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, Dismiss_WhenSave) {
  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Swipe away the banner.
  delegate_->InfoBarDismissed();

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::CLICKED_CANCEL, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.SignedInAccountStoreUser",
      password_manager::metrics_util::CLICKED_CANCEL, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kDeclined));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, Dismiss_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Swipe away the banner.
  delegate_->InfoBarDismissed();

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UpdateUIDismissalReason",
      password_manager::metrics_util::CLICKED_CANCEL, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kDeclined));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, NoAction_WhenSave) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  // Report the banner as done to record the metrics without any actions taken.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kIgnored));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, NoAction_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);

  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Report the banner as done to record the metrics.
  delegate_->InfobarGone();

  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UpdateUIDismissalReason",
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_InteractionName,
      static_cast<int64_t>(password_manager::PasswordFormMetricsRecorder::
                               BubbleDismissalReason::kIgnored));
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, UpdateCredentials) {
  NSString* updated_username_value = @"new_username";
  NSString* updated_password_value = @"new_password";

  // Verify that the password manager handles the update when there are new
  // credential values.
  EXPECT_CALL(*form_manager_ptr_, OnUpdateUsernameFromPrompt).Times(1);

  delegate_->UpdateCredentials(updated_username_value, updated_password_value);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       PresentationMetrics_WhenSave_Automatic) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  delegate_->InfobarPresenting(/*automatic=*/true);
  delegate_->InfobarGone();
  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordBubble.DisplayDisposition",
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING, 1);

  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();

  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_TriggerName,
      static_cast<int64_t>(
          password_manager::PasswordFormMetricsRecorder::BubbleTrigger::
              kPasswordManagerSuggestionAutomatic));
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 1);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_ShownName, 0);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       PresentationMetrics_WhenSave_NotAutomatic) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  delegate_->InfobarPresenting(/*automatic=*/false);
  delegate_->InfobarGone();
  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordBubble.DisplayDisposition",
      password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_TriggerName,
      static_cast<int64_t>(
          password_manager::PasswordFormMetricsRecorder::BubbleTrigger::
              kPasswordManagerSuggestionManual));
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 1);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_ShownName, 0);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       PresentationMetrics_WhenUpdate_Automatic) {
  InitializeDelegate(/*password_update=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  delegate_->InfobarPresenting(/*automatic=*/true);
  delegate_->InfobarGone();
  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordBubble.DisplayDisposition",
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
      1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_TriggerName,
      static_cast<int64_t>(
          password_manager::PasswordFormMetricsRecorder::BubbleTrigger::
              kPasswordManagerSuggestionAutomatic));
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_ShownName, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       PresentationMetrics_WhenUpdate_NotAutomatic) {
  InitializeDelegate(/*password_update=*/true);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  delegate_->InfobarPresenting(/*automatic=*/false);
  delegate_->InfobarGone();
  metrics_recorder_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordBubble.DisplayDisposition",
      password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE, 1);

  // Verify UKMs.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_THAT(entries, testing::SizeIs(1));
  const ukm::mojom::UkmEntry* entry = entries.front();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_TriggerName,
      static_cast<int64_t>(
          password_manager::PasswordFormMetricsRecorder::BubbleTrigger::
              kPasswordManagerSuggestionManual));
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::PasswordForm::kUpdating_Prompt_ShownName, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordInfobarDuration_WhenSave_OnInfobarGone) {
  base::HistogramTester histogram_tester;

  const base::TimeDelta duration = base::Seconds(1);

  // Emulate starting presenting the info banner so actions can be taken and
  // hence metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  task_environment_.AdvanceClock(duration);

  // Trigger metrics recording from dismissal.
  delegate_->InfobarGone();

  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.SaveDuration.All", duration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.SaveDuration.OnDismiss", duration, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordInfobarDuration_WhenSave_OnDeletion) {
  base::HistogramTester histogram_tester;

  const base::TimeDelta duration = base::Seconds(1);

  // Emulate starting presenting the info banner so actions can be taken and
  // hence metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  task_environment_.AdvanceClock(duration);

  // Trigger metrics recording from deletion.
  delegate_.reset();

  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.SaveDuration.All", duration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.SaveDuration.OnDeletion", duration, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordInfobarDuration_WhenUpdate_OnInfobarGone) {
  InitializeDelegate(/*password_update=*/true);

  base::HistogramTester histogram_tester;

  const base::TimeDelta duration = base::Seconds(1);

  // Emulate starting presenting the info banner so actions can be taken and
  // hence metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  task_environment_.AdvanceClock(duration);

  // Trigger metrics recording from dismissal.
  delegate_->InfobarGone();

  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.UpdateDuration.All", duration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.UpdateDuration.OnDismiss", duration, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordInfobarDuration_WhenUpdate_OnDeletion) {
  InitializeDelegate(/*password_update=*/true);

  base::HistogramTester histogram_tester;

  const base::TimeDelta duration = base::Seconds(1);

  // Emulate starting presenting the info banner so actions can be taken and
  // hence metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  task_environment_.AdvanceClock(duration);

  // Trigger metrics recording from deletion.
  delegate_.reset();

  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.UpdateDuration.All", duration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.UpdateDuration.OnDeletion", duration, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordMetrics_OnDeletion_WhenPresenting) {
  base::HistogramTester histogram_tester;

  const base::TimeDelta duration = base::Seconds(1);

  // Emulate starting presenting the info banner so actions can be taken and
  // hence metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  task_environment_.AdvanceClock(duration);

  // Delete delegate object to trigger metrics recording.
  delegate_.reset();

  // Verify that the duration is recorded.
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.iOS.InfoBar.SaveDuration.OnDeletion", duration, 1);

  // Verify that the dismissal metrics are recorded.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordMetrics_OnDeletion_WhenNotPresenting) {
  base::HistogramTester histogram_tester;

  // Delete delegate object to trigger metrics recording.
  delegate_.reset();

  // Verify that duration and dismissal metrics aren't recorded.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.iOS.InfoBar.SaveDuration.All", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.iOS.InfoBar.SaveDuration.OnDeletion", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.SaveUIDismissalReason", 0);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       RecordMetrics_OnInfobarGone_WhenNotPresenting) {
  base::HistogramTester histogram_tester;

  delegate_->InfobarGone();

  // Verify that duration and dismissal metrics aren't recorded.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.iOS.InfoBar.SaveDuration.All", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.iOS.InfoBar.SaveDuration.OnDismiss", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.SaveUIDismissalReason", 0);
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, IsPasswordUpdate_WhenUpdate) {
  InitializeDelegate(/*password_update=*/true);
  EXPECT_TRUE(delegate_->IsPasswordUpdate());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest, IsPasswordUpdate_WhenSave) {
  EXPECT_FALSE(delegate_->IsPasswordUpdate());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       IsCurrentPasswordSaved_WhenSave) {
  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  delegate_->Accept();

  EXPECT_TRUE(delegate_->IsCurrentPasswordSaved());
}

TEST_F(IOSChromeSavePasswordInfoBarDelegateTest,
       IsCurrentPasswordSaved_NoAction) {
  // Emulate starting presenting the info banner so actions can be taken hence
  // metrics recorded.
  delegate_->InfobarPresenting(/*automatic=*/true);

  EXPECT_FALSE(delegate_->IsCurrentPasswordSaved());
}
