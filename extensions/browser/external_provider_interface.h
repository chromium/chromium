// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_
#define EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_

#include <memory>
#include <vector>

#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace base {
class Version;
}

namespace extensions {

struct ExternalInstallInfoFile;
struct ExternalInstallInfoUpdateUrl;

// This class is an abstract class for implementing external extensions
// providers.
class ExternalProviderInterface {
 public:
  // ExternalProvider uses this interface to communicate back to the
  // caller what extensions are registered, and which |id|, |version| and |path|
  // they have. See also VisitRegisteredExtension below. Ownership of |version|
  // is not transferred to the visitor.  Callers of the methods below must
  // ensure that |id| is a valid extension id (use
  // crx_file::id_util::IdIsValid(id)).
  class VisitorInterface {
   public:
    // Return true if the extension install will proceed.  Install will not
    // proceed if the extension is already installed from a higher priority
    // location.
    virtual bool OnExternalExtensionFileFound(
        const ExternalInstallInfoFile& info) = 0;

    // Return true if the extension install will proceed.  Install might not
    // proceed if the extension is already installed from a higher priority
    // location.
    virtual bool OnExternalExtensionUpdateUrlFound(
        const ExternalInstallInfoUpdateUrl& info,
        bool force_update) = 0;

    // Called after all the external extensions have been reported
    // through the above two methods. |provider| is a pointer to the
    // provider that is now ready (typically this), and the
    // implementation of OnExternalProviderReady() should be able to
    // safely assert that provider->IsReady().
    virtual void OnExternalProviderReady(
        const ExternalProviderInterface* provider) = 0;

    // Once this provider becomes "ready", it can send additional external
    // extensions it learns about later on through
    // OnExternalExtensionUpdateUrlFound() or OnExternalExtensionFileFound().
    // This method will be called each time the provider finds a set of
    // updated external extensions.
    virtual void OnExternalProviderUpdateComplete(
        const ExternalProviderInterface* provider,
        const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
        const std::vector<ExternalInstallInfoFile>& file_extensions,
        const std::set<std::string>& removed_extensions) = 0;

   protected:
    virtual ~VisitorInterface() {}
  };

  virtual ~ExternalProviderInterface() {}

  // The visitor (ExtensionsService) calls this function before it goes away.
  virtual void ServiceShutdown() = 0;

  // Enumerate registered extensions, calling
  // OnExternalExtension(File|UpdateUrl)Found on the |visitor| object for each
  // registered extension found if the external loader calls LoadFinished().
  virtual void VisitRegisteredExtension() = 0;

  // Test if this provider has an extension with id |id| registered.
  virtual bool HasExtension(const std::string& id) const = 0;

  // Gets details of an extension by its id.  Output params will be set only
  // if they are not NULL.  If an output parameter is not specified by the
  // provider type, it will not be changed.
  // This function is no longer used outside unit tests.
  virtual bool GetExtensionDetails(
      const std::string& id,
      mojom::ManifestLocation* location,
      std::unique_ptr<base::Version>* version) const = 0;

  // Determines if this provider had loaded the list of external extensions
  // from its source.
  virtual bool IsReady() const = 0;

  // Notifies the provider visitor about the external extensions found with the
  // existing prefs. This method differs from VisitRegisteredExtension() in that
  // it always triggers the OnExternalExtension(File|UpdateUrl)Found() methods
  // and is independent of the external loader calling LoadFinished(). This
  // method does not load the prefs, but uses the ones present in the provider.
  virtual void TriggerOnExternalExtensionFound() = 0;
};

using ProviderCollection =
    std::vector<std::unique_ptr<ExternalProviderInterface>>;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_
