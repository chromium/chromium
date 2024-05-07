// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/android_video_surface_chooser_impl.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/android/mock_android_overlay.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Values;
using ::testing::_;

namespace {
using ::media::AndroidOverlay;
using ::media::MockAndroidOverlay;

class MockClient {
 public:
  MOCK_METHOD1(UseOverlay, void(AndroidOverlay*));

  void UseOverlayImpl(std::unique_ptr<AndroidOverlay> overlay) {
    UseOverlay(overlay.get());

    // Also take ownership of the overlay, so that it's not destroyed.
    overlay_ = std::move(overlay);
  }

  // Note that this won't clear |overlay_|, which is helpful.
  MOCK_METHOD0(UseTextureOwner, void(void));

  // Let the test have the overlay.
  std::unique_ptr<AndroidOverlay> ReleaseOverlay() {
    return std::move(overlay_);
  }

 private:
  std::unique_ptr<AndroidOverlay> overlay_;
};

// Strongly-typed enums for TestParams.  It would be nice if Values() didn't
// do something that causes these to work anyway if you mismatch them.  Maybe
// it'll work better in a future gtest.  At the very least, it's a lot more
// readable than 'true' and 'false' in the test instantiations.
//
// Outputs from the chooser.
enum class ShouldUseOverlay { No, Yes };
enum class ShouldBePowerEfficient { No, Yes, Ignored /* for clarity */ };
// Inputs to the chooser.
enum class AllowDynamic { No, Yes };
enum class IsFullscreen { No, Yes };
enum class IsRequired { No, Yes };
enum class IsSecure { No, Yes };
enum class IsCCPromotable { No, Yes };
enum class IsExpectingRelayout { No, Yes };
enum class PromoteSecureOnly { No, Yes };
// Since gtest only supports ten args, combine some uncommon ones.
enum class MiscFlags { None, Rotated, Persistent };

// Allow any misc flag values.
#define AnyMisc \
  Values(MiscFlags::None, MiscFlags::Rotated, MiscFlags::Persistent)

using TestParams = std::tuple<ShouldUseOverlay,
                              ShouldBePowerEfficient,
                              AllowDynamic,
                              IsRequired,
                              IsFullscreen,
                              IsSecure,
                              IsCCPromotable,
                              IsExpectingRelayout,
                              PromoteSecureOnly,
                              MiscFlags>;

// Useful macro for instantiating tests.
#define Either(x) Values(x::No, x::Yes)

// Check if a parameter of type |type| is Yes.  |n| is the location of the
// parameter of that type.
// c++14 can remove |n|, and std::get() by type.
#define IsYes(type, n) (::testing::get<n>(GetParam()) == type::Yes)
#define IsIgnored(type, n) (::testing::get<n>(GetParam()) == type::Ignored)
// |v| is the value to check for equality.
#define IsEqual(type, n, v) (::testing::get<n>(GetParam()) == type::v)

}  // namespace

