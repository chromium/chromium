// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/embedder_isolation_info.h"

#include <cinttypes>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace content {

// static
EmbedderIsolationInfo EmbedderIsolationInfo::CreateNone() {
  return EmbedderIsolationInfo(Mode::kNone, /*instance_id=*/0);
}

// static
EmbedderIsolationInfo EmbedderIsolationInfo::CreateForPdf() {
  return EmbedderIsolationInfo(Mode::kPdf, /*instance_id=*/0);
}

// static
EmbedderIsolationInfo EmbedderIsolationInfo::CreateForUniqueInstance(
    int64_t instance_id) {
  CHECK_GE(instance_id, 0);
  return EmbedderIsolationInfo(Mode::kUniqueInstance, instance_id);
}

EmbedderIsolationInfo::EmbedderIsolationInfo() = default;
EmbedderIsolationInfo::EmbedderIsolationInfo(const EmbedderIsolationInfo&) =
    default;
EmbedderIsolationInfo& EmbedderIsolationInfo::operator=(
    const EmbedderIsolationInfo&) = default;
EmbedderIsolationInfo::~EmbedderIsolationInfo() = default;

EmbedderIsolationInfo::EmbedderIsolationInfo(Mode mode, int64_t instance_id)
    : mode_(mode), instance_id_(instance_id) {}

std::string EmbedderIsolationInfo::ToDebugString() const {
  switch (mode_) {
    case Mode::kNone:
      return "none";
    case Mode::kPdf:
      return "pdf";
    case Mode::kUniqueInstance:
      return base::StringPrintf("unique_instance(id=%" PRId64 ")",
                                instance_id_);
  }
}

}  // namespace content
