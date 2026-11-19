// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test delegate recording all required and optional
// `PasskeyModelObserverDelegate` callbacks.
@interface FakePasskeyModelObserverDelegate
    : NSObject <PasskeyModelObserverDelegate>

@property(nonatomic, readonly) NSUInteger readyCallCount;
@property(nonatomic, readonly) webauthn::PasskeyModel* lastReadyModel;
@property(nonatomic, readonly) NSUInteger failedInitializationCallCount;
@property(nonatomic, readonly)
    webauthn::PasskeyModel* lastFailedInitializationModel;
@property(nonatomic, readonly) NSUInteger didChangeCallCount;
@property(nonatomic, readonly) NSUInteger shuttingDownCallCount;
@property(nonatomic, readonly) webauthn::PasskeyModel* lastShuttingDownModel;

- (void)observePasskeyModel:(webauthn::PasskeyModel*)passkeyModel;
- (BOOL)isObservingPasskeyModel:(webauthn::PasskeyModel*)passkeyModel;

@end

@implementation FakePasskeyModelObserverDelegate {
  std::optional<PasskeyModelObserverBridge> _bridge;
}

#pragma mark - Public

- (void)observePasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  _bridge.emplace(self, passkeyModel);
}

- (BOOL)isObservingPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  return _bridge.has_value() && _bridge->IsObserving(passkeyModel);
}

#pragma mark - PasskeyModelObserverDelegate

- (void)passKeyModelShuttingDown:(webauthn::PasskeyModel*)passkeyModel {
  _shuttingDownCallCount++;
  _lastShuttingDownModel = passkeyModel;
  _bridge.reset();
}

- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel {
  _readyCallCount++;
  _lastReadyModel = passkeyModel;
}

- (void)passkeyModelDidChange {
  _didChangeCallCount++;
}

- (void)passkeyModelDidFailInitialization:
    (webauthn::PasskeyModel*)passkeyModel {
  _failedInitializationCallCount++;
  _lastFailedInitializationModel = passkeyModel;
}

@end

// Test delegate implementing only `@required` `PasskeyModelObserverDelegate`
// methods, omitting `@optional -passkeyModelDidFailInitialization:`.
@interface RequiredOnlyPasskeyModelObserverDelegate
    : NSObject <PasskeyModelObserverDelegate>

@property(nonatomic, readonly) NSUInteger readyCallCount;

@end

@implementation RequiredOnlyPasskeyModelObserverDelegate

#pragma mark - PasskeyModelObserverDelegate

- (void)passKeyModelShuttingDown:(webauthn::PasskeyModel*)passkeyModel {
  // No-op.
}

- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel {
  _readyCallCount++;
}

- (void)passkeyModelDidChange {
  // No-op.
}

@end