namespace media {

// Unit tests for AndroidVideoSurfaceChooserImpl
class AndroidVideoSurfaceChooserImplTest
    : public testing::TestWithParam<TestParams> {
 public:
  ~AndroidVideoSurfaceChooserImplTest() override {}

  void SetUp() override {
    overlay_ = std::make_unique<MockAndroidOverlay>();

    // Advance the clock just so we're not at 0.
    tick_clock_.Advance(base::Seconds(10));

    // Don't prevent promotions because of the compositor.
    chooser_state_.is_compositor_promotable = true;

    // We create a destruction observer.  By default, the overlay must not be
    // destroyed until the test completes.  Of course, the test may ask the
    // observer to expect something else.
    destruction_observer_ = overlay_->CreateDestructionObserver();
    destruction_observer_->DoNotAllowDestruction();
    overlay_callbacks_ = overlay_->GetCallbacks();
  }

  void TearDown() override {
    // If we get this far, the assume that whatever |destruction_observer_|
    // was looking for should have already happened.  We don't want the
    // lifetime of the observer to matter with respect to the overlay when
    // checking expectations.
    // Note that it might already be null.
    destruction_observer_ = nullptr;
  }

  // Start the chooser, providing |factory| as the initial factory.
  void StartChooser(AndroidOverlayFactoryCB factory) {
    chooser_ = std::make_unique<AndroidVideoSurfaceChooserImpl>(allow_dynamic_,
                                                                &tick_clock_);
    chooser_->SetClientCallbacks(
        base::BindRepeating(&MockClient::UseOverlayImpl,
                            base::Unretained(&client_)),
        base::BindRepeating(&MockClient::UseTextureOwner,
                            base::Unretained(&client_)));
    chooser_->UpdateState(
        factory ? std::make_optional(std::move(factory)) : std::nullopt,
        chooser_state_);
  }

  // Start the chooser with |overlay_|, and verify that the client is told to
  // use it.  As a convenience, return the overlay raw ptr.
  MockAndroidOverlay* StartChooserAndProvideOverlay() {
    MockAndroidOverlay* overlay = overlay_.get();

    EXPECT_CALL(*this, MockOnOverlayCreated());
    StartChooser(FactoryFor(std::move(overlay_)));
    testing::Mock::VerifyAndClearExpectations(&client_);
    testing::Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(client_, UseOverlay(NotNull()));
    overlay_callbacks_.OverlayReady.Run();

    return overlay;
  }

  // AndroidOverlayFactoryCB is a RepeatingCallback, so we can't just bind
  // something that uses unique_ptr.  RepeatingCallback needs to copy it.
  class Factory {
   public:
    Factory(std::unique_ptr<MockAndroidOverlay> overlay,
            base::RepeatingCallback<void()> create_overlay_cb)
        : overlay_(std::move(overlay)),
          create_overlay_cb_(std::move(create_overlay_cb)) {}

    // Return whatever overlay we're given.  This is used to construct factory
    // callbacks for the chooser.
    std::unique_ptr<AndroidOverlay> ReturnOverlay(AndroidOverlayConfig config) {
      // Notify the mock.
      create_overlay_cb_.Run();
      if (overlay_)
        overlay_->SetConfig(std::move(config));
      return std::move(overlay_);
    }

   private:
    std::unique_ptr<MockAndroidOverlay> overlay_;
    base::RepeatingCallback<void()> create_overlay_cb_;
  };

  // Create a factory that will return |overlay| when run.
  AndroidOverlayFactoryCB FactoryFor(
      std::unique_ptr<MockAndroidOverlay> overlay) {
    Factory* factory = new Factory(
        std::move(overlay),
        base::BindRepeating(
            &AndroidVideoSurfaceChooserImplTest::MockOnOverlayCreated,
            base::Unretained(this)));

    // Leaky!
    return base::BindRepeating(&Factory::ReturnOverlay,
                               base::Unretained(factory));
  }

  // Called by the factory when it's run.
  MOCK_METHOD0(MockOnOverlayCreated, void());

  std::unique_ptr<AndroidVideoSurfaceChooserImpl> chooser_;
  StrictMock<MockClient> client_;
  std::unique_ptr<MockAndroidOverlay> overlay_;

  // Callbacks to control the overlay that will be vended by |factory_|
  MockAndroidOverlay::Callbacks overlay_callbacks_;

  std::unique_ptr<DestructionObserver> destruction_observer_;

  // Will the chooser created by StartChooser() support dynamic surface changes?
  bool allow_dynamic_ = true;

  base::SimpleTestTickClock tick_clock_;

  AndroidVideoSurfaceChooser::State chooser_state_;
};

TEST_F(AndroidVideoSurfaceChooserImplTest,
       InitializeWithoutFactoryUsesTextureOwner) {
  // Calling Initialize() with no factory should result in a callback to use
  // texture owner.
  EXPECT_CALL(client_, UseTextureOwner());
  StartChooser(AndroidOverlayFactoryCB());
}

TEST_F(AndroidVideoSurfaceChooserImplTest, NullInitialOverlayUsesTextureOwner) {
  // If we provide a factory, but it fails to create an overlay, then |client_|
  // should be notified to use a texture owner.

  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseTextureOwner());
  StartChooser(FactoryFor(nullptr));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       FailedInitialOverlayUsesTextureOwner) {
  // If we provide a factory, but the overlay that it provides returns 'failed',
  // then |client_| should use texture owner.  Also check that it won't retry
  // after a failed overlay too soon.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(*this, MockOnOverlayCreated());
  StartChooser(FactoryFor(std::move(overlay_)));

  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // The overlay may be destroyed at any time after we send OverlayFailed.  It
  // doesn't have to be destroyed.  We just care that it hasn't been destroyed
  // before now.
  destruction_observer_ = nullptr;
  EXPECT_CALL(client_, UseTextureOwner());
  overlay_callbacks_.OverlayFailed.Run();
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Try to get it to choose again, which shouldn't do anything.
  tick_clock_.Advance(base::Seconds(2));
  EXPECT_CALL(*this, MockOnOverlayCreated()).Times(0);
  chooser_->UpdateState(FactoryFor(nullptr), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Advance some more and try again.  This time, it should request an overlay
  // from the factory.
  tick_clock_.Advance(base::Seconds(100));
  EXPECT_CALL(*this, MockOnOverlayCreated()).Times(1);
  chooser_->UpdateState(FactoryFor(nullptr), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, NullLaterOverlayUsesTextureOwner) {
  // If an overlay factory is provided after startup that returns a null overlay
  // from CreateOverlay, |chooser_| should, at most, notify |client_| to use
  // TextureOwner zero or more times.

  // Start with TextureOwner.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseTextureOwner());
  allow_dynamic_ = true;
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory that will return a null overlay.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseTextureOwner()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(nullptr), chooser_state_);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, FailedLaterOverlayDoesNothing) {
  // If we send an overlay factory that returns an overlay, and that overlay
  // fails, then the client should not be notified except for zero or more
  // callbacks to switch to texture owner.

  // Start with TextureOwner.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseTextureOwner());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseTextureOwner()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(std::move(overlay_)), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Fail the overlay.  We don't care if it's destroyed after that, as long as
  // it hasn't been destroyed yet.
  destruction_observer_ = nullptr;
  overlay_callbacks_.OverlayFailed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       SuccessfulLaterOverlayNotifiesClient) {
  // |client_| is notified if we provide a factory that gets an overlay.

  // Start with TextureOwner.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseTextureOwner());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.  |chooser_| should try to create an overlay.  We don't
  // care if a call to UseTextureOwner is elided or not.  Note that AVDA will
  // ignore duplicate calls anyway (MultipleTextureOwnerCallbacksAreIgnored).
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseTextureOwner()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(std::move(overlay_)), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Notify |chooser_| that the overlay is ready.
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       UpdateStateAfterDeleteRetriesOverlay) {
  // Make sure that SurfaceChooser notices that we delete the overlay, and have
  // switched back to TextureOwner mode.

  chooser_state_.is_fullscreen = true;
  StartChooserAndProvideOverlay();

  // Delete the overlay.
  destruction_observer_ = nullptr;
  client_.ReleaseOverlay();

  // Force chooser to choose again.  We expect that it will retry the overlay,
  // since the delete should have informed it that we've switched back to
  // TextureOwner without a callback from SurfaceChooser.  If it didn't know
  // this, then it would think that the client is still using an overlay, and
  // take no action.

  // Note that if it enforces a delay here before retrying, that might be okay
  // too.  For now, we assume that it doesn't.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  chooser_->UpdateState(std::optional<AndroidOverlayFactoryCB>(),
                        chooser_state_);
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       PowerEffcientOverlayCancelsIfNotPowerEfficient) {
  // If we request a power efficient overlay that later becomes not power
  // efficient, then the client should switch to TextureOwner.
  MockAndroidOverlay* overlay = StartChooserAndProvideOverlay();

  // Verify that this results in a power efficient overlay.  If not, then we've
  // picked the wrong flags, since we're just assuming what state will make the
  // chooser care about power-efficiency.
  ASSERT_TRUE(overlay->config()->power_efficient);

  // Notify the chooser that it's not power efficient anymore.
  EXPECT_CALL(client_, UseTextureOwner());
  overlay_callbacks_.PowerEfficientState.Run(false);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, AlwaysUseTextureOwner) {
  // Start with an overlay.
  chooser_state_.is_fullscreen = true;
  StartChooserAndProvideOverlay();
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Change the state to force texture owner. It should use the TextureOwner
  // instead.
  chooser_state_.always_use_texture_owner = true;
  EXPECT_CALL(client_, UseTextureOwner());
  chooser_->UpdateState(std::nullopt, chooser_state_);
}

