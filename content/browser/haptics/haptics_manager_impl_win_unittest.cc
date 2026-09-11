// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_manager_impl_win.h"

#include <windows.devices.haptics.h>
#include <windows.foundation.collections.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "content/browser/haptics/haptics_waveforms_statics3_win.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/haptics/haptics.mojom.h"

namespace content {
namespace {

namespace haptics = ABI::Windows::Devices::Haptics;
namespace collections = ABI::Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::InhibitRoOriginateError;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRt;

// Semantic waveform ids the fake KnownWaveforms statics report for each effect,
// plus the device-type default waveforms (Hover for Mouse/Touchpad, Click for
// Pen).
constexpr uint16_t kHoverId = 10;
constexpr uint16_t kCollideId = 11;
constexpr uint16_t kStepId = 12;
constexpr uint16_t kAlignId = 13;
constexpr uint16_t kClickId = 14;

class FakeSimpleHapticsControllerFeedback
    : public RuntimeClass<RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
                          haptics::ISimpleHapticsControllerFeedback> {
 public:
  explicit FakeSimpleHapticsControllerFeedback(uint16_t waveform)
      : waveform_(waveform) {}

  IFACEMETHODIMP get_Waveform(UINT16* value) override {
    *value = waveform_;
    return S_OK;
  }
  IFACEMETHODIMP get_Duration(
      ABI::Windows::Foundation::TimeSpan* value) override {
    return E_NOTIMPL;
  }

 private:
  uint16_t waveform_;
};

// Minimal IVectorView over a fixed set of feedback objects. The haptics SDK
// only specializes IVectorView (not IVector) for this element type, so
// base::win::Vector cannot be used here.
class FakeSupportedFeedbackView
    : public RuntimeClass<
          RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
          collections::IVectorView<haptics::SimpleHapticsControllerFeedback*>> {
 public:
  explicit FakeSupportedFeedbackView(
      std::vector<ComPtr<haptics::ISimpleHapticsControllerFeedback>> feedbacks)
      : feedbacks_(std::move(feedbacks)) {}

  IFACEMETHODIMP GetAt(
      unsigned index,
      haptics::ISimpleHapticsControllerFeedback** item) override {
    if (index >= feedbacks_.size()) {
      return E_BOUNDS;
    }
    return feedbacks_[index].CopyTo(item);
  }
  IFACEMETHODIMP get_Size(unsigned* size) override {
    *size = static_cast<unsigned>(feedbacks_.size());
    return S_OK;
  }
  IFACEMETHODIMP IndexOf(haptics::ISimpleHapticsControllerFeedback*,
                         unsigned*,
                         boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetMany(unsigned,
                         unsigned,
                         haptics::ISimpleHapticsControllerFeedback**,
                         unsigned*) override {
    return E_NOTIMPL;
  }

 private:
  std::vector<ComPtr<haptics::ISimpleHapticsControllerFeedback>> feedbacks_;
};

class FakeSimpleHapticsController
    : public RuntimeClass<RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
                          haptics::ISimpleHapticsController> {
 public:
  explicit FakeSimpleHapticsController(std::vector<uint16_t> supported)
      : supported_(std::move(supported)) {}

  IFACEMETHODIMP get_SupportedFeedback(
      collections::IVectorView<haptics::SimpleHapticsControllerFeedback*>**
          value) override {
    std::vector<ComPtr<haptics::ISimpleHapticsControllerFeedback>> feedbacks;
    for (uint16_t waveform : supported_) {
      feedbacks.push_back(Make<FakeSimpleHapticsControllerFeedback>(waveform));
    }
    return Make<FakeSupportedFeedbackView>(std::move(feedbacks)).CopyTo(value);
  }

  IFACEMETHODIMP get_Id(HSTRING*) override { return E_NOTIMPL; }
  IFACEMETHODIMP get_IsIntensitySupported(boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_IsPlayCountSupported(boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_IsPlayDurationSupported(boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_IsReplayPauseIntervalSupported(boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP StopFeedback() override { return E_NOTIMPL; }
  IFACEMETHODIMP SendHapticFeedback(
      haptics::ISimpleHapticsControllerFeedback*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SendHapticFeedbackWithIntensity(
      haptics::ISimpleHapticsControllerFeedback*,
      DOUBLE) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SendHapticFeedbackForDuration(
      haptics::ISimpleHapticsControllerFeedback*,
      DOUBLE,
      ABI::Windows::Foundation::TimeSpan) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SendHapticFeedbackForPlayCount(
      haptics::ISimpleHapticsControllerFeedback*,
      DOUBLE,
      INT32,
      ABI::Windows::Foundation::TimeSpan) override {
    return E_NOTIMPL;
  }

 private:
  std::vector<uint16_t> supported_;
};

class FakeInputHapticsManager
    : public RuntimeClass<RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
                          haptics::IInputHapticsManager> {
 public:
  FakeInputHapticsManager(ComPtr<haptics::ISimpleHapticsController> controller,
                          haptics::HapticDeviceType device_type,
                          std::vector<uint16_t> supported)
      : controller_(std::move(controller)),
        device_type_(device_type),
        supported_(std::move(supported)) {}

  IFACEMETHODIMP get_CurrentHapticsControllerDeviceType(
      haptics::HapticDeviceType* value) override {
    *value = device_type_;
    return S_OK;
  }

  IFACEMETHODIMP get_CurrentHapticsController(
      haptics::ISimpleHapticsController** value) override {
    ++controller_query_count_;
    return controller_.CopyTo(value);
  }

  IFACEMETHODIMP TrySendHapticWaveformWithIntensity(UINT16 waveform,
                                                    UINT16 waveform_fallback,
                                                    DOUBLE intensity,
                                                    boolean* result) override {
    ++play_count_;
    last_waveform_ = waveform;
    last_fallback_ = waveform_fallback;
    last_intensity_ = intensity;
    // Model the platform: it plays only if the primary or the fallback waveform
    // is actually advertised by the device.
    auto is_supported = [this](uint16_t w) {
      return std::find(supported_.begin(), supported_.end(), w) !=
             supported_.end();
    };
    last_sent_ = is_supported(waveform) || is_supported(waveform_fallback);
    *result = last_sent_ ? TRUE : FALSE;
    return S_OK;
  }

  IFACEMETHODIMP get_ThreadId(UINT32*) override { return E_NOTIMPL; }
  IFACEMETHODIMP TrySendHapticWaveform(UINT16, UINT16, boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP TrySendHapticWaveformForDuration(
      UINT16,
      UINT16,
      DOUBLE,
      ABI::Windows::Foundation::TimeSpan,
      boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP TrySendHapticWaveformForPlayCount(
      UINT16,
      UINT16,
      DOUBLE,
      INT32,
      ABI::Windows::Foundation::TimeSpan,
      boolean*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP TryStopFeedback(boolean*) override { return E_NOTIMPL; }
  IFACEMETHODIMP SetOverrideHapticsController(
      haptics::HapticDeviceType,
      haptics::ISimpleHapticsController*,
      haptics::HapticsControllerOverrideToken*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP ClearOverrideHapticsController(
      haptics::HapticsControllerOverrideToken) override {
    return E_NOTIMPL;
  }

  int play_count() const { return play_count_; }
  int controller_query_count() const { return controller_query_count_; }
  uint16_t last_waveform() const { return last_waveform_; }
  uint16_t last_fallback() const { return last_fallback_; }
  double last_intensity() const { return last_intensity_; }
  bool last_sent() const { return last_sent_; }

 private:
  ComPtr<haptics::ISimpleHapticsController> controller_;
  haptics::HapticDeviceType device_type_;
  std::vector<uint16_t> supported_;
  int play_count_ = 0;
  int controller_query_count_ = 0;
  uint16_t last_waveform_ = 0;
  uint16_t last_fallback_ = 0;
  double last_intensity_ = 0.0;
  bool last_sent_ = false;
};

class FakeInputHapticsManagerStatics
    : public RuntimeClass<RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
                          haptics::IInputHapticsManagerStatics> {
 public:
  explicit FakeInputHapticsManagerStatics(
      ComPtr<haptics::IInputHapticsManager> manager)
      : manager_(std::move(manager)) {}

  void set_fail_get_for_current_thread(bool fail) {
    fail_get_for_current_thread_ = fail;
  }

  IFACEMETHODIMP GetForCurrentThread(
      haptics::IInputHapticsManager** result) override {
    if (fail_get_for_current_thread_) {
      *result = nullptr;
      return E_FAIL;
    }
    return manager_.CopyTo(result);
  }

  IFACEMETHODIMP IsSupported(boolean*) override { return E_NOTIMPL; }
  IFACEMETHODIMP IsHapticDevicePresent(boolean*) override { return E_NOTIMPL; }
  IFACEMETHODIMP TryGetForThread(UINT32,
                                 haptics::IInputHapticsManager**) override {
    return E_NOTIMPL;
  }

 private:
  ComPtr<haptics::IInputHapticsManager> manager_;
  bool fail_get_for_current_thread_ = false;
};

// Getters the backend never calls.
#define WAVEFORM_STUB(name)               \
  IFACEMETHODIMP name(UINT16*) override { \
    return E_NOTIMPL;                     \
  }

// Models a Windows 24H2+ system: also implements the contract-19 Statics3
// interface, so Collide/Step/Align resolve via QueryInterface. Also implements
// the base v1 statics so the device-type default (Pen -> Click) resolves.
class FakeKnownWaveformsWithStatics3
    : public RuntimeClass<
          RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
          haptics::IKnownSimpleHapticsControllerWaveformsStatics2,
          haptics::IKnownSimpleHapticsControllerWaveformsStatics,
          IKnownSimpleHapticsControllerWaveformsStatics3> {
 public:
  IFACEMETHODIMP get_Hover(UINT16* value) override {
    *value = kHoverId;
    return S_OK;
  }
  IFACEMETHODIMP get_Click(UINT16* value) override {
    *value = kClickId;
    return S_OK;
  }
  IFACEMETHODIMP get_Collide(UINT16* value) override {
    *value = kCollideId;
    return S_OK;
  }
  IFACEMETHODIMP get_Align(UINT16* value) override {
    *value = kAlignId;
    return S_OK;
  }
  IFACEMETHODIMP get_Step(UINT16* value) override {
    *value = kStepId;
    return S_OK;
  }
  WAVEFORM_STUB(get_Grow)

  // IKnownSimpleHapticsControllerWaveformsStatics (base v1) stubs.
  WAVEFORM_STUB(get_BuzzContinuous)
  WAVEFORM_STUB(get_RumbleContinuous)
  WAVEFORM_STUB(get_Press)
  WAVEFORM_STUB(get_Release)
  // IKnownSimpleHapticsControllerWaveformsStatics2 stubs.
  WAVEFORM_STUB(get_BrushContinuous)
  WAVEFORM_STUB(get_ChiselMarkerContinuous)
  WAVEFORM_STUB(get_EraserContinuous)
  WAVEFORM_STUB(get_Error)
  WAVEFORM_STUB(get_GalaxyPenContinuous)
  WAVEFORM_STUB(get_InkContinuous)
  WAVEFORM_STUB(get_MarkerContinuous)
  WAVEFORM_STUB(get_PencilContinuous)
  WAVEFORM_STUB(get_Success)
};

// Models a pre-24H2 system: implements only Statics2 and the base v1 statics,
// so the Statics3 QueryInterface fails and non-hint effects fall back to the
// device-type default.
class FakeKnownWaveformsStatics2Only
    : public RuntimeClass<
          RuntimeClassFlags<WinRt | InhibitRoOriginateError>,
          haptics::IKnownSimpleHapticsControllerWaveformsStatics2,
          haptics::IKnownSimpleHapticsControllerWaveformsStatics> {
 public:
  IFACEMETHODIMP get_Hover(UINT16* value) override {
    *value = kHoverId;
    return S_OK;
  }
  IFACEMETHODIMP get_Click(UINT16* value) override {
    *value = kClickId;
    return S_OK;
  }

  // IKnownSimpleHapticsControllerWaveformsStatics (base v1) stubs.
  WAVEFORM_STUB(get_BuzzContinuous)
  WAVEFORM_STUB(get_RumbleContinuous)
  WAVEFORM_STUB(get_Press)
  WAVEFORM_STUB(get_Release)
  // IKnownSimpleHapticsControllerWaveformsStatics2 stubs.
  WAVEFORM_STUB(get_BrushContinuous)
  WAVEFORM_STUB(get_ChiselMarkerContinuous)
  WAVEFORM_STUB(get_EraserContinuous)
  WAVEFORM_STUB(get_Error)
  WAVEFORM_STUB(get_GalaxyPenContinuous)
  WAVEFORM_STUB(get_InkContinuous)
  WAVEFORM_STUB(get_MarkerContinuous)
  WAVEFORM_STUB(get_PencilContinuous)
  WAVEFORM_STUB(get_Success)
};

#undef WAVEFORM_STUB

class FakeStaticsProvider : public HapticsWinrtStaticsProvider {
 public:
  FakeStaticsProvider(
      ComPtr<haptics::IInputHapticsManagerStatics> haptics_statics,
      ComPtr<haptics::IKnownSimpleHapticsControllerWaveformsStatics2> waveforms)
      : haptics_statics_(std::move(haptics_statics)),
        waveforms_(std::move(waveforms)) {}

  HRESULT GetInputHapticsManagerStatics(
      ComPtr<haptics::IInputHapticsManagerStatics>* statics) override {
    *statics = haptics_statics_;
    return haptics_statics_ ? S_OK : E_FAIL;
  }
  HRESULT GetKnownWaveformsStatics2(
      ComPtr<haptics::IKnownSimpleHapticsControllerWaveformsStatics2>* statics)
      override {
    *statics = waveforms_;
    return S_OK;
  }

 private:
  ComPtr<haptics::IInputHapticsManagerStatics> haptics_statics_;
  ComPtr<haptics::IKnownSimpleHapticsControllerWaveformsStatics2> waveforms_;
};

}  // namespace

class HapticsManagerImplWinTest : public testing::Test {
 protected:
  // Builds the fake WinRT tree, installs the statics provider, and returns the
  // fake manager for assertions. |supported| is the device's advertised
  // waveform set (empty models a device with none). |device_type| is the
  // current input device's type (drives the fallback default). |with_statics3|
  // toggles the 24H2+ vs pre-24H2 waveform statics. |manager_reachable| false
  // models GetForCurrentThread failing.
  FakeInputHapticsManager* InstallDeviceScenario(
      std::vector<uint16_t> supported,
      haptics::HapticDeviceType device_type = haptics::HapticDeviceType_Mouse,
      bool with_statics3 = true,
      bool manager_reachable = true) {
    auto controller = Make<FakeSimpleHapticsController>(supported);
    auto manager =
        Make<FakeInputHapticsManager>(controller, device_type, supported);
    FakeInputHapticsManager* manager_raw = manager.Get();
    auto statics = Make<FakeInputHapticsManagerStatics>(manager);
    statics->set_fail_get_for_current_thread(!manager_reachable);

    ComPtr<haptics::IKnownSimpleHapticsControllerWaveformsStatics2> waveforms;
    if (with_statics3) {
      waveforms = Make<FakeKnownWaveformsWithStatics3>();
    } else {
      waveforms = Make<FakeKnownWaveformsStatics2Only>();
    }

    provider_ = std::make_unique<FakeStaticsProvider>(statics, waveforms);
    provider_reset_ =
        HapticsManagerImplWin::SetStaticsProviderForTesting(provider_.get());
    return manager_raw;
  }

  // Installs a provider whose activation-factory lookup fails, modeling a
  // system that does not support InputHapticsManager (e.g. pre-24H2).
  void InstallUnsupportedPlatform() {
    provider_ = std::make_unique<FakeStaticsProvider>(nullptr, nullptr);
    provider_reset_ =
        HapticsManagerImplWin::SetStaticsProviderForTesting(provider_.get());
  }

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeStaticsProvider> provider_;
  // Declared after |provider_| so it restores the global provider to null
  // before |provider_| frees the fake it points to.
  std::optional<base::AutoReset<HapticsWinrtStaticsProvider*>> provider_reset_;
};

TEST_F(HapticsManagerImplWinTest,
       ForwardsSemanticWaveformWhenDeviceSupportsIt) {
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kHoverId, kCollideId, kStepId, kAlignId});
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 1.0);
  EXPECT_EQ(manager->last_waveform(), kHoverId);
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);
  EXPECT_EQ(manager->last_waveform(), kCollideId);
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kTick, 1.0);
  EXPECT_EQ(manager->last_waveform(), kStepId);
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kAlign, 1.0);
  EXPECT_EQ(manager->last_waveform(), kAlignId);
}

TEST_F(HapticsManagerImplWinTest, FallsBackToDeviceDefaultHoverForMouse) {
  // A mouse advertises Hover but not the requested edge (Collide) waveform.
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kHoverId}, haptics::HapticDeviceType_Mouse);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);

  // Mouse falls back to Hover, not to an arbitrary supported waveform.
  EXPECT_EQ(manager->last_waveform(), kHoverId);
  EXPECT_EQ(manager->last_fallback(), kHoverId);
  EXPECT_TRUE(manager->last_sent());
}

TEST_F(HapticsManagerImplWinTest, NoDeviceDefaultWhenDeviceTypeNone) {
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kHoverId}, haptics::HapticDeviceType_None);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);

  EXPECT_EQ(manager->play_count(), 0);
}

