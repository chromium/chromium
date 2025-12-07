// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"

namespace blink {

// static
CodecPressureManagerProvider& CodecPressureManagerProvider::From(
    ExecutionContext& context) {
  CHECK(!context.IsContextDestroyed());
  CodecPressureManagerProvider* supplement =
      context.GetCodecPressureManagerProvider();
  if (!supplement) {
    supplement = MakeGarbageCollected<CodecPressureManagerProvider>(context);
    context.SetCodecPressureManagerProvider(supplement);
  }
  return *supplement;
}

CodecPressureManagerProvider::CodecPressureManagerProvider(
    ExecutionContext& context)
    : execution_context_(context) {}

CodecPressureManager*
CodecPressureManagerProvider::GetDecoderPressureManager() {
  if (!decoder_pressure_manager_) {
    decoder_pressure_manager_ = MakeGarbageCollected<CodecPressureManager>(
        ReclaimableCodec::CodecType::kDecoder, GetTaskRunner());
  }

  return decoder_pressure_manager_.Get();
}

CodecPressureManager*
CodecPressureManagerProvider::GetEncoderPressureManager() {
  if (!encoder_pressure_manager_) {
    encoder_pressure_manager_ = MakeGarbageCollected<CodecPressureManager>(
        ReclaimableCodec::CodecType::kEncoder, GetTaskRunner());
  }

  return encoder_pressure_manager_.Get();
}

scoped_refptr<base::SequencedTaskRunner>
CodecPressureManagerProvider::GetTaskRunner() {
  ExecutionContext* context = execution_context_;

  DCHECK(context && !context->IsContextDestroyed());

  return context->GetTaskRunner(TaskType::kInternalMediaRealTime);
}

void CodecPressureManagerProvider::Trace(Visitor* visitor) const {
  visitor->Trace(decoder_pressure_manager_);
  visitor->Trace(encoder_pressure_manager_);
  visitor->Trace(execution_context_);
}

}  // namespace blink
