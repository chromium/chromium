// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/source_location.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace remoting {

struct SourceLocation::BackingStore {
  std::string function_name;
  std::string file_name;
};

SourceLocation::SourceLocation() = default;
SourceLocation::~SourceLocation() = default;

// The default move constructor is sufficient since the backing store remains
// unmodified except for the transfer of its ownership, so the string pointers
// will remain valid.
SourceLocation::SourceLocation(SourceLocation&&) = default;

SourceLocation::SourceLocation(const SourceLocation& other) {
  operator=(other);
}

SourceLocation::SourceLocation(const base::Location& location) {
  if (location.has_source_info()) {
    function_name_ = location.function_name();
    file_name_ = location.file_name();
    line_number_ = location.line_number();
    DCHECK(function_name_);
    DCHECK(file_name_);
    DCHECK_GT(line_number_, -1);
  } else if (location.program_counter() != nullptr) {
    // We don't log a warning if the location is uninitialized.
    LOG(WARNING) << "Location " << location.ToString()
                 << " does not have source info.";
  }
}

SourceLocation& SourceLocation::operator=(SourceLocation&&) = default;

SourceLocation& SourceLocation::operator=(const SourceLocation& other) {
  if (other.backing_store_) {
    backing_store_ = std::make_unique<BackingStore>(*other.backing_store_);
    function_name_ = backing_store_->function_name.data();
    file_name_ = backing_store_->file_name.data();
  } else {
    function_name_ = other.function_name_;
    file_name_ = other.file_name_;
  }
  line_number_ = other.line_number();
  return *this;
}

bool SourceLocation::operator==(const SourceLocation& other) const {
  if (is_null() != other.is_null()) {
    return false;
  }
  if (is_null() && other.is_null()) {
    return true;
  }
  return std::string_view(function_name_) == other.function_name_ &&
         std::string_view(file_name_) == other.file_name_ &&
         line_number_ == other.line_number_;
}

std::string SourceLocation::ToString() const {
  if (is_null()) {
    return "<null source info>";
  }
  return std::string(function_name()) + "@" + file_name() + ":" +
         base::NumberToString(line_number());
}

bool SourceLocation::HasBackingStoreForTesting() const {
  return backing_store_ != nullptr;
}

// static
SourceLocation SourceLocation::CreateWithBackingStoreForTesting(
    std::optional<std::string_view> function_name,
    std::optional<std::string_view> file_name,
    int line_number) {
  SourceLocation location;
  location.InitializeWithBackingStore(function_name, file_name, line_number);
  return location;
}

void SourceLocation::InitializeWithBackingStore(
    std::optional<std::string_view> function_name,
    std::optional<std::string_view> file_name,
    int line_number) {
  if (!function_name.has_value() || !file_name.has_value()) {
    return;
  }
  DCHECK_GT(line_number, -1);
  backing_store_ = std::make_unique<BackingStore>();
  backing_store_->function_name = *function_name;
  backing_store_->file_name = *file_name;
  function_name_ = backing_store_->function_name.data();
  file_name_ = backing_store_->file_name.data();
  line_number_ = line_number;
}

}  // namespace remoting
