// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_manager_impl_win.h"

#include <windows.foundation.collections.h>

#include <algorithm>
#include <vector>

#include "base/notreached.h"
#include "base/win/core_winrt_util.h"
#include "content/browser/haptics/haptics_waveforms_statics3_win.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

namespace haptics = ABI::Windows::Devices::Haptics;
namespace collections = ABI::Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

HapticsWinrtStaticsProvider* g_statics_provider_for_testing = nullptr;

// Collects the waveform ids advertised as supported by the current haptics
// controller for |manager|. Returns an empty vector if none are available.
std::vector<uint16_t> GetSupportedWaveforms(
    haptics::IInputHapticsManager* manager) {
  std::vector<uint16_t> waveforms;
  ComPtr<haptics::ISimpleHapticsController> controller;
  if (FAILED(manager->get_CurrentHapticsController(&controller)) ||
      !controller) {
    return waveforms;
  }
  ComPtr<collections::IVectorView<haptics::SimpleHapticsControllerFeedback*>>
      feedbacks;
  if (FAILED(controller->get_SupportedFeedback(&feedbacks)) || !feedbacks) {
    return waveforms;
  }
  unsigned size = 0;
  feedbacks->get_Size(&size);
  for (unsigned i = 0; i < size; ++i) {
    ComPtr<haptics::ISimpleHapticsControllerFeedback> feedback;
    if (FAILED(feedbacks->GetAt(i, &feedback)) || !feedback) {
      continue;
    }
    UINT16 wf = 0;
    if (SUCCEEDED(feedback->get_Waveform(&wf))) {
      waveforms.push_back(wf);
    }
  }
  return waveforms;
}

}  // namespace

HapticsManagerImplWin::HapticsManagerImplWin() {
  PrimeHapticsController();
}

HapticsManagerImplWin::~HapticsManagerImplWin() = default;

// static
base::AutoReset<HapticsWinrtStaticsProvider*>
HapticsManagerImplWin::SetStaticsProviderForTesting(
    HapticsWinrtStaticsProvider* provider) {
  return base::AutoReset<HapticsWinrtStaticsProvider*>(
      &g_statics_provider_for_testing, provider);
}

void HapticsManagerImplWin::PlayHaptics(blink::mojom::HapticEffect effect,
                                        double intensity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Clamp intensity defensively; the renderer is expected to have clamped it.
  intensity = std::clamp(intensity, 0.0, 1.0);

  if (!EnsureStatics()) {
    return;
  }

  // Target the most recent input device on this (UI) thread's input queue.
  ComPtr<haptics::IInputHapticsManager> manager;
  if (FAILED(input_haptics_statics_->GetForCurrentThread(&manager)) ||
      !manager) {
    return;
  }

  haptics::HapticDeviceType device_type = haptics::HapticDeviceType_None;
  manager->get_CurrentHapticsControllerDeviceType(&device_type);

  std::vector<uint16_t> supported = GetSupportedWaveforms(manager.Get());

  // Use the semantic waveform when advertised; otherwise use the device-type
  // default.
  std::optional<uint16_t> preferred = WaveformForEffect(effect);
  std::optional<uint16_t> device_default =
      DefaultWaveformForDevice(device_type);

  uint16_t target = 0;
  if (preferred && std::ranges::contains(supported, *preferred)) {
    target = *preferred;
  } else if (device_default) {
    target = *device_default;
  } else {
    return;
  }
  uint16_t fallback = device_default.value_or(target);

  boolean sent = false;
  manager->TrySendHapticWaveformWithIntensity(target, fallback, intensity,
                                              &sent);
}

