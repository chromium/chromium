// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREVIEW_CANCEL_REASON_H_
#define CONTENT_PUBLIC_BROWSER_PREVIEW_CANCEL_REASON_H_

#include <string>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

// TODO(b:316209095): Report UMA
enum class PreviewFinalStatus {
  kActivated = 0,
  kCancelledByWindowClose = 1,
  kBlockedByMojoBinderPolicy = 2,
  kBlockedByNonHttps = 3,

  kMaxValue = kBlockedByNonHttps,
};

class CONTENT_EXPORT PreviewCancelReason {
 public:
  static PreviewCancelReason Build(PreviewFinalStatus final_status);
  static PreviewCancelReason BlockedByMojoBinderPolicy(
      std::string interface_name);

  // Move only
  PreviewCancelReason(PreviewCancelReason&& other);
  PreviewCancelReason& operator=(PreviewCancelReason&& other);
  PreviewCancelReason(const PreviewCancelReason&) = delete;
  PreviewCancelReason& operator=(const PreviewCancelReason&) = delete;

  ~PreviewCancelReason();

  PreviewFinalStatus GetFinalStatus() const;

 private:
  struct MojoInterfaceName {
    std::string interface_name;
  };

  using ExtraData = absl::variant<absl::monostate, MojoInterfaceName>;

  PreviewCancelReason(PreviewFinalStatus final_status, ExtraData extra_data);

  PreviewFinalStatus final_status_;
  ExtraData extra_data_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREVIEW_CANCEL_REASON_H_
