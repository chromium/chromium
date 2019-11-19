// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_set.h"

class GURL;

namespace extensions {
class URLPatternSet;

// The possible type of requirements needed in order to capture the current
// page.
enum class CaptureRequirement {
  kActiveTabOrAllUrls,  // The extension needs to have the <all_urls> or
                        // activeTab permission in order to capture the current
                        // page.
  kPageCapture,         // <all_urls> is not a requirement to be able to capture
                        // the current page.
};

// A container for the permissions state of an extension, including active,
// withheld, and tab-specific permissions.
// Thread-Safety: Since this is an object on the Extension object, *some* thread
// safety is provided. All utility functions for checking if a permission is
// present or an operation is allowed are thread-safe. However, permissions can
// only be set (or updated) on the thread to which this object is bound.
// Permissions may be accessed synchronously on that same thread.
// Accessing on an improper thread will DCHECK().
// This is necessary to prevent a scenario in which one thread will access
// permissions while another thread changes them.
class PermissionsData {
 public:
  // The possible types of access for a given page.
  // TODO(devlin): Sometimes, this is used for things beyond just a "page",
  // such as network request interception or access to a particular frame.
  // Should we update this?  If so, we should also update the titles of the
  // GetPageAccess()/CanAccessPage() methods below.
  enum class PageAccess {
    kDenied,    // The extension is not allowed to access the given page.
    kAllowed,   // The extension is allowed to access the given page.
    kWithheld,  // The browser must determine if the extension can access
                // the given page.
  };

  enum class EffectiveHostPermissionsMode {
    kOmitTabSpecific,
    kIncludeTabSpecific,
  };

  using TabPermissionsMap = std::map<int, std::unique_ptr<const PermissionSet>>;

  // Delegate class to allow different contexts (e.g. browser vs renderer) to
  // have control over policy decisions.
  class PolicyDelegate {
   public:
    virtual ~PolicyDelegate() {}

    // Returns true if script access should be blocked on this page.
    // Otherwise, default policy should decide.
    virtual bool IsRestrictedUrl(const GURL& document_url,
                                 std::string* error) = 0;
  };

  static void SetPolicyDelegate(PolicyDelegate* delegate);

  PermissionsData(const ExtensionId& extension_id,
                  Manifest::Type manifest_type,
                  Manifest::Location location,
                  std::unique_ptr<const PermissionSet> initial_permissions);
  virtual ~PermissionsData();

  // Returns true if the extension is a COMPONENT extension or is on the
  // whitelist of extensions that can script all pages.
  // NOTE: This is static because it is used during extension initialization,
  // before the extension has an associated PermissionsData object.
  static bool CanExecuteScriptEverywhere(const ExtensionId& extension_id,
                                         Manifest::Location location);

  // Returns true if the given |url| is restricted for the given |extension|,
  // as is commonly the case for chrome:// urls.
  // NOTE: You probably want to use CanAccessPage().
  bool IsRestrictedUrl(const GURL& document_url, std::string* error) const;

  // Returns true if the "all_urls" meta-pattern should include access to
  // URLs with the "chrome" scheme. Access to these URLs is limited as they
  // are sensitive.
  static bool AllUrlsIncludesChromeUrls(const std::string& extension_id);

  // Is this extension using the default scope for policy_blocked_hosts and
  // policy_allowed_hosts of the ExtensionSettings policy.
  bool UsesDefaultPolicyHostRestrictions() const;

  // Locks the permissions data to the current thread. We don't do this on
  // construction, since extensions are initialized across multiple threads.
  void BindToCurrentThread() const;

  // Sets the runtime permissions of the given |extension| to |active| and
  // |withheld|.
  void SetPermissions(std::unique_ptr<const PermissionSet> active,
                      std::unique_ptr<const PermissionSet> withheld) const;

  // Applies restrictions from enterprise policy limiting which URLs this
  // extension can interact with. The same policy can also define a default set
  // of URL restrictions using SetDefaultPolicyHostRestrictions. This function
  // overrides any default host restriction policy.
  void SetPolicyHostRestrictions(
      const URLPatternSet& policy_blocked_hosts,
      const URLPatternSet& policy_allowed_hosts) const;

  // Marks this extension as using default enterprise policy limiting
  // which URLs extensions can interact with. A default policy can be set with
  // SetDefaultPolicyHostRestrictions. A policy specific to this extension
  // can be set with SetPolicyHostRestrictions.
  void SetUsesDefaultHostRestrictions() const;

