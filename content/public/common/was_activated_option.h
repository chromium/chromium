// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WAS_ACTIVATED_OPTION_H_
#define CONTENT_PUBLIC_COMMON_WAS_ACTIVATED_OPTION_H_

namespace content {

// Whether the navigation should propagate user activation. This can be
// specified by embedders in NavigationController::LoadURLParams.
enum class WasActivatedOption {
  // The content layer should make a decision about whether to propagate user
  // activation.
  kUnknown,

  // The navigation should propagate user activation.
  kYes,

  // The navigation should not propagate user activation.
  kNo,
  kMaxValue = kNo,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_WAS_ACTIVATED_OPTION_H_