TEST_P(AndroidVideoSurfaceChooserImplTest, OverlayIsUsedOrNotBasedOnState) {
  // Provide a factory, and verify that it is used when the state says that it
  // should be.  If the overlay is used, then we also verify that it does not
  // switch to TextureOwner first, since pre-M requires it.

  const bool should_use_overlay = IsYes(ShouldUseOverlay, 0);
  const bool should_be_power_efficient = IsYes(ShouldBePowerEfficient, 1);
  const bool ignore_power_efficient = IsIgnored(ShouldBePowerEfficient, 1);
  allow_dynamic_ = IsYes(AllowDynamic, 2);
  chooser_state_.is_required = IsYes(IsRequired, 3);
  chooser_state_.is_fullscreen = IsYes(IsFullscreen, 4);
  chooser_state_.is_secure = IsYes(IsSecure, 5);
  chooser_state_.is_compositor_promotable = IsYes(IsCCPromotable, 6);
  chooser_state_.is_expecting_relayout = IsYes(IsExpectingRelayout, 7);
  chooser_state_.promote_secure_only = IsYes(PromoteSecureOnly, 8);
  chooser_state_.video_rotation =
      IsEqual(MiscFlags, 9, Rotated) ? VIDEO_ROTATION_90 : VIDEO_ROTATION_0;
  chooser_state_.is_persistent_video = IsEqual(MiscFlags, 9, Persistent);

  MockAndroidOverlay* overlay = overlay_.get();

  if (should_use_overlay) {
    EXPECT_CALL(client_, UseTextureOwner()).Times(0);
    EXPECT_CALL(*this, MockOnOverlayCreated());
  } else {
    EXPECT_CALL(client_, UseTextureOwner());
    EXPECT_CALL(*this, MockOnOverlayCreated()).Times(0);
  }

  StartChooser(FactoryFor(std::move(overlay_)));

  // Check that the overlay config has the right power-efficient state set.
  if (should_use_overlay && !ignore_power_efficient)
    ASSERT_EQ(should_be_power_efficient, overlay->config()->power_efficient);

  // Verify that the overlay is provided when it becomes ready.
  if (should_use_overlay) {
    EXPECT_CALL(client_, UseOverlay(NotNull()));
    overlay_callbacks_.OverlayReady.Run();
  }
}

