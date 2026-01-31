// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_approval.h"

#include <memory>

#include "base/version.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

InstallApproval::InstallApproval() = default;

std::unique_ptr<InstallApproval> InstallApproval::CreateWithInstallPrompt(
    content::BrowserContext* browser_context) {
  // Use `new` due to private constructor.
  std::unique_ptr<InstallApproval> result(new InstallApproval());
  result->browser_context = browser_context;
  return result;
}

std::unique_ptr<InstallApproval> InstallApproval::CreateForSharedModule(
    content::BrowserContext* browser_context) {
  // Use `new` due to private constructor.
  std::unique_ptr<InstallApproval> result(new InstallApproval());
  result->browser_context = browser_context;
  result->skip_install_dialog = true;
  result->skip_post_install_ui = true;
  result->manifest_check_level = ManifestCheckLevel::kNone;
  return result;
}

std::unique_ptr<InstallApproval> InstallApproval::CreateWithNoInstallPrompt(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    base::DictValue parsed_manifest,
    bool strict_manifest_check) {
  // Use `new` due to private constructor.
  std::unique_ptr<InstallApproval> result(new InstallApproval());
  result->extension_id = extension_id;
  result->browser_context = browser_context;
  result->manifest =
      std::make_unique<Manifest>(mojom::ManifestLocation::kInvalidLocation,
                                 std::move(parsed_manifest), extension_id);
  result->skip_install_dialog = true;
  result->manifest_check_level = strict_manifest_check
                                     ? ManifestCheckLevel::kStrict
                                     : ManifestCheckLevel::kLoose;
  return result;
}

InstallApproval::~InstallApproval() = default;

}  // namespace extensions
