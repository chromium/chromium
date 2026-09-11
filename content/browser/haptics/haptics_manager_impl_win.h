// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_WIN_H_
#define CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_WIN_H_

#include <windows.devices.haptics.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/auto_reset.h"
#include "base/sequence_checker.h"
#include "content/browser/haptics/haptics_manager.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/haptics/haptics.mojom.h"

namespace content {

// Test seam for the WinRT activation-factory statics. Production resolves these
// with base::win::GetActivationFactory; unit tests install a provider that
// hands back fakes so the WinRT-consuming code paths run without real hardware.
class HapticsWinrtStaticsProvider {
 public:
  virtual ~HapticsWinrtStaticsProvider() = default;

  virtual HRESULT GetInputHapticsManagerStatics(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Haptics::IInputHapticsManagerStatics>*
          statics) = 0;
  virtual HRESULT GetKnownWaveformsStatics2(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Haptics::
              IKnownSimpleHapticsControllerWaveformsStatics2>* statics) = 0;
};

// Windows backend for the Web Haptics API, backed by the
// Windows.Devices.Haptics.InputHapticsManager WinRT API. Owned by and called
// in-process from content::HapticsServiceImpl.
//
// InputHapticsManager is bound to the thread whose window receives pointer
// input, so all WinRT interaction must happen on the browser UI thread. It
// targets the most recent input device.
class CONTENT_EXPORT HapticsManagerImplWin : public HapticsManager {
 public:
  HapticsManagerImplWin();
  HapticsManagerImplWin(const HapticsManagerImplWin&) = delete;
  HapticsManagerImplWin& operator=(const HapticsManagerImplWin&) = delete;
  ~HapticsManagerImplWin() override;

  // HapticsManager:
  void PlayHaptics(blink::mojom::HapticEffect effect,
                   double intensity) override;

  // Installs a provider that supplies the WinRT statics, replacing the real
  // activation factories, for the lifetime of the returned base::AutoReset.
  [[nodiscard]] static base::AutoReset<HapticsWinrtStaticsProvider*>
  SetStaticsProviderForTesting(HapticsWinrtStaticsProvider* provider);

 private:
  // Lazily resolves the InputHapticsManager statics and the known-waveform
  // statics. Returns false if the platform does not support the API.
  bool EnsureStatics();

  // Performs one throwaway controller lookup at construction so the platform
  // caches a warm controller and the first real PlayHaptics is not dropped.
  void PrimeHapticsController();

  // Returns the cached waveform for |effect|. Failed lookups are retried.
  std::optional<uint16_t> WaveformForEffect(blink::mojom::HapticEffect effect);

  // Looks up the waveform for |effect| in the WinRT statics.
  std::optional<uint16_t> ComputeWaveformForEffect(
      blink::mojom::HapticEffect effect);

  // Returns the device-type default waveform used when the current device does
  // not advertise the semantic waveform: Pen -> Click, Mouse/Touchpad/Generic
  // -> Hover. Returns std::nullopt if the waveform statics are unavailable or
  // the device is not haptics-capable.
  std::optional<uint16_t> DefaultWaveformForDevice(
      ABI::Windows::Devices::Haptics::HapticDeviceType device_type);

  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Haptics::IInputHapticsManagerStatics>
      input_haptics_statics_;
  // Known-waveform statics (Hover). Collide/Step/Align and Click are reached by
  // QueryInterface from this same object; see haptics_waveforms_statics3_win.h.
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Haptics::
                             IKnownSimpleHapticsControllerWaveformsStatics2>
      known_waveforms2_;

  // Whether statics resolution has already been attempted (success or failure).
  bool statics_requested_ = false;

  // Per-effect waveform ids, each cached on first successful resolution.
  static constexpr size_t kHapticEffectCount =
      static_cast<size_t>(blink::mojom::HapticEffect::kMaxValue) + 1;
  std::array<std::optional<uint16_t>, kHapticEffectCount> waveform_cache_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_WIN_H_
