// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_FAKE_ML_MODEL_HANDLES_H_
#define SERVICES_AUDIO_TEST_FAKE_ML_MODEL_HANDLES_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"

namespace audio {

// Base class for fake ML model handles supporting optional on-destroy
// notification.
class FakeMlModelHandleBase : public media::MlModelHandle {
 public:
  FakeMlModelHandleBase(const FakeMlModelHandleBase&) = delete;
  FakeMlModelHandleBase& operator=(const FakeMlModelHandleBase&) = delete;

 protected:
  explicit FakeMlModelHandleBase(
      base::OnceClosure on_destroy = base::NullCallback());
  ~FakeMlModelHandleBase() override;

 private:
  base::OnceClosure on_destroy_;
  scoped_refptr<base::SequencedTaskRunner> reply_runner_;
};

// A fake ML model handle that loads a valid test model file.
class FakeMlModelHandle : public FakeMlModelHandleBase {
 public:
  explicit FakeMlModelHandle(
      base::OnceClosure on_destroy = base::NullCallback());

  FakeMlModelHandle(const FakeMlModelHandle&) = delete;
  FakeMlModelHandle& operator=(const FakeMlModelHandle&) = delete;

  const tflite::FlatBufferModel& Get() override;

 private:
  ~FakeMlModelHandle() override;

  std::unique_ptr<tflite::FlatBufferModel> model_;
};

// A fake ML model handle that constructs an invalid FlatBuffer model in-memory
// that passes FlatBuffer verification but fails interpreter creation.
class FakeInvalidMlModelHandle : public FakeMlModelHandleBase {
 public:
  explicit FakeInvalidMlModelHandle(
      base::OnceClosure on_destroy = base::NullCallback());

  FakeInvalidMlModelHandle(const FakeInvalidMlModelHandle&) = delete;
  FakeInvalidMlModelHandle& operator=(const FakeInvalidMlModelHandle&) = delete;

  const tflite::FlatBufferModel& Get() override;

 private:
  ~FakeInvalidMlModelHandle() override;

  media::FakeModel fake_model_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_FAKE_ML_MODEL_HANDLES_H_
