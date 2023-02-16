// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/policy_check.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

PolicyCheck::PolicyCheck(content::BrowserContext* context,
                         scoped_refptr<const Extension> extension)
    : PreloadCheck(extension), context_(context) {}

PolicyCheck::~PolicyCheck() = default;

void PolicyCheck::Start(ResultCallback callback) {
  Errors errors;
  if (!ExtensionSystem::Get(context_)->management_policy()->UserMayInstall(
          extension(), &error_)) {
    DCHECK(!error_.empty());
    errors.insert(Error::kDisallowedByPolicy);
  }
  std::move(callback).Run(errors);
}

std::u16string PolicyCheck::GetErrorMessage() const {
  return error_;
}

}  // namespace extensions
