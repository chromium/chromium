// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MANAGEMENT_POLICY_H_
#define EXTENSIONS_BROWSER_MANAGEMENT_POLICY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/extension.h"

namespace extensions {

// This class registers providers that want to prohibit certain actions from
// being applied to extensions. It must be called, via the ExtensionService,
// before allowing a user or a user-level mechanism to perform the respective
// action. (That is, installing or otherwise modifying an extension in order
// to conform to enterprise administrator policy must be exempted from these
// checks.)
//
// This "policy" and its providers should not be confused with administrator
// policy, although admin policy is one of the sources ("Providers") of
// restrictions registered with and exposed by the ManagementPolicy.
class ManagementPolicy {
 public:
  // Each mechanism that wishes to limit users' ability to control extensions,
  // whether one individual extension or the whole system, should implement
  // the methods of this Provider interface that it needs. In each case, if the
  // provider does not need to control a certain action, that method does not
  // need to be implemented.
  //
  // It is not guaranteed that a particular Provider's methods will be called
  // each time a user tries to perform one of the controlled actions (the list
  // of providers is short-circuited as soon as a decision is possible), so
  // implementations of these methods must have no side effects.
  //
  // For all of the Provider methods below, if |error| is not NULL and the
  // method imposes a restriction on the desired action, |error| may be set
  // to an applicable error message, but this is not required.
  class Provider {
   public:
    Provider() {}
    virtual ~Provider() {}

    // A human-readable name for this provider, for use in debug messages.
    // Implementers should return an empty string in non-debug builds, to save
    // executable size, and should not call this in builds without DCHECKs
    // enabled.
    virtual std::string GetDebugPolicyProviderName() const = 0;

    // Providers should return false if a user may not install the |extension|,
    // or load or run it if it has already been installed.
    // TODO(crbug.com/461747): The method name is misleading, since this applies
    // to all extension installations, not just user-initiated ones. Fix either
    // the name or the semantics.
    virtual bool UserMayLoad(const Extension* extension,
                             base::string16* error) const;

    // Returns false if the user should not be allowed to install the given
    // |extension|. By default, this forwards to UserMayLoad() (since a user
    // should not be able to install an extension they cannot load).
    virtual bool UserMayInstall(const Extension* extension,
                                base::string16* error) const;

    // Providers should return false if a user may not enable, disable, or
    // uninstall the |extension|, or change its usage options (incognito
    // permission, file access, etc.).
    // TODO(crbug.com/461747): The method name is misleading, since this applies
    // to all setting modifications, not just user-initiated ones. Fix either
    // the name or the semantics.
    virtual bool UserMayModifySettings(const Extension* extension,
                                       base::string16* error) const;

    // Providers should return false if the originating extension
    // |source_extension| cannot disable the |extension|.
    virtual bool ExtensionMayModifySettings(const Extension* source_extension,
                                            const Extension* extension,
                                            base::string16* error) const;

    // Providers should return true if the |extension| must always remain
    // enabled. This is distinct from UserMayModifySettings() in that the latter
    // also prohibits enabling the extension if it is currently disabled.
    // Providers implementing this method should also implement the others
    // above, if they wish to completely lock in an extension.
    virtual bool MustRemainEnabled(const Extension* extension,
                                   base::string16* error) const;

    // Similar to MustRemainEnabled, but for whether an extension must remain
    // disabled, and returns an error and/or reason if the caller needs it.
    virtual bool MustRemainDisabled(const Extension* extension,
                                    disable_reason::DisableReason* reason,
                                    base::string16* error) const;

    // Similar to MustRemainEnabled, but for whether an extension must remain
    // installed, and returns an error and/or reason if the caller needs it.
    virtual bool MustRemainInstalled(const Extension* extension,
                                     base::string16* error) const;

    // Providers should return true for extensions that should be force
    // uninstalled.
    virtual bool ShouldForceUninstall(const Extension* extension,
                                      base::string16* error) const;

   private:
    DISALLOW_COPY_AND_ASSIGN(Provider);
  };

  ManagementPolicy();
  ~ManagementPolicy();

  // Registers or unregisters a provider, causing it to be added to or removed
  // from the list of providers queried. Ownership of the provider remains with
  // the caller. Providers do not need to be unregistered on shutdown.
  void RegisterProvider(Provider* provider);
  void UnregisterProvider(Provider* provider);

  // Like RegisterProvider(), but registers multiple providers instead.
  void RegisterProviders(
      const std::vector<std::unique_ptr<Provider>>& providers);

  // Returns true if the user is permitted to install, load, and run the given
  // extension. If not, |error| may be set to an appropriate message.
  // Installed extensions failing this check are disabled with the reason
  // DISABLE_BLOCKED_BY_POLICY.
  // TODO(crbug.com/461747): Misleading name; see comment in Provider.
  bool UserMayLoad(const Extension* extension, base::string16* error) const;

  // Returns false if the user should not be allowed to install the given
  // |extension|. By default, this forwards to UserMayLoad() (since a user
  // should not be able to install an extension they cannot load).
  bool UserMayInstall(const Extension* extension, base::string16* error) const;

  // Returns true if the user is permitted to enable, disable, or uninstall the
  // given extension, or change the extension's usage options (incognito mode,
  // file access, etc.). If not, |error| may be set to an appropriate message.
  // TODO(crbug.com/461747): Misleading name; see comment in Provider.
  bool UserMayModifySettings(const Extension* extension,
                             base::string16* error) const;

  // Returns true if the originating extension is permitted to disable the
  // given extension. If not, |error| may be set to an appropriate message.
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  base::string16* error) const;

  // Returns true if the extension must remain enabled at all times (e.g. a
  // component extension). In that case, |error| may be set to an appropriate
  // message.
  bool MustRemainEnabled(const Extension* extension,
                         base::string16* error) const;

  // Returns true immediately if any registered provider's UserMayLoad() returns
  // false or MustRemainDisabled() returns true.
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          base::string16* error) const;

  // Returns true immediately if any registered provider's MustRemainInstalled
  // function returns true.
  bool MustRemainInstalled(const Extension* extension,
                           base::string16* error) const;

  // Returns true for extensions that should be force uninstalled.
  bool ShouldForceUninstall(const Extension* extension,
                            base::string16* error) const;

  // Returns true if the |extension| should be repaired upon corruption.
  // Note that this method doesn't check whether extension is corrupted or not
  // (it's job of ContentVerifier).
  bool ShouldRepairIfCorrupted(const Extension* extension);

  // For use in testing.
  void UnregisterAllProviders();
  int GetNumProviders() const;

 private:
  // This is a pointer to a function in the Provider interface, used in
  // ApplyToProviderList.
  typedef bool (Provider::*ProviderFunction)(const Extension*,
                                             base::string16*) const;

  typedef std::set<Provider*> ProviderList;

  // This is a helper to apply a method in the Provider interface to each of
  // the Provider objects in |providers_|. The return value of this function
  // will be |normal_result|, unless any of the Provider calls to |function|
  // return !normal_result, in which case this function will then early-return
  // !normal_result.
  bool ApplyToProviderList(ProviderFunction function,
                           const char* debug_operation_name,
                           bool normal_result,
                           const Extension* extension,
                           base::string16* error) const;

  // This stores raw pointers to Provider.
  // TODO(lazyboy): Consider making ManagementPolicy own these providers.
  ProviderList providers_;

  DISALLOW_COPY_AND_ASSIGN(ManagementPolicy);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MANAGEMENT_POLICY_H_
