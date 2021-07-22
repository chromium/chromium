// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_ERROR_OR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_ERROR_OR_H_

#include <utility>

#include "base/files/file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

namespace blink {

// TODO(crbug.com/1228745): This is a copy of base::FileErrorOr. Delete this
// class once FileErrorOr is moved to //base.

// Helper for methods which perform file system operations and which may fail.
// Objects of this type can take on EITHER a base::File::Error value OR a result
// value of arbitrary type.
template <typename ValueType>
class FileErrorOr {
 public:
  explicit FileErrorOr() = default;
  FileErrorOr(base::File::Error error) : error_(error) {}
  FileErrorOr(ValueType&& value)
      : maybe_value_(absl::in_place, std::move(value)) {}
  FileErrorOr(const FileErrorOr&) = default;
  FileErrorOr(FileErrorOr&&) = default;
  FileErrorOr& operator=(const FileErrorOr&) = default;
  FileErrorOr& operator=(FileErrorOr&&) = default;
  ~FileErrorOr() = default;

  bool is_error() const { return !maybe_value_.has_value(); }
  base::File::Error error() const { return error_; }

  ValueType& value() { return maybe_value_.value(); }
  const ValueType& value() const { return maybe_value_.value(); }

  ValueType* operator->() { return &maybe_value_.value(); }
  const ValueType* operator->() const { return &maybe_value_.value(); }

 private:
  base::File::Error error_ = base::File::FILE_ERROR_FAILED;
  absl::optional<ValueType> maybe_value_;
};

}  // namespace blink

namespace WTF {
// TODO: When the {FileErrorOr<>} class gets moved to //base, then this class
// should be move into the "main" cross thread copier header file.
template <>
struct CrossThreadCopier<blink::FileErrorOr<int64_t>>
    : public WTF::CrossThreadCopierPassThrough<blink::FileErrorOr<int64_t>> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_ERROR_OR_H_
