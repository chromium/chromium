// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace on_device_model {

// A bundle of file paths to use for execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP) ModelAssetPaths {
  ModelAssetPaths();
  ModelAssetPaths(const ModelAssetPaths&);
  ~ModelAssetPaths();

  base::FilePath weights;
  base::FilePath sp_model;
};

// A bundle of opened file assets comprising model description to use for
// execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP) ModelAssets {
  ModelAssets();
  ModelAssets(ModelAssets&&);
  ModelAssets& operator=(ModelAssets&&);
  ~ModelAssets();

  base::File weights;
  base::FilePath weights_path;
  base::FilePath sp_model_path;
};

// Helper to open files for ModelAssets given their containing paths.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP)
ModelAssets LoadModelAssets(const ModelAssetPaths& paths);

// A bundle of file paths to use for loading an adaptation.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP) AdaptationAssetPaths {
  AdaptationAssetPaths();
  AdaptationAssetPaths(const AdaptationAssetPaths&);
  ~AdaptationAssetPaths();

  base::FilePath weights;
};

// A bundle of opened file assets comprising an adaptation description to use
// for execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP) AdaptationAssets {
  AdaptationAssets();
  AdaptationAssets(AdaptationAssets&&);
  AdaptationAssets& operator=(AdaptationAssets&&);
  ~AdaptationAssets();

  base::File weights;
  base::FilePath weights_path;
};

// Helper to open files for AdaptationAssets given their containing paths.
COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP)
AdaptationAssets LoadAdaptationAssets(const AdaptationAssetPaths& paths);

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
