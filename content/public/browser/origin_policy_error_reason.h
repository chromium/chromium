// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_ERROR_REASON_H_
#define CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_ERROR_REASON_H_

namespace content {

// Enumerate the reasons why an origin policy was rejected.
enum class OriginPolicyErrorReason : int {
  kCannotLoadPolicy,         // The policy document could not be downloaded.
  kPolicyShouldNotRedirect,  // The policy doc request was met with a redirect.
  kOther,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_ERROR_REASON_H_