  // Applies restrictions from enterprise policy limiting which URLs all
  // extensions can interact with. This restriction can be overridden on a
  // per-extension basis with SetPolicyHostRestrictions.
  static void SetDefaultPolicyHostRestrictions(
      const URLPatternSet& default_policy_blocked_hosts,
      const URLPatternSet& default_policy_allowed_hosts);

  // Updates the tab-specific permissions of |tab_id| to include those from
  // |permissions|.
  void UpdateTabSpecificPermissions(int tab_id,
                                    const PermissionSet& permissions) const;

  // Clears the tab-specific permissions of |tab_id|.
  void ClearTabSpecificPermissions(int tab_id) const;

  // Returns true if the |extension| has the given |permission|. Prefer
  // IsExtensionWithPermissionOrSuggestInConsole when developers may be using an
  // api that requires a permission they didn't know about, e.g. open web apis.
  // Note this does not include APIs with no corresponding permission, like
  // "runtime" or "browserAction".
  // TODO(mpcomplete): drop the "API" from these names, it's confusing.
  bool HasAPIPermission(APIPermission::ID permission) const;
  bool HasAPIPermission(const std::string& permission_name) const;
  bool HasAPIPermissionForTab(int tab_id, APIPermission::ID permission) const;
  bool CheckAPIPermissionWithParam(
      APIPermission::ID permission,
      const APIPermission::CheckParam* param) const;

  // Returns the hosts this extension effectively has access to, including
  // explicit and scriptable hosts, and any hosts on tabs the extension has
  // active tab permissions for.
  URLPatternSet GetEffectiveHostPermissions(
      EffectiveHostPermissionsMode mode) const;

  // TODO(rdevlin.cronin): HasHostPermission() and
  // HasEffectiveAccessToAllHosts() are just forwards for the active
  // permissions. We should either get rid of these, and have callers use
  // active_permissions(), or should get rid of active_permissions(), and make
  // callers use PermissionsData for everything. We should not do both.

  // Whether the extension has access to the given |url|.
  bool HasHostPermission(const GURL& url) const;

  // Whether the extension has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  bool HasEffectiveAccessToAllHosts() const;

  // Returns the full list of permission details for messages that should
  // display at install time, in a nested format ready for display.
  PermissionMessages GetPermissionMessages() const;

  // Returns the list of permission details for permissions that are included in
  // active_permissions(), but not present in |granted_permissions|.  These are
  // returned in a nested format, ready for display.
  PermissionMessages GetNewPermissionMessages(
      const PermissionSet& granted_permissions) const;

  // Returns true if the associated extension has permission to access and
  // interact with the specified page, in order to do things like inject
  // scripts or modify the content.
  // If this returns false and |error| is non-NULL, |error| will be popualted
  // with the reason the extension cannot access the page.
  bool CanAccessPage(const GURL& document_url,
                     int tab_id,
                     std::string* error) const;
  // Like CanAccessPage, but also takes withheld permissions into account.
  // TODO(rdevlin.cronin) We shouldn't have two functions, but not all callers
  // know how to wait for permission.
  PageAccess GetPageAccess(const GURL& document_url,
                           int tab_id,
                           std::string* error) const;

  // Returns true if the associated extension has permission to inject a
  // content script on the page.
  // If this returns false and |error| is non-NULL, |error| will be popualted
  // with the reason the extension cannot script the page.
  // NOTE: You almost certainly want to use CanAccessPage() instead of this
  // method.
  bool CanRunContentScriptOnPage(const GURL& document_url,
                                 int tab_id,
                                 std::string* error) const;
  // Like CanRunContentScriptOnPage, but also takes withheld permissions into
  // account.
  // TODO(rdevlin.cronin) We shouldn't have two functions, but not all callers
  // know how to wait for permission.
  PageAccess GetContentScriptAccess(const GURL& document_url,
                                    int tab_id,
                                    std::string* error) const;

  // Returns true if the associated extension is allowed to obtain the contents
  // of a page as an image. Pages may contain multiple sources (e.g.,
  // example.com may embed google.com), so simply checking the top-frame's URL
  // is insufficient.
  // Instead:
  // - If the page is a chrome:// page, require activeTab.
  // - For all other pages, ensure |capture_requirement| is satisfied.
  bool CanCaptureVisiblePage(const GURL& document_url,
                             int tab_id,
                             std::string* error,
                             CaptureRequirement capture_requirement) const;

  const TabPermissionsMap& tab_specific_permissions() const {
    DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
    return tab_specific_permissions_;
  }

  const PermissionSet& active_permissions() const {
    DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
    return *active_permissions_unsafe_;
  }

