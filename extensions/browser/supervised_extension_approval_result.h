// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SUPERVISED_EXTENSION_APPROVAL_RESULT_H_
#define EXTENSIONS_BROWSER_SUPERVISED_EXTENSION_APPROVAL_RESULT_H_

namespace extensions {

// Result of the supervised user extension approval flow.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.extensions.browser
enum class SupervisedExtensionApprovalResult {
  kApproved,  // Extension installation was approved.
  kCanceled,  // Extension approval flow was canceled.
  kFailed,    // Extension approval failed due to an error.
  kBlocked,   // Extension installation has been blocked by a parent.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUPERVISED_EXTENSION_APPROVAL_RESULT_H_
