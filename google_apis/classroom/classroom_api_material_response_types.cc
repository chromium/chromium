// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_material_response_types.h"

#include <string>
#include <string_view>

#include "base/values.h"

namespace google_apis::classroom {
namespace {

// JSON keys related to materials.
constexpr char kApiResponseItemTitleKey[] = "title";
constexpr char kApiResponseMaterialDriveKey[] = "driveFile";
constexpr char kApiResponseMaterialYoutubeVideoKey[] = "youtubeVideo";
constexpr char kApiResponseMaterialLinkKey[] = "link";
constexpr char kApiResponseMaterialFormKey[] = "form";

}  // namespace

Material::Material() = default;
Material::~Material() = default;

bool Material::ConvertMaterial(const base::Value* input, Material* output) {
  const base::Value::Dict* dict = input->GetIfDict();
  if (!dict) {
    return false;
  }

  const auto* const sharedDriveFile =
      dict->FindDict(kApiResponseMaterialDriveKey);
  const auto* const youtubeVideo =
      dict->FindDict(kApiResponseMaterialYoutubeVideoKey);
  const auto* const link = dict->FindDict(kApiResponseMaterialLinkKey);
  const auto* const form = dict->FindDict(kApiResponseMaterialFormKey);

  const base::Value::Dict* content_dict = nullptr;
  Material::Type content_type = Material::Type::kUnknown;

  if (sharedDriveFile) {
    content_dict = sharedDriveFile->FindDict(kApiResponseMaterialDriveKey);
    if (!content_dict) {
      // Shared drive file should contain a drive file.
      return false;
    }
    content_type = Material::Type::kSharedDriveFile;
  } else if (youtubeVideo) {
    content_dict = youtubeVideo;
    content_type = Material::Type::kYoutubeVideo;
  } else if (link) {
    content_dict = link;
    content_type = Material::Type::kLink;
  } else if (form) {
    content_dict = form;
    content_type = Material::Type::kForm;
  }

  if (!content_dict) {
    output->type_ = Material::Type::kUnknown;
    return true;
  }

  const std::string* title = content_dict->FindString(kApiResponseItemTitleKey);
  if (!title) {
    // Title is a required field for all material types.
    return false;
  }

  output->title_ = *title;
  output->type_ = content_type;
  return true;
}

}  // namespace google_apis::classroom