// We should default to TextureOwner if we only promote secure video.
INSTANTIATE_TEST_SUITE_P(NoFullscreenUsesTextureOwner,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::No),
                                 Values(ShouldBePowerEfficient::Ignored),
                                 Either(AllowDynamic),
                                 Values(IsRequired::No),
                                 Values(IsFullscreen::No),
                                 Values(IsSecure::No),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Values(PromoteSecureOnly::Yes),
                                 AnyMisc));

INSTANTIATE_TEST_SUITE_P(FullscreenUsesOverlay,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::Ignored),
                                 Either(AllowDynamic),
                                 Either(IsRequired),
                                 Values(IsFullscreen::Yes),
                                 Values(IsSecure::No),
                                 Values(IsCCPromotable::Yes),
                                 Values(IsExpectingRelayout::No),
                                 Values(PromoteSecureOnly::No),
                                 Values(MiscFlags::None)));

INSTANTIATE_TEST_SUITE_P(RequiredUsesOverlay,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::Yes),
                                 Values(IsRequired::Yes),
                                 Either(IsFullscreen),
                                 Either(IsSecure),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::None,
                                        MiscFlags::Persistent)));

// Secure textures should use an overlay if the compositor will promote them.
// We don't care about relayout, since it's transient; either behavior is okay
// if a relayout is expected.  Similarly, hidden frames are fine either way.
INSTANTIATE_TEST_SUITE_P(SecureUsesOverlayIfPromotable,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::No),
                                 Either(AllowDynamic),
                                 Either(IsRequired),
                                 Either(IsFullscreen),
                                 Values(IsSecure::Yes),
                                 Values(IsCCPromotable::Yes),
                                 Values(IsExpectingRelayout::No),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::None)));