  const PermissionSet& withheld_permissions() const {
    DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
    return *withheld_permissions_unsafe_;
  }

  // Returns list of hosts this extension may not interact with by policy.
  // This should only be used for 1. Serialization when initializing renderers
  // or 2. Called from utility methods above. For all other uses, call utility
  // methods instead (e.g. CanAccessPage()).
  static URLPatternSet default_policy_blocked_hosts();

  // Returns list of hosts this extension may interact with regardless of
  // what is defined by policy_blocked_hosts().
  // This should only be used for 1. Serialization when initializing renderers
  // or 2. Called from utility methods above. For all other uses, call utility
  // methods instead (e.g. CanAccessPage()).
  static URLPatternSet default_policy_allowed_hosts();

  // Returns list of hosts this extension may not interact with by policy.
  // This should only be used for 1. Serialization when initializing renderers
  // or 2. Called from utility methods above. For all other uses, call utility
  // methods instead (e.g. CanAccessPage()).
  URLPatternSet policy_blocked_hosts() const;

  // Returns list of hosts this extension may interact with regardless of
  // what is defined by policy_blocked_hosts().
  // This should only be used for 1. Serialization when initializing renderers
  // or 2. Called from utility methods above. For all other uses, call utility
  // methods instead (e.g. CanAccessPage()).
  URLPatternSet policy_allowed_hosts() const;

  // Check if a specific URL is blocked by policy from extension use at runtime.
  bool IsPolicyBlockedHost(const GURL& url) const {
    base::AutoLock auto_lock(runtime_lock_);
    return IsPolicyBlockedHostUnsafe(url);
  }

#if defined(UNIT_TEST)
  const PermissionSet* GetTabSpecificPermissionsForTesting(int tab_id) const {
    base::AutoLock auto_lock(runtime_lock_);
    return GetTabSpecificPermissions(tab_id);
  }
#endif

 private:
  // Gets the tab-specific host permissions of |tab_id|, or NULL if there
  // aren't any.
  // Must be called with |runtime_lock_| acquired.
  const PermissionSet* GetTabSpecificPermissions(int tab_id) const;

  // Returns whether or not the extension is permitted to run on the given page,
  // checking against |permitted_url_patterns| and |tab_url_patterns| in
  // addition to blocking special sites (like the webstore or chrome:// urls).
  // Must be called with |runtime_lock_| acquired.
  PageAccess CanRunOnPage(const GURL& document_url,
                          int tab_id,
                          const URLPatternSet& permitted_url_patterns,
                          const URLPatternSet& withheld_url_patterns,
                          const URLPatternSet* tab_url_patterns,
                          std::string* error) const;

  // Check if a specific URL is blocked by policy from extension use at runtime.
  // You must acquire the runtime_lock_ before calling.
  bool IsPolicyBlockedHostUnsafe(const GURL& url) const;

  // The associated extension's id.
  std::string extension_id_;

  // The associated extension's manifest type.
  Manifest::Type manifest_type_;

  // The associated extension's location.
  Manifest::Location location_;

  mutable base::Lock runtime_lock_;

  // The permission's which are currently active on the extension during
  // runtime.
  // Unsafe indicates that we must lock anytime this is directly accessed.
  // Unless you need to change |active_permissions_unsafe_|, use the (safe)
  // active_permissions() accessor.
  mutable std::unique_ptr<const PermissionSet> active_permissions_unsafe_;

  // The permissions the extension requested, but was not granted due because
  // they are too powerful. This includes things like all_hosts.
  // Unsafe indicates that we must lock anytime this is directly accessed.
  // Unless you need to change |withheld_permissions_unsafe_|, use the (safe)
  // withheld_permissions() accessor.
  mutable std::unique_ptr<const PermissionSet> withheld_permissions_unsafe_;

  // The list of hosts an extension may not interact with by policy.
  // Unless you need to change |policy_blocked_hosts_unsafe_|, use the (safe)
  // policy_blocked_hosts() accessor.
  mutable URLPatternSet policy_blocked_hosts_unsafe_;

  // The exclusive list of hosts an extension may interact with by policy.
  // Unless you need to change |policy_allowed_hosts_unsafe_|, use the (safe)
  // policy_allowed_hosts() accessor.
  mutable URLPatternSet policy_allowed_hosts_unsafe_;

  // If the ExtensionSettings policy is not being used, or no per-extension
  // exception to the default policy was declared for this extension.
  mutable bool uses_default_policy_host_restrictions_ = true;

  mutable TabPermissionsMap tab_specific_permissions_;

  mutable std::unique_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsData);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_
