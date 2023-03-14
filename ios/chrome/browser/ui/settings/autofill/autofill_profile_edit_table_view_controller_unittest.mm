// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#import <memory>

#import "base/feature_list.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char16_t kTestHonorificPrefix[] = u"";
const char16_t kTestFullName[] = u"That Guy John";
const char16_t kTestCompany[] = u"Awesome Inc.";
const char16_t kTestAddressLine1[] = u"Some person's garage";
const char16_t kTestAddressLine2[] = u"Near the lake";
const char16_t kTestCity[] = u"Springfield";
const char16_t kTestState[] = u"IL";
const char16_t kTestZip[] = u"55123";
const char16_t kTestCountry[] = u"United States";
const char16_t kTestPhone[] = u"16502530000";
const char16_t kTestEmail[] = u"test@email.com";

}  // namespace

@interface AutofillProfileEditFakeConsumer : NSObject

- (void)createAccountProfile;

@property(nonatomic, weak) id<AutofillProfileEditConsumer> consumer;

@end

@implementation AutofillProfileEditFakeConsumer

- (void)didSelectCountry:(NSString*)country {
}

- (void)setConsumer:(id<AutofillProfileEditConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [self createProfileData];
}

- (void)createProfileData {
  [self.consumer
      setHonorificPrefix:base::SysUTF16ToNSString(kTestHonorificPrefix)];
  [self.consumer setFullName:base::SysUTF16ToNSString(kTestFullName)];
  [self.consumer setCompanyName:base::SysUTF16ToNSString(kTestCompany)];
  [self.consumer
      setHomeAddressLine1:base::SysUTF16ToNSString(kTestAddressLine1)];
  [self.consumer
      setHomeAddressLine2:base::SysUTF16ToNSString(kTestAddressLine2)];
  [self.consumer setHomeAddressCity:base::SysUTF16ToNSString(kTestCity)];
  [self.consumer setHomeAddressState:base::SysUTF16ToNSString(kTestState)];
  [self.consumer setHomeAddressZip:base::SysUTF16ToNSString(kTestZip)];
  [self.consumer setHomeAddressCountry:base::SysUTF16ToNSString(kTestCountry)];
  [self.consumer setHomePhoneWholeNumber:base::SysUTF16ToNSString(kTestPhone)];
  [self.consumer setEmailAddress:base::SysUTF16ToNSString(kTestEmail)];
}

- (void)createAccountProfile {
  [self.consumer setAccountProfile:YES];
}

@end

namespace {

class AutofillProfileEditTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    delegate_mock_ = OCMProtocolMock(
        @protocol(AutofillProfileEditTableViewControllerDelegate));
    fake_consumer_ = [[AutofillProfileEditFakeConsumer alloc] init];
    CreateController();
    CheckController();
    fake_consumer_.consumer =
        base::mac::ObjCCastStrict<AutofillProfileEditTableViewController>(
            controller());

    // Reload the model so that the consumer changes are propogated.
    [controller() loadModel];
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileEditTableViewController alloc]
        initWithDelegate:delegate_mock_
               userEmail:nil];
  }

  AutofillProfileEditFakeConsumer* fake_consumer_;
  id delegate_mock_;
};

// Default test case of no addresses or credit cards.
TEST_F(AutofillProfileEditTableViewControllerTest, TestInitialization) {
  TableViewModel* model = [controller() tableViewModel];
  int rowCnt =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes)
          ? 11
          : 10;

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(rowCnt, [model numberOfItemsInSection:0]);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileEditTableViewControllerTest, TestOneProfile) {
  TableViewModel* model = [controller() tableViewModel];
  //  UITableView* tableView = [autofill_profile_edit_controller_ tableView];

  std::vector<const char16_t*> expected_values = {
      kTestFullName, kTestCompany, kTestAddressLine1, kTestAddressLine2,
      kTestCity,     kTestState,   kTestZip,          kTestCountry,
      kTestPhone,    kTestEmail};
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
    expected_values.insert(expected_values.begin(), kTestHonorificPrefix);
  }

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(expected_values.size(), (size_t)[model numberOfItemsInSection:0]);

  for (size_t row = 0; row < expected_values.size(); row++) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(0, row));
    EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[row]),
                cell.textFieldValue);
  }
}

class AutofillProfileEditTableViewControllerTestWithUnionViewEnabled
    : public AutofillProfileEditTableViewControllerTest {
 protected:
  AutofillProfileEditTableViewControllerTestWithUnionViewEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillAccountProfilesUnionView);
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileEditTableViewController alloc]
        initWithDelegate:delegate_mock_
               userEmail:base::SysUTF16ToNSString(kTestEmail)];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the footer text of the view controller for the address profiles with
// source kAccount when `autofill::features::kAutofillAccountProfilesUnionView`
// is enabled.
TEST_F(AutofillProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestFooterTextWithEmail) {
  [fake_consumer_ createAccountProfile];

  // Reload the model so that the consumer changes are propogated.
  [controller() loadModel];

  TableViewModel* model = [controller() tableViewModel];

  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT, kTestEmail);
  TableViewLinkHeaderFooterItem* footer = [model footerForSectionIndex:1];
  EXPECT_NSEQ(expected_footer_text, footer.text);
}

}  // namespace
