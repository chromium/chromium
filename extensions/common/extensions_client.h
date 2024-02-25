// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "extensions/common/features/feature.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-forward.h"

class GURL;

namespace base {
class CommandLine;
class FilePath;
}

namespace extensions {

class APIPermissionSet;
class Extension;
class ExtensionsAPIProvider;
class FeatureProvider;
class JSONFeatureProviderSource;
class PermissionMessageProvider;
class URLPatternSet;

// Sets up global state for the extensions system. Should be Set() once in each
// process. This should be implemented by the client of the extensions system.
class ExtensionsClient {
 public:
  using ScriptingAllowlist = std::vector<std::string>;

  // Return the extensions client.
  static ExtensionsClient* Get();

  // Initialize the extensions system with this extensions client.
  static void Set(ExtensionsClient* client);

  ExtensionsClient();
  ExtensionsClient(const ExtensionsClient&) = delete;
  ExtensionsClient& operator=(const ExtensionsClient&) = delete;
  virtual ~ExtensionsClient();

  void SetFeatureDelegatedAvailabilityCheckMap(
      Feature::FeatureDelegatedAvailabilityCheckMap map);
  const Feature::FeatureDelegatedAvailabilityCheckMap&
  GetFeatureDelegatedAvailabilityCheckMap() const;

  // Create a FeatureProvider for a specific feature type, e.g. "permission".
  std::unique_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const;

  // Returns the dictionary of the API features json file.
  // TODO(devlin): We should find a way to remove this.
  std::unique_ptr<JSONFeatureProviderSource> CreateAPIFeatureSource() const;

  // Returns true iff a schema named |name| is generated.
  bool IsAPISchemaGenerated(const std::string& name) const;

  // Gets the generated API schema named |name|.
  std::string_view GetAPISchema(const std::string& name) const;

  // Adds a new API provider.
  void AddAPIProvider(std::unique_ptr<ExtensionsAPIProvider> provider);

  //////////////////////////////////////////////////////////////////////////////
  // Virtual Functions:

  // Initializes global state. Not done in the constructor because unit tests
  // can create additional ExtensionsClients because the utility thread runs
  // in-process.
  virtual void Initialize() = 0;

  // Initializes web store URLs.
  // Default values could be overriden with command line.
  virtual void InitializeWebStoreUrls(base::CommandLine* command_line) = 0;

  // Returns the global PermissionMessageProvider to use to provide permission
  // warning strings.
  virtual const PermissionMessageProvider& GetPermissionMessageProvider()
      const = 0;

  // Returns the application name. For example, "Chromium" or "app_shell".
  virtual const std::string GetProductName() = 0;

  // Takes the list of all hosts and filters out those with special
  // permission strings. Adds the regular hosts to |new_hosts|,
  // and adds any additional permissions to |permissions|.
  // TODO(sashab): Split this function in two: One to filter out ignored host
  // permissions, and one to get permissions for the given hosts.
  virtual void FilterHostPermissions(const URLPatternSet& hosts,
                                     URLPatternSet* new_hosts,
                                     PermissionIDSet* permissions) const = 0;

  // Replaces the scripting allowlist with |allowlist|. Used in the renderer;
  // only used for testing in the browser process.
  virtual void SetScriptingAllowlist(const ScriptingAllowlist& allowlist) = 0;

  // Return the allowlist of extensions that can run content scripts on
  // any origin.
  virtual const ScriptingAllowlist& GetScriptingAllowlist() const = 0;

  // Get the set of chrome:// hosts that |extension| can have host permissions
  // for.
  virtual URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const = 0;

  // Returns false if content scripts are forbidden from running on |url|.
  virtual bool IsScriptableURL(const GURL& url, std::string* error) const = 0;

  // Returns the base webstore URL prefix.
  virtual const GURL& GetWebstoreBaseURL() const = 0;

  // Returns the base webstore URL prefix for the new webstore. This is defined
  // separately rather than just changing what GetWebstoreBaseURL returns, as
  // during the transition some functionality needs to operate across both the
  // old and the new domain.
  virtual const GURL& GetNewWebstoreBaseURL() const = 0;

  // Returns the URL to use for update manifest queries.
  virtual const GURL& GetWebstoreUpdateURL() const = 0;

  // Returns a flag indicating whether or not a given URL is a valid
  // extension blocklist URL.
  virtual bool IsBlocklistUpdateURL(const GURL& url) const = 0;

  // Returns the set of file paths corresponding to any images within an
  // extension's contents that may be displayed directly within the browser UI
  // or WebUI, such as icons or theme images. This set of paths is used by the
  // extension unpacker to determine which assets should be transcoded safely
  // within the utility sandbox.
  //
  // The default implementation returns the images used as icons for the
  // extension itself, so implementors of ExtensionsClient overriding this may
  // want to call the base class version and then add additional paths to that
  // result.
  virtual std::set<base::FilePath> GetBrowserImagePaths(
      const Extension* extension);

  // Adds client specific permitted origins to |origin_patterns| for
  // cross-origin communication for an extension context.
  virtual void AddOriginAccessPermissions(
      const Extension& extension,
      bool is_extension_active,
      std::vector<network::mojom::CorsOriginPatternPtr>* origin_patterns) const;

  // Returns the extended error code used by the embedder when an extension
  // blocks a request. Returns std::nullopt if the embedder doesn't define such
  // an error code.
  virtual std::optional<int> GetExtensionExtendedErrorCode() const;

 private:
  // Performs common initialization and calls Initialize() to allow subclasses
  // to do any extra initialization.
  void DoInitialize();

  std::vector<std::unique_ptr<ExtensionsAPIProvider>> api_providers_;

  Feature::FeatureDelegatedAvailabilityCheckMap availability_check_map_;

  // Whether DoInitialize() has been called.
  bool initialize_called_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
