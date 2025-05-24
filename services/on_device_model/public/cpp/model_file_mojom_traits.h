// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_FILE_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_FILE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
    UnionTraits<on_device_model::mojom::ModelFileDataView,
                on_device_model::ModelFile> {
 public:
  static base::File file(on_device_model::ModelFile& file);
  static base::FilePath path(on_device_model::ModelFile& file);

  static bool Read(on_device_model::mojom::ModelFileDataView data,
                   on_device_model::ModelFile* weights);

  static on_device_model::mojom::ModelFileDataView::Tag GetTag(
      const on_device_model::ModelFile& weights);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_FILE_MOJOM_TRAITS_H_
