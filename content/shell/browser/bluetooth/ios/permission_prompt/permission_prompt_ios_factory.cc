// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  // TODO(crbug.com/40263537): Implement PermissionPrompt.
  return nullptr;
}

}  // namespace permissions
