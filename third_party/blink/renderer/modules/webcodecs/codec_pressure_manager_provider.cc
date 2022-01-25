// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"

namespace blink {

// static
const char CodecPressureManagerProvider::kSupplementName[] =
    "CodecPressureManagerProvider";

// static
CodecPressureManagerProvider& CodecPressureManagerProvider::From(
    ExecutionContext& context) {
  CHECK(!context.IsContextDestroyed());
  CodecPressureManagerProvider* supplement =
      Supplement<ExecutionContext>::From<CodecPressureManagerProvider>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CodecPressureManagerProvider>(context);
    ProvideTo(context, supplement);
  }
  return *supplement;
}

CodecPressureManagerProvider::CodecPressureManagerProvider(
    ExecutionContext& context)
    : Supplement(context) {}

CodecPressureManager*
CodecPressureManagerProvider::GetDecoderPressureManager() {
  if (!decoder_pressure_manager_)
    decoder_pressure_manager_ = MakeGarbageCollected<CodecPressureManager>();

  return decoder_pressure_manager_;
}

CodecPressureManager*
CodecPressureManagerProvider::GetEncoderPressureManager() {
  if (!encoder_pressure_manager_)
    encoder_pressure_manager_ = MakeGarbageCollected<CodecPressureManager>();

  return encoder_pressure_manager_;
}

void CodecPressureManagerProvider::Trace(Visitor* visitor) const {
  visitor->Trace(decoder_pressure_manager_);
  visitor->Trace(encoder_pressure_manager_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
