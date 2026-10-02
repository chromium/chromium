// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/fake_ml_model_handles.h"

#include <utility>

#include "base/check.h"

namespace audio {

FakeMlModelHandleBase::FakeMlModelHandleBase(base::OnceClosure on_destroy)
    : on_destroy_(std::move(on_destroy)),
      reply_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

FakeMlModelHandleBase::~FakeMlModelHandleBase() {
  if (on_destroy_) {
    reply_runner_->PostTask(FROM_HERE, std::move(on_destroy_));
  }
}

FakeMlModelHandle::FakeMlModelHandle(base::OnceClosure on_destroy)
    : FakeMlModelHandleBase(std::move(on_destroy)),
      model_(media::LoadVoiceIsolationTestModel()) {
  CHECK(model_);
}

FakeMlModelHandle::~FakeMlModelHandle() = default;

const tflite::FlatBufferModel& FakeMlModelHandle::Get() {
  return *model_;
}

FakeInvalidMlModelHandle::FakeInvalidMlModelHandle(base::OnceClosure on_destroy)
    : FakeMlModelHandleBase(std::move(on_destroy)),
      fake_model_(media::BuildBogusModel()) {
  CHECK(fake_model_.model);
}

FakeInvalidMlModelHandle::~FakeInvalidMlModelHandle() = default;

const tflite::FlatBufferModel& FakeInvalidMlModelHandle::Get() {
  return *fake_model_.model;
}

}  // namespace audio