bool HapticsManagerImplWin::EnsureStatics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (input_haptics_statics_) {
    return true;
  }
  if (statics_requested_) {
    return false;
  }
  statics_requested_ = true;

  if (g_statics_provider_for_testing) {
    if (FAILED(g_statics_provider_for_testing->GetInputHapticsManagerStatics(
            &input_haptics_statics_)) ||
        !input_haptics_statics_) {
      input_haptics_statics_.Reset();
      return false;
    }
    g_statics_provider_for_testing->GetKnownWaveformsStatics2(
        &known_waveforms2_);
    return true;
  }

  HRESULT hr = base::win::GetActivationFactory<
      haptics::IInputHapticsManagerStatics,
      RuntimeClass_Windows_Devices_Haptics_InputHapticsManager>(
      &input_haptics_statics_);
  if (FAILED(hr) || !input_haptics_statics_) {
    input_haptics_statics_.Reset();
    return false;
  }

  // Known-waveform statics; absence is non-fatal (PlayHaptics falls back to a
  // device-supported waveform). See haptics_waveforms_statics3_win.h.
  base::win::GetActivationFactory<
      haptics::IKnownSimpleHapticsControllerWaveformsStatics2,
      RuntimeClass_Windows_Devices_Haptics_KnownSimpleHapticsControllerWaveforms>(
      &known_waveforms2_);
  return true;
}

void HapticsManagerImplWin::PrimeHapticsController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!EnsureStatics()) {
    return;
  }

  ComPtr<haptics::IInputHapticsManager> manager;
  if (FAILED(input_haptics_statics_->GetForCurrentThread(&manager)) ||
      !manager) {
    return;
  }

  // The discarded lookup makes the platform establish and cache the current
  // haptics controller for this thread, so the first PlayHaptics sees a warm
  // controller instead of a cold, null one.
  ComPtr<haptics::ISimpleHapticsController> controller;
  manager->get_CurrentHapticsController(&controller);
}

std::optional<uint16_t> HapticsManagerImplWin::WaveformForEffect(
    blink::mojom::HapticEffect effect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<uint16_t>& cached =
      waveform_cache_[static_cast<size_t>(effect)];
  if (!cached.has_value()) {
    cached = ComputeWaveformForEffect(effect);
  }
  return cached;
}

std::optional<uint16_t> HapticsManagerImplWin::ComputeWaveformForEffect(
    blink::mojom::HapticEffect effect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The four semantic effects map to these Windows waveforms:
  //   hint -> Hover (Statics2), edge -> Collide, tick -> Step, align -> Align
  //   (Collide/Step/Align on Statics3).
  UINT16 value = 0;
  HRESULT hr = E_FAIL;

  if (effect == blink::mojom::HapticEffect::kHint) {
    if (!known_waveforms2_) {
      return std::nullopt;
    }
    hr = known_waveforms2_->get_Hover(&value);
    return SUCCEEDED(hr) ? std::optional<uint16_t>(value) : std::nullopt;
  }

  // Collide/Step/Align live on Statics3; QI it from the same factory (24H2+).
  if (!known_waveforms2_) {
    return std::nullopt;
  }
  ComPtr<IKnownSimpleHapticsControllerWaveformsStatics3> waveforms3;
  if (FAILED(known_waveforms2_.As(&waveforms3)) || !waveforms3) {
    return std::nullopt;
  }
  switch (effect) {
    case blink::mojom::HapticEffect::kEdge:
      hr = waveforms3->get_Collide(&value);
      break;
    case blink::mojom::HapticEffect::kTick:
      hr = waveforms3->get_Step(&value);
      break;
    case blink::mojom::HapticEffect::kAlign:
      hr = waveforms3->get_Align(&value);
      break;
    case blink::mojom::HapticEffect::kHint:
      NOTREACHED();
  }
  if (FAILED(hr)) {
    return std::nullopt;
  }
  return value;
}

std::optional<uint16_t> HapticsManagerImplWin::DefaultWaveformForDevice(
    haptics::HapticDeviceType device_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!known_waveforms2_) {
    return std::nullopt;
  }

  UINT16 value = 0;
  if (device_type == haptics::HapticDeviceType_None) {
    return std::nullopt;
  }
  if (device_type == haptics::HapticDeviceType_Pen) {
    ComPtr<haptics::IKnownSimpleHapticsControllerWaveformsStatics> base_statics;
    if (FAILED(known_waveforms2_.As(&base_statics)) || !base_statics) {
      return std::nullopt;
    }
    return SUCCEEDED(base_statics->get_Click(&value))
               ? std::optional<uint16_t>(value)
               : std::nullopt;
  }

  // Mouse, Touchpad, and Generic default to Hover.
  return SUCCEEDED(known_waveforms2_->get_Hover(&value))
             ? std::optional<uint16_t>(value)
             : std::nullopt;
}

}  // namespace content
