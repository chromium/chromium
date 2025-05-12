// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_

#include <optional>
#include <variant>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-data-view.h"

namespace on_device_model {

// A bundle of file paths to use for execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ModelAssetPaths {
  ModelAssetPaths();
  ModelAssetPaths(const ModelAssetPaths&);
  ~ModelAssetPaths();

  base::FilePath weights;
  base::FilePath sp_model;
  base::FilePath cache;
};

class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ModelFile {
 public:
  explicit ModelFile(base::File file);
  explicit ModelFile(base::FilePath path);
  explicit ModelFile(mojo::DefaultConstruct::Tag);
  ModelFile(const ModelFile&);
  ModelFile& operator=(const ModelFile&);
  ModelFile(ModelFile&&);
  ModelFile& operator=(ModelFile&&);
  ~ModelFile();

  base::File& file();
  const base::File& file() const;

  const base::FilePath& path() const;

  bool IsFile() const;

 private:
  friend struct mojo::UnionTraits<on_device_model::mojom::ModelFileDataView,
                                  on_device_model::ModelFile>;

  std::variant<base::File, base::FilePath> file_;
};

// A bundle of opened file assets comprising model description to use for
// execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ModelAssets {
  // Convenience methods which construct `weights` from the given arg.
  static ModelAssets FromFile(base::File file);
  static ModelAssets FromPath(base::FilePath path);

  explicit ModelAssets(ModelFile weights);

  explicit ModelAssets(mojo::DefaultConstruct::Tag);
  ModelAssets(const ModelAssets&);
  ModelAssets& operator=(const ModelAssets&);
  ModelAssets(ModelAssets&&);
  ModelAssets& operator=(ModelAssets&&);
  ~ModelAssets();

  ModelFile weights;
  base::FilePath sp_model_path;
  base::File cache;
};

// Helper to open files for ModelAssets given their containing paths.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
ModelAssets LoadModelAssets(const ModelAssetPaths& paths);

// A bundle of file paths to use for loading an adaptation.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) AdaptationAssetPaths {
  AdaptationAssetPaths();
  AdaptationAssetPaths(const AdaptationAssetPaths&);
  ~AdaptationAssetPaths();

  bool operator==(const AdaptationAssetPaths& other) const {
    return weights == other.weights;
  }

  base::FilePath weights;
};

// A bundle of opened file assets comprising an adaptation description to use
// for execution.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) AdaptationAssets {
  AdaptationAssets();
  AdaptationAssets(const AdaptationAssets&);
  AdaptationAssets& operator=(const AdaptationAssets&);
  AdaptationAssets(AdaptationAssets&&);
  AdaptationAssets& operator=(AdaptationAssets&&);
  ~AdaptationAssets();

  // TODO(crbug.com/401011041): Use a ModelFile to represent these members.
  base::File weights;
  base::FilePath weights_path;
};

// Helper to open files for AdaptationAssets given their containing paths.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
AdaptationAssets LoadAdaptationAssets(const AdaptationAssetPaths& paths);

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_H_
