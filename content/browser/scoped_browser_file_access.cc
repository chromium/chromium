// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scoped_browser_file_access.h"

#include "content/browser/child_process_security_policy_impl.h"

namespace content {

ScopedBrowserFileAccess::ScopedBrowserFileAccess(
    std::vector<base::FilePath> files)
    : owner_token_(base::UnguessableToken::Create()) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  for (const auto& file : files) {
    policy->GrantFileForBrowserUpload(owner_token_, file);
  }
}

ScopedBrowserFileAccess::~ScopedBrowserFileAccess() {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeFileForBrowserUpload(
      owner_token_);
}

}  // namespace content
