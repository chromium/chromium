// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"

// Pretend we're always under global pressure.
// TODO(crbug.com/1277618): Use per-platform limits.
constexpr int kPressureThreshold = 0;

namespace blink {

CodecPressureManager::CodecPressureManager()
    : pressure_threshold_(kPressureThreshold) {}

void CodecPressureManager::AddCodec(ReclaimableCodec* codec) {
  DCHECK(codec->is_applying_codec_pressure());

  DCHECK(!codecs_with_pressure_.Contains(codec));
  codecs_with_pressure_.insert(codec);

  MaybeSetGlobalPressureFlags(codec);
}

void CodecPressureManager::RemoveCodec(ReclaimableCodec* codec) {
  DCHECK(codec->is_applying_codec_pressure());

  DCHECK(codecs_with_pressure_.Contains(codec));
  codecs_with_pressure_.erase(codec);

  // |codec| is responsible for clearing its own global pressure exceeded flag.

  MaybeClearGlobalPressureFlags();
}

void CodecPressureManager::OnCodecDisposed(ReclaimableCodec* codec) {
  DCHECK(codec->is_applying_codec_pressure());

  // The GC should have removed |codec| from |codecs_with_pressure_|.
  DCHECK(!codecs_with_pressure_.Contains(codec));

  MaybeClearGlobalPressureFlags();
}

void CodecPressureManager::MaybeSetGlobalPressureFlags(
    ReclaimableCodec* new_codec) {
  DCHECK(codecs_with_pressure_.Contains(new_codec));
  if (codecs_with_pressure_.size() <= pressure_threshold_) {
    // Not enough pressure.
    DCHECK(!global_pressure_exceeded_);
    return;
  }

  if (global_pressure_exceeded_) {
    // Codecs in |codecs_with_pressure_| were set, but not |new_codec|.
    new_codec->SetGlobalPressureExceededFlag(true);
    return;
  }

  DCHECK_EQ(codecs_with_pressure_.size(), pressure_threshold_ + 1);
  for (auto codec : codecs_with_pressure_)
    codec->SetGlobalPressureExceededFlag(true);

  global_pressure_exceeded_ = true;
}

void CodecPressureManager::MaybeClearGlobalPressureFlags() {
  if (!global_pressure_exceeded_)
    return;

  // Still too much pressure.
  if (codecs_with_pressure_.size() > pressure_threshold_)
    return;

  for (auto codec : codecs_with_pressure_)
    codec->SetGlobalPressureExceededFlag(false);

  global_pressure_exceeded_ = false;
}

void CodecPressureManager::Trace(Visitor* visitor) const {
  visitor->Trace(codecs_with_pressure_);
}

}  // namespace blink
