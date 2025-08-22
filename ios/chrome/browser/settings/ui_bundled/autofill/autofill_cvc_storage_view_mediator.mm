// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_mediator.h"

#import "base/check.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface AutofillCvcStorageViewMediator () <BooleanObserver>

@end

@implementation AutofillCvcStorageViewMediator {
  // The personal data manager, used to fetch and update autofill data.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
  raw_ptr<PrefService> _prefs;
  PrefBackedBoolean* _cvcStorageEnabled;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(personalDataManager);
    CHECK(prefService);
    _personalDataManager = personalDataManager;
    _prefs = prefService;
    _cvcStorageEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefs
                   prefName:autofill::prefs::kAutofillPaymentCvcStorage];
    _cvcStorageEnabled.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<AutofillCvcStorageConsumer>)consumer {
  _consumer = consumer;
  if (consumer) {
    _consumer.cvcStorageSwitchIsOn = _cvcStorageEnabled.value;
    _consumer.hasSavedCvcs = [self hasSavedCvcs];
  }
}

- (void)disconnect {
  _personalDataManager = nullptr;
  _cvcStorageEnabled.observer = nil;
  _cvcStorageEnabled = nil;
}

#pragma mark - AutofillCvcStorageViewControllerDelegate

- (void)viewController:(AutofillCvcStorageViewController*)controller
    didChangeCvcStorageSwitchTo:(BOOL)isOn {
  _cvcStorageEnabled.value = isOn;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(PrefBackedBoolean*)boolean {
  CHECK_EQ(boolean, _cvcStorageEnabled);
  self.consumer.cvcStorageSwitchIsOn = _cvcStorageEnabled.value;
}

#pragma mark - Private

- (BOOL)hasSavedCvcs {
  CHECK(_personalDataManager);
  // TODO(crbug.com/40266992): This should check if there are any saved CVCs.
  return true;
}

@end