namespace {

using PasskeyModelObserverBridgeTest = PlatformTest;

// Tests that `OnPasskeyModelIsReady(true)` invokes `-passkeyModelIsReady:` and
// does not invoke `-passkeyModelDidFailInitialization:`.
TEST_F(PasskeyModelObserverBridgeTest, ForwardsReadinessWhenReady) {
  webauthn::TestPasskeyModel passkey_model;
  passkey_model.SetReady(false);

  FakePasskeyModelObserverDelegate* delegate =
      [[FakePasskeyModelObserverDelegate alloc] init];
  [delegate observePasskeyModel:&passkey_model];

  passkey_model.SetReady(true);
  EXPECT_EQ(1u, delegate.readyCallCount);
  EXPECT_EQ(&passkey_model, delegate.lastReadyModel);
  EXPECT_EQ(0u, delegate.failedInitializationCallCount);
}

// Tests that `OnPasskeyModelIsReady(false)` invokes
// `-passkeyModelDidFailInitialization:` instead of `-passkeyModelIsReady:`.
TEST_F(PasskeyModelObserverBridgeTest,
       ForwardsFailedInitializationWhenImplemented) {
  webauthn::TestPasskeyModel passkey_model;
  passkey_model.SetReady(false);

  FakePasskeyModelObserverDelegate* delegate =
      [[FakePasskeyModelObserverDelegate alloc] init];
  [delegate observePasskeyModel:&passkey_model];

  passkey_model.SetReady(false);
  EXPECT_EQ(0u, delegate.readyCallCount);
  EXPECT_EQ(1u, delegate.failedInitializationCallCount);
  EXPECT_EQ(&passkey_model, delegate.lastFailedInitializationModel);
}

// Tests that `OnPasskeyModelIsReady(false)` is safely ignored when the delegate
// does not implement `@optional -passkeyModelDidFailInitialization:`.
TEST_F(PasskeyModelObserverBridgeTest,
       IgnoresFailedInitializationWhenOptionalMethodOmitted) {
  webauthn::TestPasskeyModel failed_model;
  failed_model.SetReady(false);
  RequiredOnlyPasskeyModelObserverDelegate* delegate =
      [[RequiredOnlyPasskeyModelObserverDelegate alloc] init];
  PasskeyModelObserverBridge failed_bridge(delegate, &failed_model);

  failed_model.SetReady(false);
  EXPECT_EQ(0u, delegate.readyCallCount);

  webauthn::TestPasskeyModel ready_model;
  ready_model.SetReady(false);
  PasskeyModelObserverBridge ready_bridge(delegate, &ready_model);

  ready_model.SetReady(true);
  EXPECT_EQ(1u, delegate.readyCallCount);
}

// Tests that `IsObserving`, `OnPasskeysChanged`, and
// `OnPasskeyModelShuttingDown` forward events to the delegate and support
// bridge teardown during shutdown.
TEST_F(PasskeyModelObserverBridgeTest, ForwardsChangesAndShutdown) {
  std::optional<webauthn::TestPasskeyModel> passkey_model;
  passkey_model.emplace();
  webauthn::TestPasskeyModel other_model;
  FakePasskeyModelObserverDelegate* delegate =
      [[FakePasskeyModelObserverDelegate alloc] init];
  [delegate observePasskeyModel:&*passkey_model];

  EXPECT_TRUE([delegate isObservingPasskeyModel:&*passkey_model]);
  EXPECT_FALSE([delegate isObservingPasskeyModel:&other_model]);

  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id("sync_id_16_bytes");
  passkey.set_credential_id("cred_id_16_bytes");
  passkey.set_rp_id("example.com");
  passkey_model->AddNewPasskeyForTesting(passkey);
  EXPECT_EQ(1u, delegate.didChangeCallCount);

  webauthn::PasskeyModel* expected_model = &*passkey_model;
  passkey_model.reset();

  EXPECT_EQ(1u, delegate.shuttingDownCallCount);
  EXPECT_EQ(expected_model, delegate.lastShuttingDownModel);
  EXPECT_FALSE([delegate isObservingPasskeyModel:expected_model]);
}

// Tests that when `PasskeyModel` shuts down before `PasskeyModelObserverBridge`
// is destroyed, the bridge detaches cleanly without leaving a dangling pointer
// or stale `IsObserving` state.
TEST_F(PasskeyModelObserverBridgeTest,
       BridgeOutlivingModelShutdownStopsObserving) {
  std::optional<webauthn::TestPasskeyModel> passkey_model;
  passkey_model.emplace();
  RequiredOnlyPasskeyModelObserverDelegate* delegate =
      [[RequiredOnlyPasskeyModelObserverDelegate alloc] init];
  PasskeyModelObserverBridge bridge(delegate, &*passkey_model);

  webauthn::PasskeyModel* expected_model = &*passkey_model;
  EXPECT_TRUE(bridge.IsObserving(expected_model));
  EXPECT_FALSE(bridge.IsObserving(nullptr));

  passkey_model.reset();

  EXPECT_FALSE(bridge.IsObserving(expected_model));
  EXPECT_FALSE(bridge.IsObserving(nullptr));
}

}  // namespace
