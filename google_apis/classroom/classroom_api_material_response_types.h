// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_MATERIAL_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_MATERIAL_RESPONSE_TYPES_H_

#include <memory>
#include <string>

namespace base {
class Value;
}  // namespace base

namespace google_apis::classroom {

// Represents a material attached to a course work item. This is
// a shared type used by both CourseWork and CourseWorkMaterial.
// https://developers.google.com/classroom/reference/rest/v1/Material
class Material {
 public:
  // Material type.
  enum class Type {
    kSharedDriveFile,
    kYoutubeVideo,
    kLink,
    kForm,
    kUnknown,
  };

  Material();
  Material(const Material&) = delete;
  Material& operator=(const Material&) = delete;
  ~Material();

  static bool ConvertMaterial(const base::Value* input, Material* output);

  const std::string& title() const { return title_; }
  Type type() const { return type_; }

 private:
  friend class CourseWorkItem;
  friend class CourseWorkMaterialItem;

  std::string title_;
  Type type_ = Type::kUnknown;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_MATERIAL_RESPONSE_TYPES_H_
