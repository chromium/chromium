// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_controller+Testing.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/payments/card_unmask_delegate.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/card_unmask_header_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item+Testing.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

class MockCardUnmaskPromptController
    : public autofill::CardUnmaskPromptControllerImpl {
 public:
  MockCardUnmaskPromptController(
      TestingPrefServiceSimple* pref_service,
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate)
      : CardUnmaskPromptControllerImpl(pref_service,
                                       card,
                                       card_unmask_prompt_options,
                                       delegate) {}

  MockCardUnmaskPromptController(const MockCardUnmaskPromptController&) =
      delete;
  MockCardUnmaskPromptController& operator=(
      const MockCardUnmaskPromptController&) = delete;

  // Mock method to validate that submitting forms forward the data to the
  // controller.
  MOCK_METHOD(void,
              OnUnmaskPromptAccepted,
              (const std::u16string& cvc,
               const std::u16string& exp_month,
               const std::u16string& exp_year,
               bool enable_fido_auth,
               bool was_checkbox_visible),
              (override));

  MOCK_METHOD(const autofill::CreditCard&,
              GetCreditCard,
              (),
              (const, override));

  MOCK_METHOD(bool, ShouldRequestExpirationDate, (), (const, override));

  MOCK_METHOD(bool,
              InputExpirationIsValid,
              (const std::u16string& month, const std::u16string& year),
              (const, override));
};

class MockCardUnmaskPromptViewBridge
    : public autofill::CardUnmaskPromptViewBridge {
 public:
  explicit MockCardUnmaskPromptViewBridge(
      autofill::CardUnmaskPromptControllerImpl* controller,
      autofill::TestPersonalDataManager* personal_data_manager)
      : CardUnmaskPromptViewBridge(
            controller,
            [[UINavigationController alloc] init],
            personal_data_manager,
            /*browser_coordinator_commands_handler=*/nil) {}

  MockCardUnmaskPromptViewBridge(const MockCardUnmaskPromptViewBridge&) =
      delete;
  MockCardUnmaskPromptViewBridge& operator=(
      const MockCardUnmaskPromptViewBridge&) = delete;

  ~MockCardUnmaskPromptViewBridge() override {
    // The production implementation notifies the controller when it gets
    // destroyed. This is not needed in the mock implementation and the
    // controller might be destroyed at this point. Discarding the pointer
    // prevent CardUnmaskPromptViewBridge from referencing it.
    controller_ = nullptr;
  }

  MOCK_METHOD(void, PerformClose, (), (override));
};

class CardUnmaskPromptViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    root_view_controller_ = [[UIViewController alloc] init];

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    card_unmask_prompt_controller_ =
        std::make_unique<NiceMock<MockCardUnmaskPromptController>>(
            pref_service_.get(), card_, challenge_option_,
            /*delegate=*/nullptr);
    ON_CALL(*card_unmask_prompt_controller_, GetCreditCard)
        .WillByDefault(testing::ReturnRef(card_));

    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();

    card_unmask_prompt_bridge_ =
        std::make_unique<NiceMock<MockCardUnmaskPromptViewBridge>>(
            card_unmask_prompt_controller_.get(), personal_data_manager_.get());

    CreateController();
  }

  void TearDown() override {
    CardUnmaskPromptViewController* prompt_controller =
        static_cast<CardUnmaskPromptViewController*>(controller());
    // Delete C++ reference in view controller to prevent UAF error.
    [prompt_controller disconnectFromBridge];

    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[CardUnmaskPromptViewController alloc]
        initWithBridge:(card_unmask_prompt_bridge_.get())];
  }

  // Fetches the model for the header from the tableViewModel.
  CardUnmaskHeaderItem* HeaderITem() {
    return static_cast<CardUnmaskHeaderItem*>(
        [controller().tableViewModel headerForSectionIndex:0]);
  }

  // Helper method that fetches an item from the tableViewModel.
  id GetItem(int item, int section) {
    auto* indexPath = [NSIndexPath indexPathForItem:item inSection:section];
    auto* model = controller().tableViewModel;

    return [model hasItemAtIndexPath:indexPath]
               ? [model itemAtIndexPath:indexPath]
               : nil;
  }

  // Helper method that fetches a cell from the tableView's datasource.
  id GetCell(int item, int section) {
    auto* cvc_controller = controller();
    auto* cvc_index_path = [NSIndexPath indexPathForItem:item
                                               inSection:section];

    return
        [cvc_controller.tableView.dataSource tableView:cvc_controller.tableView
                                 cellForRowAtIndexPath:cvc_index_path];
  }

  // Fetches the card info cell from the tableView's datasource.
  TableViewDetailIconCell* GetCardInfoCell() {
    return GetCell(/*item=*/0, /*section*/ 0);
  }

  TableViewDetailIconItem* GetCardInfoItem() {
    return GetItem(/*item=*/0, /*section=*/0);
  }

  // Fetches the CVC input cell from the tableView's datasource.
  TableViewTextEditCell* GetCVCInputCell() {
    return GetCell(/*item=*/0, /*section*/ 1);
  }

  // Fetches the model for the CVC input cell from the tableViewModel.
  TableViewTextEditItem* CVCInputItem() {
    return GetItem(/*item=*/0, /*section=*/1);
  }

  // Fetches the model for the expiration date input cell from the
  // tableViewModel.
  ExpirationDateEditItem* ExpirationDateItem() {
    return GetItem(/*item=*/1, /*section=*/1);
  }

  // Fetches the model for the footer from the tableViewModel.
  TableViewLinkHeaderFooterItem* FooterItem() {
    return static_cast<TableViewLinkHeaderFooterItem*>(
        [controller().tableViewModel footerForSectionIndex:1]);
  }

  // Returns the right bar item in the navigationItem.
  UIBarButtonItem* RightBarButtonItem() {
    return controller().navigationItem.rightBarButtonItem;
  }

  // Returns the confirm button in the navigationItem.
  // Validates the button has a title and an action set.
  UIBarButtonItem* ConfirmButton() {
    auto* button = RightBarButtonItem();
    EXPECT_FALSE(button.customView);
    EXPECT_TRUE(button.title.length);
    EXPECT_TRUE(button.action);
    return button;
  }

  // Validates that the header is in the tableViewModel and the instructions
  // match those in card_unmask_prompt_controller_.
  void CheckHeaderAndInstructions() {
    CardUnmaskHeaderItem* header_item = HeaderITem();
    EXPECT_TRUE(header_item);

    NSString* expected_title = base::SysUTF16ToNSString(
        card_unmask_prompt_controller_->GetWindowTitle());
    NSString* expected_instructions = base::SysUTF16ToNSString(
        card_unmask_prompt_controller_->GetInstructionsMessage());

    EXPECT_NSEQ(header_item.titleText, expected_title);
    EXPECT_NSEQ(header_item.instructionsText, expected_instructions);
  }

  // Validates that the right item in the navigation bar displays an activity
  // indicator.
  void CheckLoadingIndicator() {
    auto* right_bar_button_item = RightBarButtonItem();

    EXPECT_TRUE([right_bar_button_item.customView
        isMemberOfClass:[UIActivityIndicatorView class]]);
  }

  // Presents the controller in a ScopedKeyWindow.
  // Used for tests that require running animations.
  void PresentController() {
    scoped_window_.Get().rootViewController = root_view_controller_;

    // Present the view controller.
    __block bool presentation_finished = NO;
    UINavigationController* navigation_controller =
        [[UINavigationController alloc]
            initWithRootViewController:controller()];

    [root_view_controller_ presentViewController:navigation_controller
                                        animated:NO
                                      completion:^{
                                        presentation_finished = YES;
                                      }];

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool {
          return presentation_finished;
        }));
  }

  // Simulates submitting either the cvc or expiration date form and verifies
  // the Controller receives the form data.
  void CheckSubmittingForm(NSString* CVC,
                           NSString* month = nil,
                           NSString* year = nil) {
    CVCInputItem().textFieldValue = CVC;

    auto* expiration_date_item = ExpirationDateItem();

    if (expiration_date_item) {
      expiration_date_item.month = month;
      expiration_date_item.year = year;
    }

    auto* prompt_controller =
        static_cast<CardUnmaskPromptViewController*>(controller());

    // Mock expiration date validation so the prompt sends test data.
    ON_CALL(*card_unmask_prompt_controller_, InputExpirationIsValid)
        .WillByDefault(Return(true));

    // ViewController should notify its Controller when the CVC form is
    // submitted.
    EXPECT_CALL(*card_unmask_prompt_controller_,
                OnUnmaskPromptAccepted(Eq(base::SysNSStringToUTF16(CVC)),
                                       Eq(base::SysNSStringToUTF16(month)),
                                       Eq(base::SysNSStringToUTF16(year)),
                                       Eq(false), Eq(false)));

    [prompt_controller onVerifyTapped];
  }

  // Verifies that the update expiration link is in the tableViewModel.
  void CheckUpdateExpirationDateLink() {
    TableViewLinkHeaderFooterItem* footer = FooterItem();

    // Validate that the footer has text and a link.
    EXPECT_TRUE(footer.text.length);
    EXPECT_EQ(footer.urls.count, 1LU);
  }

  // Verifies that the correct items are in the tableViewModel.
  void CheckTableViewModel(bool with_expiration_date_item) {
    // Check expected number of sections and items.
    EXPECT_EQ(NumberOfSections(), 2);
    // First section has two items (a header and a card info cell).
    EXPECT_EQ(NumberOfItemsInSection(0), 1);
    // CVC and expiration date fields if applicable.
    EXPECT_EQ(NumberOfItemsInSection(1), with_expiration_date_item ? 2 : 1);

    CheckHeaderAndInstructions();

    // Verifying card info is in model.
    EXPECT_TRUE(GetCardInfoItem());

    // Verifing CVC field is in model.
    EXPECT_TRUE(CVCInputItem());

    // Verifying expiration date field is updated correctly.
    EXPECT_EQ(with_expiration_date_item, !!ExpirationDateItem());

    // Footer shouldn't be in model.
    EXPECT_FALSE(FooterItem());
  }

  // Validates that the first responder has the given accessibility identifier.
  void CheckFirstResponderHasAccessibilityIdentifier(
      NSString* accessibility_identifier) {
    UIView* first_responder =
        base::apple::ObjCCastStrict<UIView>(GetFirstResponder());

    EXPECT_NSEQ(first_responder.accessibilityIdentifier,
                accessibility_identifier);
  }

  std::unique_ptr<NiceMock<MockCardUnmaskPromptViewBridge>>
      card_unmask_prompt_bridge_;
  std::unique_ptr<NiceMock<MockCardUnmaskPromptController>>
      card_unmask_prompt_controller_;
  autofill::CardUnmaskPromptOptions challenge_option_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_;
  autofill::CreditCard card_ = autofill::test::GetMaskedServerCard();
};

}  // namespace

