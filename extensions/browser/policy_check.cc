// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/policy_check.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

PolicyCheck::PolicyCheck(content::BrowserContext* context,
                         scoped_refptr<const Extension> extension)
    : PreloadCheck(extension), context_(context) {}

PolicyCheck::~PolicyCheck() = default;

void PolicyCheck::Start(ResultCallback callback) {
  ExtensionSystem::Get(context_)->management_policy()->UserMayInstall(
      extension(),
      base::BindOnce(&PolicyCheck::OnUserMayInstallDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PolicyCheck::OnUserMayInstallDone(ResultCallback callback,
                                       ManagementPolicy::Decision decision) {
  Errors errors;
  if (!decision.allowed) {
    error_ = std::move(decision.error);
    DCHECK(!error_.empty());
    errors.insert(Error::kDisallowedByPolicy);
  }
  std::move(callback).Run(errors);
}

std::u16string PolicyCheck::GetErrorMessage() const {
  return error_;
}

}  // namespace extensions