TEST_F(HapticsManagerImplWinTest, FallsBackToDeviceDefaultClickForPen) {
  // A pen advertises Click but not the requested edge (Collide) waveform.
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kClickId}, haptics::HapticDeviceType_Pen);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);

  // Pen falls back to Click.
  EXPECT_EQ(manager->last_waveform(), kClickId);
  EXPECT_EQ(manager->last_fallback(), kClickId);
  EXPECT_TRUE(manager->last_sent());
}

TEST_F(HapticsManagerImplWinTest, PassesDeviceDefaultAsFallbackArgument) {
  // The device advertises the semantic waveform, so it is the primary target,
  // but the device-type default is still passed as the built-in fallback.
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kCollideId}, haptics::HapticDeviceType_Mouse);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);

  EXPECT_EQ(manager->last_waveform(), kCollideId);
  EXPECT_EQ(manager->last_fallback(), kHoverId);
}

TEST_F(HapticsManagerImplWinTest, DropsEffectWhenNoSupportedWaveforms) {
  // The device advertises nothing, so neither the semantic waveform nor the
  // device-type default can play.
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({}, haptics::HapticDeviceType_Mouse);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 1.0);

  EXPECT_FALSE(manager->last_sent());
}

TEST_F(HapticsManagerImplWinTest, DropsEffectWhenNoCurrentInputDevice) {
  FakeInputHapticsManager* manager = InstallDeviceScenario(
      {kHoverId}, haptics::HapticDeviceType_Mouse, /*with_statics3=*/true,
      /*manager_reachable=*/false);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 1.0);

  EXPECT_EQ(manager->play_count(), 0);
}

