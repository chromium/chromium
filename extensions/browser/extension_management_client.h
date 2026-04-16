// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MANAGEMENT_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSION_MANAGEMENT_CLIENT_H_

#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

class GURL;

namespace extensions {

class Extension;
class URLPatternSet;

// Provides access to policies, preferences, and other embedder-specific
// settings managed by ExtensionManagement.
class ExtensionManagementClient {
 public:
  virtual ~ExtensionManagementClient() = default;

  // Returns true if this extension's update URL is from webstore.
  virtual bool UpdatesFromWebstore(const Extension& extension) = 0;

  // Returns if an extension with id `id` is explicitly allowed by enterprise
  // policy or not.
  virtual bool IsInstallationExplicitlyAllowed(const ExtensionId& id) = 0;

  // Returns true if a force-installed extension is in a low-trust environment.
  // Only applies to Windows and MacOS.
  virtual bool IsForceInstalledInLowTrustEnvironment(
      const Extension& extension) = 0;

  // Returns the list of hosts blocked by policy for the given `extension`.
  virtual const URLPatternSet& GetPolicyBlockedHosts(
      const Extension* extension) = 0;

  // Returns the hosts exempted by policy from the PolicyBlockedHosts for
  // the given `extension`.
  virtual const URLPatternSet& GetPolicyAllowedHosts(
      const Extension* extension) = 0;

  // Checks if the given `extension` has its own runtime_blocked_hosts or
  // runtime_allowed_hosts defined within the individual scope of the
  // ExtensionSettings policy under ExtensionManagement.
  // Returns false if an individual scoped setting isn't defined.
  virtual bool UsesDefaultPolicyHostRestrictions(
      const Extension* extension) = 0;

  // Checks if extensions are blocklisted by default, by policy. When true,
  // this means that even extensions without an ID should be blocklisted (e.g.
  // from the command line, or when loaded as an unpacked extension).
  virtual bool BlocklistedByDefault() const = 0;

  // Get the effective update URL for the extension. Normally this URL comes
  // from the extension manifest, but may be overridden by policies.
  virtual GURL GetEffectiveUpdateURL(const Extension& extension) = 0;

  // Returns true if the extension associated with the given `extension_id` is
  // exempt from the MV2 deprecation because of an active admin policy.
  virtual bool IsExemptFromMV2DeprecationByPolicy(
      int manifest_version,
      const std::string& extension_id,
      Manifest::Type manifest_type) = 0;

  // Checks if the specified manifest version is permitted for an extension,
  // based on its ID and manifest type.
  virtual bool IsAllowedManifestVersion(int manifest_version,
                                        const std::string& extension_id,
                                        Manifest::Type manifest_type) = 0;

  // Checks if the manifest version of the given extension is permitted.
  virtual bool IsAllowedManifestVersion(const Extension* extension) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MANAGEMENT_CLIENT_H_
