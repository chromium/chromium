// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace on_device_model {

// A bundle of opened file assets comprising model description to use for
// execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ModelAssets {
  ModelAssets() = default;
  ModelAssets(ModelAssets&&) = default;
  ModelAssets& operator=(ModelAssets&&) = default;
  ~ModelAssets() = default;

  base::File sp_model;
  base::File model;
  base::File weights;
};

// Helper to open files for ModelAssets given a base path.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
ModelAssets LoadModelAssets(const base::FilePath& model_path);

}  // namespace on_device_model

#endif  //  SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
