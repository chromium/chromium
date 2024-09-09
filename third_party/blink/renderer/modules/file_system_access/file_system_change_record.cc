// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_change_record.h"

#include <optional>

#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"

namespace blink {

namespace {

constexpr V8FileSystemChangeType::Enum ToChangeTypeEnum(
    mojom::blink::FileSystemAccessChangeType::Tag tag) {
  // This assertion protects against the IDL enum changing without updating the
  // corresponding mojom interface, while the lack of a default case in the
  // switch statement below ensures the opposite.
  static_assert(
      V8FileSystemChangeType::kEnumSize == 6u,
      "the number of fields in the FileSystemAccessChangeType mojom union "
      "must match the number of fields in the FileSystemChangeType blink enum");

  switch (tag) {
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kAppeared:
      return V8FileSystemChangeType::Enum::kAppeared;
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kDisappeared:
      return V8FileSystemChangeType::Enum::kDisappeared;
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kErrored:
      return V8FileSystemChangeType::Enum::kErrored;
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kModified:
      return V8FileSystemChangeType::Enum::kModified;
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kMoved:
      return V8FileSystemChangeType::Enum::kMoved;
    case mojom::blink::FileSystemAccessChangeType::Data_::
        FileSystemAccessChangeType_Tag::kUnknown:
      return V8FileSystemChangeType::Enum::kUnknown;
  }
}

}  // namespace

FileSystemChangeRecord::FileSystemChangeRecord(
    FileSystemHandle* root,
    FileSystemHandle* changed_handle,
    const Vector<String>& relative_path,
    mojom::blink::FileSystemAccessChangeTypePtr type)
    : root_(root),
      changed_handle_(changed_handle),
      relative_path_components_(relative_path),
      type_(std::move(type)) {}

V8FileSystemChangeType FileSystemChangeRecord::type() const {
  return V8FileSystemChangeType(ToChangeTypeEnum(type_->which()));
}

std::optional<Vector<String>> FileSystemChangeRecord::relativePathMovedFrom()
    const {
  return type_->is_moved() ? type_->get_moved()->former_relative_path
                           : std::nullopt;
}

void FileSystemChangeRecord::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  visitor->Trace(changed_handle_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
