// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/preview_cancel_reason.h"

#include "base/check_op.h"

namespace content {

PreviewCancelReason::PreviewCancelReason(PreviewFinalStatus final_status,
                                         ExtraData extra_data)
    : final_status_(final_status), extra_data_(extra_data) {}

PreviewCancelReason::~PreviewCancelReason() = default;

PreviewCancelReason::PreviewCancelReason(PreviewCancelReason&& other) {
  final_status_ = other.final_status_;
  extra_data_ = std::move(other.extra_data_);
}

PreviewCancelReason& PreviewCancelReason::operator=(
    PreviewCancelReason&& other) {
  final_status_ = other.final_status_;
  extra_data_ = std::move(other.extra_data_);

  return *this;
}

// static
PreviewCancelReason PreviewCancelReason::Build(
    PreviewFinalStatus final_status) {
  CHECK_NE(final_status, PreviewFinalStatus::kBlockedByMojoBinderPolicy)
      << "use BlockedByMojoBinderPolicy instead";

  return PreviewCancelReason(final_status, ExtraData());
}

// static
PreviewCancelReason PreviewCancelReason::BlockedByMojoBinderPolicy(
    std::string interface_name) {
  ExtraData extra_data = MojoInterfaceName{.interface_name = interface_name};
  return PreviewCancelReason(PreviewFinalStatus::kBlockedByMojoBinderPolicy,
                             std::move(extra_data));
}

PreviewFinalStatus PreviewCancelReason::GetFinalStatus() const {
  return final_status_;
}

}  // namespace content
