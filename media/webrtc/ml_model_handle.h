// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_ML_MODEL_HANDLE_H_
#define MEDIA_WEBRTC_ML_MODEL_HANDLE_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

// third_party/flatbuffers/ used by third_party/tflite has 64-bit truncation
// issues on 32-bit platforms.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#pragma clang diagnostic pop

namespace media {

// Interface for a refcounted, read-only TF Lite model that can be accessed and
// destroyed on any thread.
class COMPONENT_EXPORT(MEDIA_WEBRTC) MlModelHandle
    : public base::RefCountedThreadSafe<MlModelHandle> {
 public:
  // Returns a reference to a TFLite model, valid for the lifetime of the
  // ModelHandle.
  virtual const tflite::FlatBufferModel& Get() = 0;

 protected:
  friend class base::RefCountedThreadSafe<MlModelHandle>;
  virtual ~MlModelHandle() = default;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_ML_MODEL_HANDLE_H_
