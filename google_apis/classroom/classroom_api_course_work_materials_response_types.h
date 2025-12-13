// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_MATERIALS_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_MATERIALS_RESPONSE_TYPES_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "google_apis/classroom/classroom_api_material_response_types.h"
#include "url/gurl.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::classroom {

// Represents a single course work material item.
// https://developers.google.com/classroom/reference/rest/v1/courses.courseWorkMaterials
class CourseWorkMaterialItem {
 public:
  enum class State {
    kPublished,
    kOther,
  };

  CourseWorkMaterialItem();
  CourseWorkMaterialItem(const CourseWorkMaterialItem&) = delete;
  CourseWorkMaterialItem& operator=(const CourseWorkMaterialItem&) = delete;
  ~CourseWorkMaterialItem();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CourseWorkMaterialItem>* converter);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  State state() const { return state_; }
  const GURL& alternate_link() const { return alternate_link_; }
  const base::Time& creation_time() const { return creation_time_; }
  const base::Time& last_update() const { return last_update_; }
  const std::vector<std::unique_ptr<Material>>& materials() const {
    return materials_;
  }

 private:
  // Classroom-assigned identifier of this course work material.
  std::string id_;

  // Title of this course work material.
  std::string title_;

  // Status of this course work material.
  State state_ = State::kOther;

  // Absolute link to this course work material in the Classroom web UI.
  GURL alternate_link_;

  // The timestamp when this course work material was created.
  base::Time creation_time_;

  // The timestamp of the last course work material update.
  base::Time last_update_;

  // The materials associated with this course work material.
  std::vector<std::unique_ptr<Material>> materials_;
};

// Container for multiple `CourseWorkMaterialItem`s.
class CourseWorkMaterial {
 public:
  CourseWorkMaterial();
  CourseWorkMaterial(const CourseWorkMaterial&) = delete;
  CourseWorkMaterial& operator=(const CourseWorkMaterial&) = delete;
  ~CourseWorkMaterial();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CourseWorkMaterial>* converter);

  // Creates a `CourseWorkMaterial` from parsed JSON.
  static std::unique_ptr<CourseWorkMaterial> CreateFrom(
      const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<CourseWorkMaterialItem>>& items() const {
    return items_;
  }

 private:
  std::vector<std::unique_ptr<CourseWorkMaterialItem>> items_;

  // Token that can be used to request the next page of this result.
  std::string next_page_token_;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_MATERIALS_RESPONSE_TYPES_H_