// For all dynamic cases, we shouldn't use an overlay if the compositor won't
// promote it, unless it's marked as required.  This includes secure surfaces,
// so that L3 will fall back to TextureOwner.  Non-dynamic is excluded, since
// we don't get (or use) compositor feedback before the first frame.  At that
// point, we've already chosen the output surface and can't switch it.
INSTANTIATE_TEST_SUITE_P(NotCCPromotableNotRequiredUsesTextureOwner,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::No),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::Yes),
                                 Values(IsRequired::No),
                                 Either(IsFullscreen),
                                 Either(IsSecure),
                                 Values(IsCCPromotable::No),
                                 Either(IsExpectingRelayout),
                                 Either(PromoteSecureOnly),
                                 AnyMisc));

// If we're expecting a relayout, then we should never use an overlay unless
// it's required.
INSTANTIATE_TEST_SUITE_P(InsecureExpectingRelayoutUsesTextureOwner,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::No),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::Yes),
                                 Values(IsRequired::No),
                                 Either(IsFullscreen),
                                 Either(IsSecure),
                                 Either(IsCCPromotable),
                                 Values(IsExpectingRelayout::Yes),
                                 Either(PromoteSecureOnly),
                                 AnyMisc));

// "is_fullscreen" should be enough to trigger an overlay pre-M.
INSTANTIATE_TEST_SUITE_P(NotDynamicInFullscreenUsesOverlay,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::No),
                                 Either(IsRequired),
                                 Values(IsFullscreen::Yes),
                                 Either(IsSecure),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Values(PromoteSecureOnly::No),
                                 Values(MiscFlags::None,
                                        MiscFlags::Persistent)));

// "is_secure" should be enough to trigger an overlay pre-M.
INSTANTIATE_TEST_SUITE_P(NotDynamicSecureUsesOverlay,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::No),
                                 Either(IsRequired),
                                 Either(IsFullscreen),
                                 Values(IsSecure::Yes),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::None,
                                        MiscFlags::Persistent)));

// "is_required" should be enough to trigger an overlay pre-M.
INSTANTIATE_TEST_SUITE_P(NotDynamicRequiredUsesOverlay,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::No),
                                 Values(AllowDynamic::No),
                                 Values(IsRequired::Yes),
                                 Either(IsFullscreen),
                                 Either(IsSecure),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::None,
                                        MiscFlags::Persistent)));

// Overlay should request power efficient by default.
INSTANTIATE_TEST_SUITE_P(AggressiveOverlayIsPowerEfficient,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::Yes),
                                 Values(ShouldBePowerEfficient::Yes),
                                 Values(AllowDynamic::Yes),
                                 Values(IsRequired::No),
                                 Values(IsFullscreen::No),
                                 Values(IsSecure::No),
                                 Values(IsCCPromotable::Yes),
                                 Values(IsExpectingRelayout::No),
                                 Values(PromoteSecureOnly::No),
                                 Values(MiscFlags::None)));

// Rotated video is unsupported for overlays in all cases.
INSTANTIATE_TEST_SUITE_P(IsVideoRotatedUsesTextureOwner,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::No),
                                 Either(ShouldBePowerEfficient),
                                 Either(AllowDynamic),
                                 Either(IsRequired),
                                 Either(IsFullscreen),
                                 Either(IsSecure),
                                 Either(IsCCPromotable),
                                 Either(IsExpectingRelayout),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::Rotated)));

// Persistent, non-required video should not use an overlay.
INSTANTIATE_TEST_SUITE_P(FullscreenPersistentVideoUsesSurfaceTexture,
                         AndroidVideoSurfaceChooserImplTest,
                         Combine(Values(ShouldUseOverlay::No),
                                 Values(ShouldBePowerEfficient::Ignored),
                                 Values(AllowDynamic::Yes),
                                 Values(IsRequired::No),
                                 Values(IsFullscreen::Yes),
                                 Either(IsSecure),
                                 Values(IsCCPromotable::Yes),
                                 Values(IsExpectingRelayout::No),
                                 Either(PromoteSecureOnly),
                                 Values(MiscFlags::Persistent)));

}  // namespace media