// Validates that the CVC form is displayed as the initial state of the
// controller when the card is not expired.
TEST_F(CardUnmaskPromptViewControllerTest, CVCFormDisplayedAsInitialState) {
  CheckController();

  CheckTableViewModel(/*with_expiration_date_item=*/false);

  // Confirm button should be disabled until valid input is entered.
  EXPECT_FALSE(ConfirmButton().enabled);

  // Add controller to view hierarchy to verify CVC field is focused.
  PresentController();
  CheckFirstResponderHasAccessibilityIdentifier(@"CVC_textField");
}

// Validates that the Expiration Date form is displayed as the initial state of
// the controller when the card is expired.
TEST_F(CardUnmaskPromptViewControllerTest,
       ExpirationDateFormDisplayedAsInitialState) {
  // Recreate controller for the expiration date form state.
  ResetController();

  ON_CALL(*card_unmask_prompt_controller_, ShouldRequestExpirationDate)
      .WillByDefault(Return(true));

  CreateController();

  CheckTableViewModel(/*with_expiration_date_item=*/true);

  // Add controller to view hierarchy to verify CVC field is focused.
  PresentController();
  CheckFirstResponderHasAccessibilityIdentifier(@"CVC_textField");
}

// Validates that the tableViewModel is properly setup for displaying update
// expiration date form.
TEST_F(CardUnmaskPromptViewControllerTest,
       UpdateExpirationDateFormStateHasExpectedItems) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  // Update state of controller to get the instructions for updating the card's
  // expiration date.
  card_unmask_prompt_controller_->NewCardLinkClicked();

  [prompt_controller showUpdateExpirationDateForm];

  CheckTableViewModel(/*with_expiration_date_item=*/true);

  // Add controller to view hierarchy to verify Expiration Date field is
  // focused.
  PresentController();
  CheckFirstResponderHasAccessibilityIdentifier(@"Expiration Date_textField");
}

// Validates the model is properly setup for displaying the expiration card link
// in the controller's footer.
TEST_F(CardUnmaskPromptViewControllerTest,
       ShowUpdateExpirationCardLinkAddsFooterWithLink) {
  // Footer shouldn't be in model before link is shown.
  EXPECT_FALSE(FooterItem());

  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  [prompt_controller showUpdateExpirationDateLink];

  CheckUpdateExpirationDateLink();
}

// Validates that the loading state displays an activity indicator in the
// navigation bar and disables interactions with the input fields.
TEST_F(CardUnmaskPromptViewControllerTest,
       ShowLoadingStateDisplaysActivityIndicatorAndDisablesInteractions) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  EXPECT_TRUE(prompt_controller.tableView.userInteractionEnabled);

  [prompt_controller showLoadingState];

  CheckLoadingIndicator();

  EXPECT_FALSE(prompt_controller.tableView.userInteractionEnabled);
}

