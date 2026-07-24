// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_mediator.h"

#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_consumer.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_coordinator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

class AutofillBnplTosMediatorTest : public PlatformTest {
 protected:
  AutofillBnplTosMediatorTest() {
    mock_consumer_ = OCMProtocolMock(@protocol(AutofillBnplTosConsumer));
  }

  autofill::payments::BnplTosModel CreateTestModel(bool linked) {
    autofill::payments::BnplTosModel model;
    std::optional<int64_t> instrument_id = std::nullopt;
    if (linked) {
      instrument_id = 123456;
    }
    model.issuer = autofill::BnplIssuer(
        instrument_id, autofill::BnplIssuer::IssuerId::kBnplAffirm,
        /*eligible_price_ranges=*/{});

    autofill::TestLegalMessageLine line(
        "Agree to Terms and Privacy Policy.",
        {
            autofill::LegalMessageLine::Link(9, 14,
                                             "https://terms.example.com"),
            autofill::LegalMessageLine::Link(19, 33,
                                             "https://privacy.example.com"),
        });
    model.legal_message_lines = {line};
    return model;
  }

  id mock_consumer_;
};

// Tests that the mediator correctly populates the consumer for an unlinked
// issuer.
TEST_F(AutofillBnplTosMediatorTest, PopulateConsumerUnlinked) {
  auto callbacks = std::make_unique<BnplCallbacks>();
  callbacks->accept_callback = base::DoNothing();
  callbacks->cancel_callback = base::DoNothing();

  AutofillBnplTosMediator* mediator = [[AutofillBnplTosMediator alloc]
      initWithModel:CreateTestModel(/*linked=*/false)
          callbacks:std::move(callbacks)];

  NSString* expectedTitle = l10n_util::GetNSStringF(
      IDS_AUTOFILL_BNPL_TOS_UNLINKED_TITLE,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFFIRM));

  OCMExpect([mock_consumer_ setTitleText:expectedTitle]);
  OCMExpect([mock_consumer_ setIssuerLogo:[OCMArg checkWithBlock:^BOOL(id val) {
                              return val != nil;
                            }]]);
  OCMExpect([mock_consumer_
      setTermsBulletPoints:[OCMArg checkWithBlock:^BOOL(id val) {
        NSArray<AutofillBnplTosBulletItem*>* list = val;
        if (list.count != 3) {
          return NO;
        }
        EXPECT_NSEQ(SymbolTemplateWithPointSize(SymbolPersonCropCircle, 20.0),
                    list[0].icon);
        EXPECT_NSEQ(SymbolTemplateWithPointSize(SymbolTextDocument, 20.0),
                    list[1].icon);
        return YES;
      }]]);
  OCMExpect([mock_consumer_
      setConsentText:[OCMArg checkWithBlock:^BOOL(id val) {
        NSAttributedString* consent = val;
        if (![consent.string
                isEqualToString:@"Agree to Terms and Privacy Policy."]) {
          return NO;
        }
        NSRange range;
        NSURL* termsURL = [consent attribute:NSLinkAttributeName
                                     atIndex:10
                              effectiveRange:&range];
        EXPECT_NSEQ(@"https://terms.example.com/", termsURL.absoluteString);
        EXPECT_EQ(9U, range.location);
        EXPECT_EQ(5U, range.length);

        NSURL* privacyURL = [consent attribute:NSLinkAttributeName
                                       atIndex:22
                                effectiveRange:&range];
        EXPECT_NSEQ(@"https://privacy.example.com/", privacyURL.absoluteString);
        EXPECT_EQ(19U, range.location);
        EXPECT_EQ(14U, range.length);
        return YES;
      }]]);

  mediator.consumer = mock_consumer_;

  [mock_consumer_ verify];
}

// Tests that the mediator correctly populates the consumer for a linked issuer.
TEST_F(AutofillBnplTosMediatorTest, PopulateConsumerLinked) {
  auto callbacks = std::make_unique<BnplCallbacks>();
  callbacks->accept_callback = base::DoNothing();
  callbacks->cancel_callback = base::DoNothing();

  AutofillBnplTosMediator* mediator = [[AutofillBnplTosMediator alloc]
      initWithModel:CreateTestModel(/*linked=*/true)
          callbacks:std::move(callbacks)];

  NSString* expectedTitle = l10n_util::GetNSStringF(
      IDS_AUTOFILL_BNPL_TOS_LINKED_TITLE,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFFIRM));

  OCMExpect([mock_consumer_ setTitleText:expectedTitle]);
  OCMStub([mock_consumer_ setIssuerLogo:[OCMArg any]]);
  OCMStub([mock_consumer_ setTermsBulletPoints:[OCMArg any]]);
  OCMStub([mock_consumer_ setConsentText:[OCMArg any]]);

  mediator.consumer = mock_consumer_;

  [mock_consumer_ verify];
}

// Tests that accepting the ToS triggers the C++ callback.
TEST_F(AutofillBnplTosMediatorTest, AcceptCallbackTriggered) {
  __block bool accept_callback_run = false;
  auto callbacks = std::make_unique<BnplCallbacks>();
  callbacks->accept_callback =
      base::BindOnce([](bool* run) { *run = true; }, &accept_callback_run);
  callbacks->cancel_callback = base::DoNothing();

  AutofillBnplTosMediator* mediator = [[AutofillBnplTosMediator alloc]
      initWithModel:CreateTestModel(/*linked=*/false)
          callbacks:std::move(callbacks)];

  [mediator didTapContinue];

  EXPECT_TRUE(accept_callback_run);
}

// Tests that canceling the ToS triggers the C++ callback.
TEST_F(AutofillBnplTosMediatorTest, CancelCallbackTriggered) {
  __block bool cancel_callback_run = false;
  auto callbacks = std::make_unique<BnplCallbacks>();
  callbacks->accept_callback = base::DoNothing();
  callbacks->cancel_callback =
      base::BindOnce([](bool* run) { *run = true; }, &cancel_callback_run);

  AutofillBnplTosMediator* mediator = [[AutofillBnplTosMediator alloc]
      initWithModel:CreateTestModel(/*linked=*/false)
          callbacks:std::move(callbacks)];

  [mediator didTapCancel];

  EXPECT_TRUE(cancel_callback_run);
}
