// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_command_serializers.h"

#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)

namespace blink {

DawnNoopCommandSerializer::DawnNoopCommandSerializer() = default;
DawnNoopCommandSerializer::~DawnNoopCommandSerializer() = default;

size_t DawnNoopCommandSerializer::GetMaximumAllocationSize() const {
  return sizeof(buf_);
}

std::optional<std::span<volatile std::byte>>
DawnNoopCommandSerializer::GetCommandSpace(size_t size) {
  if (size > sizeof(buf_)) {
    return std::nullopt;
  }
  return buf_;
}

bool DawnNoopCommandSerializer::Flush() {
  return true;
}

DawnTestingCommandSerializer::DawnTestingCommandSerializer() = default;
DawnTestingCommandSerializer::~DawnTestingCommandSerializer() = default;

size_t DawnTestingCommandSerializer::GetMaximumAllocationSize() const {
  return (buf_.size() * sizeof(decltype(buf_)::value_type));
}

void DawnTestingCommandSerializer::SetHandler(
    dawn::wire::CommandHandler* handler) {
  handler_ = handler;
}

std::optional<std::span<volatile std::byte>>
DawnTestingCommandSerializer::GetCommandSpace(size_t size) {
  if (size > backing_buf_.size()) {
    return std::nullopt;
  }
  if (size > buf_.size()) {
    if (!Flush()) {
      return std::nullopt;
    }
  }
  return buf_.take_first(size);
}

bool DawnTestingCommandSerializer::Flush() {
  if (!handler_) {
    return false;
  }
  bool success = handler_->HandleCommands(
      base::span(backing_buf_).first(backing_buf_.size() - buf_.size()));
  buf_ = backing_buf_;
  return success;
}

}  // namespace blink

#endif  // BUILDFLAG(USE_DAWN)