// Validates that an alert is presented for the error state.
TEST_F(CardUnmaskPromptViewControllerTest, ShowErrorPresentsAlert) {
  PresentController();

  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  NSString* alert_message = @"Test";
  [prompt_controller showErrorAlertWithMessage:alert_message closeOnDismiss:NO];

  // Spin the run loop to trigger the alert presentation animation.
  base::test::ios::SpinRunLoopWithMaxDelay(
      base::test::ios::kWaitForUIElementTimeout);
  ASSERT_TRUE([prompt_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  auto* alertController = static_cast<UIAlertController*>(
      prompt_controller.presentedViewController);
  // Check the state of the alert.
  EXPECT_TRUE(alertController.title.length);
  EXPECT_NSEQ(alertController.message, alert_message);
  EXPECT_EQ(alertController.actions.count, 1LU);

  // Confirm button should be restored.
  EXPECT_TRUE(ConfirmButton());
}

// Verifies that submitting the CVC form forwards the CVC value to
// CardUnmaskPromptController.
TEST_F(CardUnmaskPromptViewControllerTest, TestSubmittingCVCForm) {
  CheckSubmittingForm(/*CVC=*/@"123");
}

// Verifies that submitting the update expiration date form forwards CVC and
// expiration date to CardUnmaskPromptController/
TEST_F(CardUnmaskPromptViewControllerTest, TestSubmittingExpirationDateForm) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());
  [prompt_controller showUpdateExpirationDateForm];

  CheckSubmittingForm(/*CVC=*/@"123", /*month=*/@"09", /*year=*/@"2022");
}

// Verifies that the controller is dismissed after an error.
TEST_F(CardUnmaskPromptViewControllerTest,
       TestDismissingViewControllerAfterError) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  EXPECT_CALL(*card_unmask_prompt_bridge_, PerformClose);

  [prompt_controller onErrorAlertDismissedAndShouldCloseOnDismiss:YES];
}

// Verifies that the expiration date link is displayed after an error.
TEST_F(CardUnmaskPromptViewControllerTest,
       TestShowingUpdateExpirationDateLinkAfterError) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  [prompt_controller onErrorAlertDismissedAndShouldCloseOnDismiss:NO];

  CheckUpdateExpirationDateLink();
}

// Verifies that the expiration date form is displayed after an error.
TEST_F(CardUnmaskPromptViewControllerTest,
       TestShowingUpdateExpirationDateFormAfterError) {
  auto* prompt_controller =
      static_cast<CardUnmaskPromptViewController*>(controller());

  ON_CALL(*card_unmask_prompt_controller_, ShouldRequestExpirationDate)
      .WillByDefault(Return(true));

  [prompt_controller onErrorAlertDismissedAndShouldCloseOnDismiss:NO];

  CheckTableViewModel(/*with_expiration_date_item=*/true);
}

// Verifies that the icon in the CVC input cell is hidden from Voice Over.
TEST_F(CardUnmaskPromptViewControllerTest,
       TestCVCCellIconIsHiddenFromVoiceOver) {
  auto* CVC_cell = GetCVCInputCell();
  ASSERT_TRUE(CVC_cell);
  EXPECT_FALSE(CVC_cell.identifyingIconButton.isAccessibilityElement);
}

// Verifies that the textField in the CVC input cell does not accept more than 4
// digits.
TEST_F(CardUnmaskPromptViewControllerTest,
       TestCVCTextFieldRejectsTooLongCVCValues) {
  auto* CVC_field = GetCVCInputCell().textField;
  ASSERT_TRUE(CVC_field);
  ASSERT_EQ(CVC_field.text.length, 0LU);

  auto* delegate = CVC_field.delegate;
  NSRange start_range = NSMakeRange(0, 0);

  NSString* short_CVC = @"1";
  EXPECT_TRUE([delegate textField:CVC_field
      shouldChangeCharactersInRange:start_range
                  replacementString:short_CVC]);

  NSString* valid_CVC = @"123";
  EXPECT_TRUE([delegate textField:CVC_field
      shouldChangeCharactersInRange:start_range
                  replacementString:valid_CVC]);

  NSString* too_long_CVC = @"12345";
  EXPECT_FALSE([delegate textField:CVC_field
      shouldChangeCharactersInRange:start_range
                  replacementString:too_long_CVC]);
}
