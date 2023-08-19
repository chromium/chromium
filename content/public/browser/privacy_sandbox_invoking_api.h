// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_INVOKING_API_H_
#define CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_INVOKING_API_H_

namespace content {

// This enum stores all the APIs that can create a FencedFrameReporter.
enum class PrivacySandboxInvokingAPI {
  kProtectedAudience,
  kSharedStorage,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRIVACY_SANDBOX_INVOKING_API_H_
