// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_APPROVAL_H_
#define EXTENSIONS_BROWSER_INSTALL_APPROVAL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "extensions/browser/manifest_check_level.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/image/image_skia.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace base {
class Version;
}

namespace extensions {

class Extension;
class Manifest;

// Contains information about what parts of the extension install process can be
// skipped or modified. Captures the information about the extension install
// that the user approved, so that we can compare that information against the
// extension downloaded in case there was a gap in time between when the install
// was approved and the CRX was downloaded.
struct InstallApproval : public base::SupportsUserData::Data {
  static std::unique_ptr<InstallApproval> CreateWithInstallPrompt(
      Profile* profile);

  // Creates an InstallApproval for installing a shared module.
  static std::unique_ptr<InstallApproval> CreateForSharedModule(
      Profile* profile);

  // Creates an InstallApproval that will skip putting up an install
  // confirmation prompt if the actual manifest from the extension to be
  // installed matches `parsed_manifest`. The `strict_manifest_check` controls
  // whether we want to require an exact manifest match, or are willing to
  // tolerate a looser check just that the effective permissions are the same.
  static std::unique_ptr<InstallApproval> CreateWithNoInstallPrompt(
      Profile* profile,
      const ExtensionId& extension_id,
      base::Value::Dict parsed_manifest,
      bool strict_manifest_check);

  ~InstallApproval() override;

  // The extension id that was approved for installation.
  ExtensionId extension_id;

  // The profile the extension should be installed into.
  raw_ptr<Profile> profile = nullptr;

  // The expected manifest, before localization.
  std::unique_ptr<Manifest> manifest;

  // Whether to use a bubble notification when an app is installed, instead of
  // the default behavior of transitioning to the new tab page.
  bool use_app_installed_bubble = false;

  // Whether to skip the post install UI like the extension installed bubble.
  bool skip_post_install_ui = false;

  // Whether to skip the install dialog once the extension has been downloaded
  // and unpacked. One reason this can be true is that in the normal webstore
  // installation, the dialog is shown earlier, before any download is done,
  // so there's no need to show it again.
  bool skip_install_dialog = false;

  // Manifest check level for checking actual manifest against expected
  // manifest.
  ManifestCheckLevel manifest_check_level = ManifestCheckLevel::kStrict;

  // The icon to use to display the extension while it is installing.
  gfx::ImageSkia installing_icon;

  // A dummy extension created from `manifest`;
  scoped_refptr<Extension> dummy_extension;

  // Required minimum version.
  std::unique_ptr<base::Version> minimum_version;

  // The authuser index required to download the item being installed. May be
  // the empty string, in which case no authuser parameter is used.
  std::string authuser;

  // Whether the user clicked through the install friction dialog when the
  // extension is not included in the Enhanced Safe Browsing CRX allowlist and
  // the user has enabled Enhanced Protection.
  bool bypassed_safebrowsing_friction = false;

  // Whether to withhold permissions at installation. By default, permissions
  // are granted at installation.
  bool withhold_permissions = false;

 private:
  InstallApproval();
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_APPROVAL_H_