TEST_F(HapticsManagerImplWinTest, NoOpWhenPlatformUnsupported) {
  InstallUnsupportedPlatform();
  HapticsManagerImplWin haptics_manager;

  // Must not crash when the platform lacks InputHapticsManager.
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 1.0);
}

TEST_F(HapticsManagerImplWinTest, FallsBackToDeviceDefaultWhenStatics3Absent) {
  // Pre-24H2: Hover (Statics2) resolves but Collide/Step/Align (Statics3) do
  // not, so a non-hint effect degrades to the device-type default.
  FakeInputHapticsManager* manager =
      InstallDeviceScenario({kHoverId}, haptics::HapticDeviceType_Mouse,
                            /*with_statics3=*/false);
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 1.0);
  EXPECT_EQ(manager->last_waveform(), kHoverId);

  // kEdge cannot resolve its semantic waveform, so it falls back to the Mouse
  // default (Hover).
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kEdge, 1.0);
  EXPECT_EQ(manager->last_waveform(), kHoverId);
}

TEST_F(HapticsManagerImplWinTest, ClampsAndForwardsIntensity) {
  FakeInputHapticsManager* manager = InstallDeviceScenario({kHoverId});
  HapticsManagerImplWin haptics_manager;

  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 0.25);
  EXPECT_EQ(manager->last_intensity(), 0.25);
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, 2.0);
  EXPECT_EQ(manager->last_intensity(), 1.0);
  haptics_manager.PlayHaptics(blink::mojom::HapticEffect::kHint, -1.0);
  EXPECT_EQ(manager->last_intensity(), 0.0);
}

TEST_F(HapticsManagerImplWinTest, PrimesControllerOnConstruction) {
  FakeInputHapticsManager* manager = InstallDeviceScenario({kHoverId});
  HapticsManagerImplWin haptics_manager;

  // Construction performs one throwaway controller lookup to warm the platform.
  EXPECT_GE(manager->controller_query_count(), 1);
}

}  // namespace content
