// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/passwords/cwv_leak_check_credential_internal.h"
#import "ios/web_view/internal/passwords/cwv_leak_check_service_internal.h"

#import "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#import "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#import "ios/web_view/public/cwv_leak_check_service_observer.h"

using password_manager::BulkLeakCheckServiceInterface;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::LeakDetectionInitiator;

// Private interface callable by the ObserverBridge.
@interface CWVLeakCheckService ()

// Called when the ObserverBridge is notified of a change in state.
- (void)bulkLeakCheckServiceDidChangeState;

// Called when the ObserverBridge is notified of a finished credential.
// |credential| The credential that finished.
// |isLeaked| Whether the credential was leaked or not.
- (void)bulkLeakCheckServiceDidFinishCredential:
            (const LeakCheckCredential&)credential
                                       isLeaked:(BOOL)isLeaked;

@end

namespace ios_web_view {

// C++ to ObjC bridge for BulkLeakCheckServiceInterface::Observer.
class ObserverBridge : public BulkLeakCheckServiceInterface::Observer {
 public:
  ObserverBridge(CWVLeakCheckService* service) : service_(service) {
    DCHECK(service_);
  }

  void OnStateChanged(
      BulkLeakCheckServiceInterface::State internalState) override {
    [service_ bulkLeakCheckServiceDidChangeState];
  }

  void OnCredentialDone(const LeakCheckCredential& credential,
                        IsLeaked is_leaked) override {
    [service_ bulkLeakCheckServiceDidFinishCredential:credential
                                             isLeaked:*is_leaked];
  }

 private:
  __weak CWVLeakCheckService* service_;
};

}  // namespace ios_web_view

@implementation CWVLeakCheckService {
  BulkLeakCheckServiceInterface* _bulkLeakCheckService;
  std::unique_ptr<ios_web_view::ObserverBridge> _observerBridge;
  NSHashTable<id<CWVLeakCheckServiceObserver>>* _observers;
}

- (instancetype)initWithBulkLeakCheckService:
    (BulkLeakCheckServiceInterface*)bulkLeakCheckService {
  DCHECK(bulkLeakCheckService);
  self = [super init];
  if (self) {
    _bulkLeakCheckService = bulkLeakCheckService;
    _observerBridge = std::make_unique<ios_web_view::ObserverBridge>(self);
    _bulkLeakCheckService->AddObserver(_observerBridge.get());
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)dealloc {
  _bulkLeakCheckService->RemoveObserver(_observerBridge.get());
}

- (CWVLeakCheckServiceState)state {
  switch (_bulkLeakCheckService->GetState()) {
    case BulkLeakCheckServiceInterface::State::kIdle:
      return CWVLeakCheckServiceStateIdle;
    case BulkLeakCheckServiceInterface::State::kRunning:
      return CWVLeakCheckServiceStateRunning;
    case BulkLeakCheckServiceInterface::State::kCanceled:
      return CWVLeakCheckServiceStateCanceled;
    case BulkLeakCheckServiceInterface::State::kSignedOut:
      return CWVLeakCheckServiceStateSignedOut;
    case BulkLeakCheckServiceInterface::State::kTokenRequestFailure:
      return CWVLeakCheckServiceStateTokenRequestFailure;
    case BulkLeakCheckServiceInterface::State::kHashingFailure:
      return CWVLeakCheckServiceStateHashingFailure;
    case BulkLeakCheckServiceInterface::State::kNetworkError:
      return CWVLeakCheckServiceStateNetworkError;
    case BulkLeakCheckServiceInterface::State::kServiceError:
      return CWVLeakCheckServiceStateServiceError;
    case BulkLeakCheckServiceInterface::State::kQuotaLimit:
      return CWVLeakCheckServiceStateQuotaLimit;
  }
}

- (void)addObserver:(__weak id<CWVLeakCheckServiceObserver>)observer {
  [_observers addObject:observer];
}

- (void)removeObserver:(__weak id<CWVLeakCheckServiceObserver>)observer {
  [_observers removeObject:observer];
}

- (void)checkCredentials:(NSArray<CWVLeakCheckCredential*>*)credentials {
  std::vector<LeakCheckCredential> internalCredentials;
  if (!credentials.count)
    return;

  for (CWVLeakCheckCredential* credential in credentials) {
    internalCredentials.emplace_back(credential.internalCredential.username(),
                                     credential.internalCredential.password());
  }

  _bulkLeakCheckService->CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kIGABulkSyncedPasswordsCheck,
      std::move(internalCredentials));
}

- (void)cancel {
  _bulkLeakCheckService->Cancel();
}

- (void)bulkLeakCheckServiceDidChangeState {
  for (id<CWVLeakCheckServiceObserver> observer in _observers) {
    [observer leakCheckServiceDidChangeState:self];
  }
}

- (void)bulkLeakCheckServiceDidFinishCredential:
            (const LeakCheckCredential&)internalCredential
                                       isLeaked:(BOOL)isLeaked {
  auto internalCredentialCopy = std::make_unique<LeakCheckCredential>(
      internalCredential.username(), internalCredential.password());
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::move(internalCredentialCopy)];

  for (id<CWVLeakCheckServiceObserver> observer in _observers) {
    [observer leakCheckService:self
            didCheckCredential:credential
                      isLeaked:isLeaked];
  }
}

@end
