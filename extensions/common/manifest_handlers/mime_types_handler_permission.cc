// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler_permission.h"

#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/stl_util.h"
#include "base/types/optional_util.h"
#include "extensions/common/manifest_constants.h"
#include "tools/json_schema_compiler/util.h"

namespace extensions {

namespace {

// MIME types extensions are currently allowed to handle, each bound to the
// permission that carries its install warning. Adding a type here requires
// adding its permission and warning string.
constexpr auto kSupportedMimeTypes =
    base::MakeFixedFlatMap<std::string_view, mojom::APIPermissionID>({
        {"application/pdf", mojom::APIPermissionID::kMimeTypesHandlerPdf},
    });

const MimeTypesHandlerPermission::MimeTypeSet& MimeTypesOf(
    const ManifestPermission* permission) {
  return static_cast<const MimeTypesHandlerPermission*>(permission)
      ->mime_types();
}

}  // namespace

// static
std::optional<mojom::APIPermissionID>
MimeTypesHandlerPermission::PermissionIDForMimeType(
    const std::string_view& mime_type) {
  return base::OptionalFromPtr(
      base::FindOrNull(kSupportedMimeTypes, mime_type));
}

MimeTypesHandlerPermission::MimeTypesHandlerPermission() = default;

MimeTypesHandlerPermission::MimeTypesHandlerPermission(MimeTypeSet mime_types)
    : mime_types_(std::move(mime_types)) {}

MimeTypesHandlerPermission::~MimeTypesHandlerPermission() = default;

std::string MimeTypesHandlerPermission::name() const {
  return manifest_keys::kMimeTypesHandler;
}

std::string MimeTypesHandlerPermission::id() const {
  return name();
}

PermissionIDSet MimeTypesHandlerPermission::GetPermissions() const {
  PermissionIDSet ids;
  for (const std::string& mime_type : mime_types_) {
    std::optional<mojom::APIPermissionID> id =
        PermissionIDForMimeType(mime_type);
    if (id.has_value()) {
      ids.insert(*id);
    }
  }
  return ids;
}

bool MimeTypesHandlerPermission::FromValue(const base::Value* value) {
  const base::ListValue* list = value ? value->GetIfList() : nullptr;
  std::vector<std::string> mime_types;
  if (!list ||
      !json_schema_compiler::util::PopulateArrayFromList(*list, mime_types)) {
    return false;
  }
  mime_types_ = MimeTypeSet(std::move(mime_types));
  return true;
}

std::unique_ptr<base::Value> MimeTypesHandlerPermission::ToValue() const {
  return std::make_unique<base::Value>(base::ToValueList(mime_types_));
}

std::unique_ptr<ManifestPermission> MimeTypesHandlerPermission::Diff(
    const ManifestPermission* rhs) const {
  return std::make_unique<MimeTypesHandlerPermission>(
      base::STLSetDifference<MimeTypeSet>(mime_types_, MimeTypesOf(rhs)));
}

std::unique_ptr<ManifestPermission> MimeTypesHandlerPermission::Union(
    const ManifestPermission* rhs) const {
  return std::make_unique<MimeTypesHandlerPermission>(
      base::STLSetUnion<MimeTypeSet>(mime_types_, MimeTypesOf(rhs)));
}

std::unique_ptr<ManifestPermission> MimeTypesHandlerPermission::Intersect(
    const ManifestPermission* rhs) const {
  return std::make_unique<MimeTypesHandlerPermission>(
      base::STLSetIntersection<MimeTypeSet>(mime_types_, MimeTypesOf(rhs)));
}

bool MimeTypesHandlerPermission::RequiresManagementUIWarning() const {
  return !mime_types_.empty();
}

}  // namespace extensions
