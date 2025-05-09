// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_file_mojom_traits.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"

namespace mojo {

// static
base::File UnionTraits<
    on_device_model::mojom::ModelFileDataView,
    on_device_model::ModelFile>::file(on_device_model::ModelFile& file) {
  return std::get<base::File>(std::move(file.file_));
}

// static
base::FilePath UnionTraits<
    on_device_model::mojom::ModelFileDataView,
    on_device_model::ModelFile>::path(on_device_model::ModelFile& file) {
  return std::get<base::FilePath>(std::move(file.file_));
}

// static
bool UnionTraits<on_device_model::mojom::ModelFileDataView,
                 on_device_model::ModelFile>::
    Read(on_device_model::mojom::ModelFileDataView data,
         on_device_model::ModelFile* out) {
  switch (data.tag()) {
    case on_device_model::mojom::ModelFileDataView::Tag::kFile: {
      base::File file;
      if (!data.ReadFile(&file)) {
        return false;
      }
      *out = on_device_model::ModelFile(std::move(file));
      return true;
    }
    case on_device_model::mojom::ModelFileDataView::Tag::kPath: {
      // base::FilePath doesn't have nullable StructTraits, so we need to use
      // optional.
      std::optional<base::FilePath> path;
      if (!data.ReadPath(&path)) {
        return false;
      }
      *out = on_device_model::ModelFile(
          std::move(path).value_or(base::FilePath()));
      return true;
    }
    default:
      return false;
  }
}

// static
on_device_model::mojom::ModelFileDataView::Tag UnionTraits<
    on_device_model::mojom::ModelFileDataView,
    on_device_model::ModelFile>::GetTag(const on_device_model::ModelFile&
                                            file) {
  return std::visit(
      base::Overloaded{
          [](const base::File& file) {
            return on_device_model::mojom::ModelFileDataView::Tag::kFile;
          },
          [](const base::FilePath& path) {
            return on_device_model::mojom::ModelFileDataView::Tag::kPath;
          }},
      file.file_);
}

}  // namespace mojo
