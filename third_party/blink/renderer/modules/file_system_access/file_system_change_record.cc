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

// Returns the `changed_handle` passed in except for "disappeared", "errored",
// and "unknown" who do not have a changed handle.
FileSystemHandle* GetChangedHandleForType(FileSystemHandle* changed_handle,
                                          const V8FileSystemChangeType& type) {
  switch (type.AsEnum()) {
    case V8FileSystemChangeType::Enum::kAppeared:
    case V8FileSystemChangeType::Enum::kModified:
    case V8FileSystemChangeType::Enum::kMoved:
      return changed_handle;
    case V8FileSystemChangeType::Enum::kDisappeared:
    case V8FileSystemChangeType::Enum::kErrored:
    case V8FileSystemChangeType::Enum::kUnknown:
      return nullptr;
  }
}

}  // namespace

// static
FileSystemChangeRecord* FileSystemChangeRecord::Create(
    FileSystemHandle* root,
    FileSystemHandle* changed_handle,
    Vector<String> relative_path_components,
    V8FileSystemChangeType type,
    std::optional<Vector<String>> relative_path_moved_from) {
  return MakeGarbageCollected<FileSystemChangeRecord>(
      root, changed_handle, std::move(relative_path_components),
      std::move(type), std::move(relative_path_moved_from));
}

FileSystemChangeRecord::FileSystemChangeRecord(
    FileSystemHandle* root,
    FileSystemHandle* changed_handle,
    Vector<String> relative_path_components,
    V8FileSystemChangeType type,
    std::optional<Vector<String>> relative_path_moved_from)
    : type_(std::move(type)),
      root_(root),
      changed_handle_(GetChangedHandleForType(changed_handle, type_)),
      relative_path_components_(std::move(relative_path_components)),
      relative_path_moved_from_(std::move(relative_path_moved_from)) {}

FileSystemChangeRecord::FileSystemChangeRecord(
    FileSystemHandle* root,
    FileSystemHandle* changed_handle,
    const Vector<String>& relative_path_components,
    mojom::blink::FileSystemAccessChangeTypePtr mojo_type)
    : type_(V8FileSystemChangeType(ToChangeTypeEnum(mojo_type->which()))),
      root_(root),
      changed_handle_(GetChangedHandleForType(changed_handle, type_)),
      relative_path_components_(relative_path_components),
      relative_path_moved_from_(
          mojo_type->is_moved() ? mojo_type->get_moved()->former_relative_path
                                : std::nullopt) {}

void FileSystemChangeRecord::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  visitor->Trace(changed_handle_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
