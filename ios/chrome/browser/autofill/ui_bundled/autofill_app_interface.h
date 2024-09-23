// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// CreditCardSaveManager events that can be waited on by the IOSTestEventWaiter.
// Name reflects the observer method that is triggering this event.
enum CreditCardSaveManagerObserverEvent : int {
  kOnOfferLocalSaveCalled,
  kOnDecideToRequestUploadSaveCalled,
  kOnReceivedGetUploadDetailsResponseCalled,
  kOnSentUploadCardRequestCalled,
  kOnReceivedUploadCardResponseCalled,
  kOnStrikeChangeCompleteCalled,
  kOnShowCardSavedFeedbackCalled
};

// AutofillAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface AutofillAppInterface : NSObject

// Removes all credentials stored in the profile store.
+ (void)clearProfilePasswordStore;

// Saves an example form in the profile store.
+ (void)saveExamplePasswordFormToProfileStore;

// Saves an example form in the store for the passed URL spec.
+ (void)savePasswordFormForURLSpec:(NSString*)URLSpec;

// Returns the number of profiles (addresses) in the data manager.
+ (NSInteger)profilesCount;

// Clears the profiles (addresses) in the data manager.
+ (void)clearProfilesStore;

// Saves a sample profile (address) in the data manager.
+ (void)saveExampleProfile;

// Saves a sample account profile (address) in the data manager.
+ (void)saveExampleAccountProfile;

// Returns the name of the sample profile.
+ (NSString*)exampleProfileName;

// Removes the locally stored credit cards.
+ (void)clearCreditCardStore;

// Clears all server data including server cards.
+ (void)clearAllServerDataForTesting;

// Saves a local credit card that doesn't require CVC to be used.
// Returns the `card.NetworkAndLastFourDigits` of the card used in the UIs.
+ (NSString*)saveLocalCreditCard;

// Returns the number of credit cards in the local store.
+ (NSInteger)localCreditCount;

// Saves a masked credit card that requires CVC to be used.
// Returns the `card.NetworkAndLastFourDigits` of the card used in the UIs.
+ (NSString*)saveMaskedCreditCard;

// Saves a masked credit card which is enrolled to create a Virtual Card.
// No current CVC auth required.
// Returns the `card.NetworkAndLastFourDigits` of the card used in the UIs.
+ (NSString*)saveMaskedCreditCardEnrolledInVirtualCard;

// The functions below are helpers for the SaveCardInfobarEGTest that requires
// observing autofill events in the app process.
// FakeCreditCardServer is an object instantiated in the app process that
// will observe the needed events.

// Creates a FakeCreditCardServer object and call SetUp on it.
// This will register event observer and test URL loader and histogram tester.
+ (void)setUpFakeCreditCardServer;

// Tear down the FakeCreditCardServer, unregister it and delete it.
+ (void)tearDownFakeCreditCardServer;

// Clears the virtual card enrollment strike data.
+ (void)clearVirtualCardEnrollmentStrikes;

// Sets the Autofill events that are expected to be triggered.
+ (void)resetEventWaiterForEvents:(NSArray*)events
                          timeout:(base::TimeDelta)timeout;

// Wait until all expected events are triggered.
+ (BOOL)waitForEvents [[nodiscard]];

// Sets the next response of the payments server for `request`.
+ (void)setPaymentsResponse:(NSString*)response
                 forRequest:(NSString*)request
              withErrorCode:(int)error;

// Clear all existing fake response.
+ (void)clearPaymentsResponses;

// Sets a fake access token to bypass the token fetch request.
+ (void)setAccessToken;

// Sets the number of times the user refused to save a card.
+ (void)setFormFillMaxStrikes:(int)max forCard:(NSString*)card;

// Gets the risk data for payments.
+ (NSString*)paymentsRiskData;

// Sets the risk data for payments.
+ (void)setPaymentsRiskData:(NSString*)riskData;

// Make it that we consider the credit card form to be a secure in the current
// context. This will allow us to fill the textfields on the web page. We only
// want to use this for tests.
+ (void)considerCreditCardFormSecureForTesting;

// Sets a re-authentication mock (i.e. what asks user for fingerprint to
// view password) and its options for next test.
+ (void)setUpMockReauthenticationModule;
+ (void)clearMockReauthenticationModule;
+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt;
+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult;

// Configs the mandatory reauth preference.
+ (void)setMandatoryReauthEnabled:(BOOL)enabled;

// Returns true if the Keyboard Accessory Upgrade feature is enabled.
+ (BOOL)isKeyboardAccessoryUpgradeEnabled;

// Returns true if the dynamically loading fields on input feature is enabled.
+ (BOOL)isDynamicallyLoadFieldsOnInputEnabled;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_APP_INTERFACE_H_
