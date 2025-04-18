// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
#define SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

class COMPONENT_EXPORT(WEBNN_SERVICE) PlatformFunctions {
 public:
  PlatformFunctions(const PlatformFunctions&) = delete;
  PlatformFunctions& operator=(const PlatformFunctions&) = delete;

  static PlatformFunctions* GetInstance();

  const OrtApi* ort_api() const { return ort_api_.get(); }
  const OrtModelEditorApi* ort_model_editor_api() const {
    return ort_model_editor_api_.get();
  }

 private:
  friend class base::NoDestructor<PlatformFunctions>;

  PlatformFunctions();
  ~PlatformFunctions();

  bool AllFunctionsLoaded();

  base::ScopedNativeLibrary ort_library_;
  raw_ptr<const OrtApi> ort_api_ = nullptr;
  raw_ptr<const OrtModelEditorApi> ort_model_editor_api_ = nullptr;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
