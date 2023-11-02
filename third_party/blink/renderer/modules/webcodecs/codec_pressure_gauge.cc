// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_gauge.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#if !BUILDFLAG(IS_WIN)
#define USE_SHARED_INSTANCE
#endif

// These numbers were picked as roughly 1/4th of the empirical lower limit at
// which we start getting errors when allocating new codecs. Some platforms have
// a decoder limit and an encoder limit, whereas others have a common shared
// limit. These estimates are conservative, but they take into account the fact
// that the true limits are OS-wide, while these thresholds are per-process. It
// also takes into account that we never actually gate codec creation, and we
// only vary the eagerness with which we will try to reclaim codecs instead.
#if BUILDFLAG(IS_WIN)
constexpr int kDecoderPressureThreshold = 6;
constexpr int kEncoderPressureThreshold = 0;
#elif BUILDFLAG(IS_CHROMEOS)
constexpr int kSharedPressureThreshold = 3;
#elif BUILDFLAG(IS_MAC)
constexpr int kSharedPressureThreshold = 24;
#elif BUILDFLAG(IS_ANDROID)
constexpr int kSharedPressureThreshold = 4;
#else
// By default (e.g. for Linux, Fuschia, Chromecast...), any codec with pressure
// should be reclaimable, regardless of global presure.
constexpr int kSharedPressureThreshold = 0;
#endif

namespace blink {

CodecPressureGauge::CodecPressureGauge(size_t pressure_threshold)
    : pressure_threshold_(pressure_threshold) {}

// static
CodecPressureGauge& CodecPressureGauge::GetInstance(
    ReclaimableCodec::CodecType type) {
#if defined(USE_SHARED_INSTANCE)
  return SharedInstance();
#else
  switch (type) {
    case ReclaimableCodec::CodecType::kDecoder:
      return DecoderInstance();
    case ReclaimableCodec::CodecType::kEncoder:
      return EncoderInstance();
  }
#endif
}

#if defined(USE_SHARED_INSTANCE)
// static
CodecPressureGauge& CodecPressureGauge::SharedInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecPressureGauge, shared_instance,
                                  (kSharedPressureThreshold));

  return shared_instance;
}
#else
// static
CodecPressureGauge& CodecPressureGauge::DecoderInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecPressureGauge, decoder_instance,
                                  (kDecoderPressureThreshold));

  return decoder_instance;
}

// static
CodecPressureGauge& CodecPressureGauge::EncoderInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecPressureGauge, encoder_instance,
                                  (kEncoderPressureThreshold));

  return encoder_instance;
}
#endif

#undef USE_SHARED_INSTANCE

void CodecPressureGauge::Increment() {
  base::AutoLock locker(lock_);
  DCHECK(pressure_callbacks_.size());

  ++global_pressure_;

  CheckForThresholdChanges_Locked();
}

void CodecPressureGauge::Decrement() {
  base::AutoLock locker(lock_);
  DCHECK(pressure_callbacks_.size());

  DCHECK(global_pressure_);
  --global_pressure_;

  CheckForThresholdChanges_Locked();
}

std::pair<CodecPressureGauge::PressureCallbackId, bool>
CodecPressureGauge::RegisterPressureCallback(
    PressureThresholdChangedCallback pressure_callback) {
  base::AutoLock locker(lock_);
  PressureCallbackId id = next_pressure_callback_id_++;

  auto result = pressure_callbacks_.insert(id, std::move(pressure_callback));
  DCHECK(result.is_new_entry);

  return std::make_pair(id, global_pressure_exceeded_);
}

void CodecPressureGauge::UnregisterPressureCallback(
    PressureCallbackId callback_id,
    size_t pressure_released) {
  base::AutoLock locker(lock_);

  DCHECK(pressure_callbacks_.Contains(callback_id));
  pressure_callbacks_.erase(callback_id);

  DCHECK_GE(global_pressure_, pressure_released);
  global_pressure_ -= pressure_released;

  // Make sure we still have callbacks left if we have leftover pressure.
  DCHECK(!global_pressure_ || pressure_callbacks_.size());

  CheckForThresholdChanges_Locked();
}

void CodecPressureGauge::CheckForThresholdChanges_Locked() {
  lock_.AssertAcquired();

  bool pressure_exceeded = global_pressure_ > pressure_threshold_;

  if (pressure_exceeded == global_pressure_exceeded_)
    return;

  global_pressure_exceeded_ = pressure_exceeded;

  // Notify all callbacks of pressure threshold changes.
  // Note: we normally should make a copy of |pressure_callbacks_| and release
  // |lock_|, to avoid deadlocking on reentrant calls. However, the only
  // callbacks registered are from CodecPressureManagers, which do not
  // reentranly call into this class.
  for (auto& callback : pressure_callbacks_.Values())
    callback.Run(global_pressure_exceeded_);
}

}  // namespace blink
