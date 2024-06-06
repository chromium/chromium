// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_data.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/stack_allocated.h"
#include "base/no_destructor.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = nullptr;

struct URLPatternAccessSet {
  URLPatternSet blocked_hosts;
  URLPatternSet allowed_hosts;
};

struct ContextPermissions {
  // The set of default policy restrictions to apply to extensions if a more
  // specific rule isn't set.
  // Extensions cannot interact with the blocked sites, even if the permission
  // is otherwise granted. The allowlist takes precedent over the blocklist.
  URLPatternAccessSet default_policy_restrictions;
  // Restrictions set by the user dictating which sites extensions can / cannot
  // run on. The allowlist takes precedent over the blocklist.
  // Policy-specified hosts take precedent over user-specified hosts.
  URLPatternAccessSet user_restrictions;
};

// A map between a profile (referenced by a unique id) and the default policies
// for that profile. Since different profile have different defaults, we need to
// have separate entries.
using ContextPermissionsMap = std::map<int, ContextPermissions>;

// Lock to access the context permissions map. This should never be acquired
// before PermissionsData instance level |runtime_lock_| to prevent deadlocks.
base::Lock& GetContextPermissionsLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Returns the ContextPermissions for the given context_id.
// Must be called with the context permissions lock already required.
ContextPermissions& GetContextPermissions(int context_id) {
  static base::NoDestructor<ContextPermissionsMap> context_permissions_map;
  GetContextPermissionsLock().AssertAcquired();
  return (*context_permissions_map)[context_id];
}

class AutoLockOnValidThread {
  STACK_ALLOCATED();

 public:
  AutoLockOnValidThread(base::Lock& lock, base::ThreadChecker* thread_checker)
      : auto_lock_(lock) {
    DCHECK(!thread_checker || thread_checker->CalledOnValidThread());
  }

  AutoLockOnValidThread(const AutoLockOnValidThread&) = delete;
  AutoLockOnValidThread& operator=(const AutoLockOnValidThread&) = delete;

 private:
  base::AutoLock auto_lock_;
};

}  // namespace

PermissionsData::PermissionsData(
    const ExtensionId& extension_id,
    Manifest::Type manifest_type,
    mojom::ManifestLocation location,
    std::unique_ptr<const PermissionSet> initial_permissions)
    : extension_id_(extension_id),
      manifest_type_(manifest_type),
      location_(location),
      active_permissions_unsafe_(std::move(initial_permissions)),
      withheld_permissions_unsafe_(std::make_unique<PermissionSet>()) {}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
}

// static
bool PermissionsData::CanExecuteScriptEverywhere(
    const ExtensionId& extension_id,
    mojom::ManifestLocation location) {
  if (location == mojom::ManifestLocation::kComponent)
    return true;

  const ExtensionsClient::ScriptingAllowlist& allowlist =
      ExtensionsClient::Get()->GetScriptingAllowlist();

  return base::Contains(allowlist, extension_id);
}

bool PermissionsData::IsRestrictedUrl(const GURL& document_url,
                                      std::string* error) const {
  if (CanExecuteScriptEverywhere(extension_id_, location_))
    return false;

  if (g_policy_delegate &&
      g_policy_delegate->IsRestrictedUrl(document_url, error)) {
    return true;
  }

  // Check if the scheme is valid for extensions. If not, return.
  if (!URLPattern::IsValidSchemeForExtensions(document_url.scheme()) &&
      document_url.spec() != url::kAboutBlankURL &&
      document_url.spec() != url::kAboutSrcdocURL) {
    if (error) {
      if (active_permissions().HasAPIPermission(APIPermissionID::kTab)) {
        *error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
      } else {
        *error = manifest_errors::kCannotAccessPage;
      }
    }
    return true;
  }

  if (!ExtensionsClient::Get()->IsScriptableURL(document_url, error))
    return true;

  bool allow_on_chrome_urls = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kExtensionsOnChromeURLs);
  if (document_url.SchemeIs(content::kChromeUIScheme) &&
      !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessChromeUrl;
    return true;
  }

  if (document_url.SchemeIs(kExtensionScheme) &&
      document_url.host() != extension_id_ && !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessExtensionUrl;
    return true;
  }

  return false;
}

// static
bool PermissionsData::AllUrlsIncludesChromeUrls(
    const ExtensionId& extension_id) {
  return extension_id == extension_misc::kChromeVoxExtensionId;
}

bool PermissionsData::UsesDefaultPolicyHostRestrictions() const {
  DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
  return uses_default_policy_host_restrictions_;
}

// static
URLPatternSet PermissionsData::GetDefaultPolicyBlockedHosts(int context_id) {
  base::AutoLock lock(GetContextPermissionsLock());
  return GetContextPermissions(context_id)
      .default_policy_restrictions.blocked_hosts.Clone();
}

// static
URLPatternSet PermissionsData::GetDefaultPolicyAllowedHosts(int context_id) {
  base::AutoLock lock(GetContextPermissionsLock());
  return GetContextPermissions(context_id)
      .default_policy_restrictions.allowed_hosts.Clone();
}

URLPatternSet PermissionsData::policy_blocked_hosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  if (uses_default_policy_host_restrictions_ && context_id_.has_value())
    return GetDefaultPolicyBlockedHosts(context_id_.value());

  return policy_blocked_hosts_unsafe_.Clone();
}

URLPatternSet PermissionsData::policy_allowed_hosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  if (uses_default_policy_host_restrictions_ && context_id_.has_value())
    return GetDefaultPolicyAllowedHosts(context_id_.value());

  return policy_allowed_hosts_unsafe_.Clone();
}

void PermissionsData::BindToCurrentThread() const {
  DCHECK(!thread_checker_);
  thread_checker_ = std::make_unique<base::ThreadChecker>();
}

void PermissionsData::SetContextId(int context_id) const {
  DCHECK(!context_id_ || context_id_ == context_id);
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  context_id_ = context_id;
}

void PermissionsData::SetPermissions(
    std::unique_ptr<const PermissionSet> active,
    std::unique_ptr<const PermissionSet> withheld) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  active_permissions_unsafe_ = std::move(active);
  withheld_permissions_unsafe_ = std::move(withheld);
}

void PermissionsData::SetPolicyHostRestrictions(
    const URLPatternSet& policy_blocked_hosts,
    const URLPatternSet& policy_allowed_hosts) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  policy_blocked_hosts_unsafe_ = policy_blocked_hosts.Clone();
  policy_allowed_hosts_unsafe_ = policy_allowed_hosts.Clone();
  uses_default_policy_host_restrictions_ = false;
}

void PermissionsData::SetUsesDefaultHostRestrictions() const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  DCHECK(context_id_);
  uses_default_policy_host_restrictions_ = true;
}

// static
void PermissionsData::SetDefaultPolicyHostRestrictions(
    int context_id,
    const URLPatternSet& default_policy_blocked_hosts,
    const URLPatternSet& default_policy_allowed_hosts) {
  base::AutoLock lock(GetContextPermissionsLock());
  ContextPermissions& context_permissions = GetContextPermissions(context_id);
  context_permissions.default_policy_restrictions.blocked_hosts =
      default_policy_blocked_hosts.Clone();
  context_permissions.default_policy_restrictions.allowed_hosts =
      default_policy_allowed_hosts.Clone();
}

// static
void PermissionsData::SetUserHostRestrictions(
    int context_id,
    URLPatternSet user_blocked_hosts,
    URLPatternSet user_allowed_hosts) {
  base::AutoLock lock(GetContextPermissionsLock());
  ContextPermissions& context_permissions = GetContextPermissions(context_id);
  context_permissions.user_restrictions.blocked_hosts =
      std::move(user_blocked_hosts);
  context_permissions.user_restrictions.allowed_hosts =
      std::move(user_allowed_hosts);
}

// static
URLPatternSet PermissionsData::GetUserAllowedHosts(int context_id) {
  base::AutoLock lock(GetContextPermissionsLock());
  return GetContextPermissions(context_id)
      .user_restrictions.allowed_hosts.Clone();
}

// static
URLPatternSet PermissionsData::GetUserBlockedHosts(int context_id) {
  base::AutoLock lock(GetContextPermissionsLock());
  return GetContextPermissions(context_id)
      .user_restrictions.blocked_hosts.Clone();
}

URLPatternSet PermissionsData::GetUserBlockedHosts() const {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    // Feature not enabled.
    return {};
  }

  if (!context_id_) {
    // Context ID is unset, so this extension isn't associated with a context.
    // This happens a) in unit tests and b) in extensions embedders like app
    // shell that don't set the context (and also don't support user host
    // restrictions).
    // TODO(crbug.com/40803363): It'd be nice to change this (even if
    // app shell just sets a global context id) so that we can DCHECK it here.
    // If we didn't have a context ID set in production Chromium, it'd be a bug
    // and would result in the extension potentially having access to user-
    // restricted sites.
    return {};
  }

  if (location_ == mojom::ManifestLocation::kComponent ||
      Manifest::IsPolicyLocation(location_)) {
    // Extension is exempt from user settings.
    return {};
  }

  base::AutoLock lock(GetContextPermissionsLock());
  return GetContextPermissions(*context_id_)
      .user_restrictions.blocked_hosts.Clone();
}

void PermissionsData::UpdateTabSpecificPermissions(
    int tab_id,
    const PermissionSet& permissions) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  std::unique_ptr<const PermissionSet> new_permissions =
      PermissionSet::CreateUnion(
          iter == tab_specific_permissions_.end()
              ? static_cast<const PermissionSet&>(PermissionSet())
              : *iter->second,
          permissions);
  tab_specific_permissions_[tab_id] = std::move(new_permissions);
}

void PermissionsData::ClearTabSpecificPermissions(int tab_id) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  tab_specific_permissions_.erase(tab_id);
}

bool PermissionsData::HasTabPermissionsForSecurityOrigin(
    int tab_id,
    const GURL& url) const {
  base::AutoLock auto_lock(runtime_lock_);
  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return tab_permissions &&
         tab_permissions->effective_hosts().MatchesSecurityOrigin(url);
}

bool PermissionsData::HasAPIPermission(APIPermissionID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission);
}

bool PermissionsData::HasAPIPermission(
    const std::string& permission_name) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission_name);
}

bool PermissionsData::HasAPIPermissionForTab(int tab_id,
                                             APIPermissionID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  if (active_permissions_unsafe_->HasAPIPermission(permission))
    return true;

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return tab_permissions && tab_permissions->HasAPIPermission(permission);
}

bool PermissionsData::CheckAPIPermissionWithParam(
    APIPermissionID permission,
    const APIPermission::CheckParam* param) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->CheckAPIPermissionWithParam(permission,
                                                                 param);
}

URLPatternSet PermissionsData::GetEffectiveHostPermissions() const {
  base::AutoLock auto_lock(runtime_lock_);
  URLPatternSet effective_hosts =
      active_permissions_unsafe_->effective_hosts().Clone();

  for (const auto& val : tab_specific_permissions_)
    effective_hosts.AddPatterns(val.second->effective_hosts());
  return effective_hosts;
}

bool PermissionsData::HasHostPermission(const GURL& url) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasExplicitAccessToOrigin(url) &&
         !IsPolicyBlockedHostUnsafe(url);
}

PermissionMessages PermissionsData::GetPermissionMessages() const {
  base::AutoLock auto_lock(runtime_lock_);
  return PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(
          *active_permissions_unsafe_, manifest_type_));
}

PermissionMessages PermissionsData::GetNewPermissionMessages(
    const PermissionSet& granted_permissions) const {
  base::AutoLock auto_lock(runtime_lock_);

  std::unique_ptr<const PermissionSet> new_permissions =
      PermissionSet::CreateDifference(*active_permissions_unsafe_,
                                      granted_permissions);

  return PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(*new_permissions,
                                                            manifest_type_));
}

bool PermissionsData::CanAccessPage(const GURL& document_url,
                                    int tab_id,
                                    std::string* error) const {
  PageAccess result = GetPageAccess(document_url, tab_id, error);

  // TODO(rdevlin.cronin) Update callers so that they only need
  // PageAccess::kAllowed.
  return result == PageAccess::kAllowed || result == PageAccess::kWithheld;
}

PermissionsData::PageAccess PermissionsData::GetPageAccess(
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return CanRunOnPage(
      document_url, active_permissions_unsafe_->explicit_hosts(),
      withheld_permissions_unsafe_->explicit_hosts(),
      tab_permissions ? &tab_permissions->explicit_hosts() : nullptr, error);
}

bool PermissionsData::CanRunContentScriptOnPage(const GURL& document_url,
                                                int tab_id,
                                                std::string* error) const {
  PageAccess result = GetContentScriptAccess(document_url, tab_id, error);

  // TODO(rdevlin.cronin) Update callers so that they only need
  // PageAccess::kAllowed.
  return result == PageAccess::kAllowed || result == PageAccess::kWithheld;
}

PermissionsData::PageAccess PermissionsData::GetContentScriptAccess(
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return CanRunOnPage(
      document_url, active_permissions_unsafe_->scriptable_hosts(),
      withheld_permissions_unsafe_->scriptable_hosts(),
      tab_permissions ? &tab_permissions->scriptable_hosts() : nullptr, error);
}

bool PermissionsData::CanCaptureVisiblePage(
    const GURL& document_url,
    int tab_id,
    std::string* error,
    CaptureRequirement capture_requirement) const {
  bool has_active_tab = false;
  bool has_all_urls = false;
  bool has_page_capture = false;
  // Check the real origin, in order to account for filesystem:, blob:, etc.
  // (url::Origin grabs the inner origin of these, whereas GURL::GetOrigin()
  // does not.)
  url::Origin origin = url::Origin::Create(document_url);
  const GURL origin_url = origin.GetURL();
  {
    base::AutoLock auto_lock(runtime_lock_);
    // Disallow capturing policy-blocked hosts. No exceptions.
    // Note: This isn't foolproof, since an extension could embed a policy-
    // blocked host in a different page and then capture that, but it's better
    // than nothing (and policy hosts can set their x-frame options
    // accordingly).
    if (location_ != mojom::ManifestLocation::kComponent &&
        IsPolicyBlockedHostUnsafe(origin_url)) {
      if (error)
        *error = extension_misc::kPolicyBlockedScripting;
      return false;
    }

    const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
    has_active_tab = tab_permissions &&
                     tab_permissions->HasAPIPermission(APIPermissionID::kTab);

    // Check if any of the host permissions match all urls. We don't use
    // URLPatternSet::ContainsPattern() here because a) the schemes may be
    // different and b) this is more efficient.
    for (const auto& pattern : active_permissions_unsafe_->explicit_hosts()) {
      if (pattern.match_all_urls()) {
        has_all_urls = true;
        break;
      }
    }

    has_page_capture = active_permissions_unsafe_->HasAPIPermission(
        APIPermissionID::kPageCapture);
  }
  std::string access_error;
  if (capture_requirement == CaptureRequirement::kActiveTabOrAllUrls) {
    if (!has_active_tab && !has_all_urls) {
      if (error)
        *error = manifest_errors::kAllURLOrActiveTabNeeded;
      return false;
    }

    // We check GetPageAccess() (in addition to the <all_urls> and activeTab
    // checks below) for the case of URLs that can be conditionally granted
    // (such as file:// URLs or chrome:// URLs for component extensions). If an
    // extension has <all_urls>, GetPageAccess() will still (correctly) return
    // false if, for instance, the URL is a file:// URL and the extension does
    // not have file access. See https://crbug.com/810220. If the extension has
    // page access (and has activeTab or <all_urls>), allow the capture.
    if (GetPageAccess(origin_url, tab_id, &access_error) ==
        PageAccess::kAllowed)
      return true;
  } else {
    DCHECK_EQ(CaptureRequirement::kPageCapture, capture_requirement);
    if (!has_page_capture) {
      if (error)
        *error = manifest_errors::kPageCaptureNeeded;
      return false;
    }

    // If the URL is a typical web URL, the pageCapture permission is
    // sufficient.
    if ((origin_url.SchemeIs(url::kHttpScheme) ||
         origin_url.SchemeIs(url::kHttpsScheme)) &&
        !extension_urls::IsWebstoreOrigin(origin)) {
      return true;
    }
  }

  // The extension doesn't have explicit page access. However, there are a
  // number of cases where tab capture may still be allowed.

  // First special case: an extension's own pages.
  // These aren't restricted URLs, but won't be matched by <all_urls> or
  // activeTab (since the extension scheme is not included in the list of
  // valid schemes for extension permissions). To capture an extension's own
  // page, either activeTab or <all_urls> is needed (it's no higher privilege
  // than a normal web page). At least one of these is still needed because
  // the extension page may have embedded web content.
  // TODO(devlin): Should activeTab/<all_urls> account for the extension's own
  // domain?
  if (origin_url.host() == extension_id_)
    return true;

  // The following are special cases that require activeTab explicitly. Normal
  // extensions will never have full access to these pages (i.e., can never
  // inject scripts or otherwise modify the page), but capturing the page can
  // still be useful for e.g. screenshots. We allow these pages only if the
  // extension has been explicitly granted activeTab, which serves as a
  // stronger guarantee that the user wants to run the extension on the site.
  // These origins include:
  // - chrome:-scheme pages.
  // - Other extension's pages.
  // - data: URLs (which don't have a defined underlying origin).
  // - The Chrome Web Store.
  bool allowed_with_active_tab =
      origin_url.SchemeIs(content::kChromeUIScheme) ||
      origin_url.SchemeIs(kExtensionScheme) ||
      // Note: The origin of a data: url is empty, so check the url itself.
      document_url.SchemeIs(url::kDataScheme) ||
      extension_urls::IsWebstoreOrigin(origin);

  if (!allowed_with_active_tab) {
    if (error)
      *error = access_error;
    return false;
  }
  // If the extension has activeTab, these origins are allowed.
  if (has_active_tab)
    return true;

  // Otherwise, access is denied.
  if (error)
    *error = manifest_errors::kActiveTabPermissionNotGranted;
  return false;
}

const PermissionSet* PermissionsData::GetTabSpecificPermissions(
    int tab_id) const {
  runtime_lock_.AssertAcquired();
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  return iter != tab_specific_permissions_.end() ? iter->second.get() : nullptr;
}

bool PermissionsData::IsPolicyBlockedHostUnsafe(const GURL& url) const {
  // We don't use [default_]policy_[blocked|allowed]_hosts() to avoid copying
  // URLPatternSet.
  runtime_lock_.AssertAcquired();
  if (uses_default_policy_host_restrictions_ && context_id_.has_value()) {
    base::AutoLock lock(GetContextPermissionsLock());
    const URLPatternAccessSet& default_policy_restrictions =
        GetContextPermissions(*context_id_).default_policy_restrictions;
    return default_policy_restrictions.blocked_hosts.MatchesURL(url) &&
           !default_policy_restrictions.allowed_hosts.MatchesURL(url);
  }

  return policy_blocked_hosts_unsafe_.MatchesURL(url) &&
         !policy_allowed_hosts_unsafe_.MatchesURL(url);
}

PermissionsData::PageAccess PermissionsData::CanRunOnPage(
    const GURL& document_url,
    const URLPatternSet& permitted_url_patterns,
    const URLPatternSet& withheld_url_patterns,
    const URLPatternSet* tab_url_patterns,
    std::string* error) const {
  runtime_lock_.AssertAcquired();
  if (location_ != mojom::ManifestLocation::kComponent &&
      IsPolicyBlockedHostUnsafe(document_url)) {
    if (error)
      *error = extension_misc::kPolicyBlockedScripting;
    return PageAccess::kDenied;
  }

  if (IsRestrictedUrl(document_url, error))
    return PageAccess::kDenied;

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl) &&
      context_id_ && location_ != mojom::ManifestLocation::kComponent &&
      !Manifest::IsPolicyLocation(location_)) {
    base::AutoLock lock(GetContextPermissionsLock());
    auto& context_permissions = GetContextPermissions(*context_id_);
    // Check if the host is restricted by the user. `allowed_hosts` takes
    // precedent over `blocked_hosts`. Note that, today, PermissionsManager
    // ensures there's no overlap, but this will change if/when
    // PermissionsManager uses URLPatterns instead of origins.
    if (context_permissions.user_restrictions.blocked_hosts.MatchesURL(
            document_url) &&
        !context_permissions.user_restrictions.allowed_hosts.MatchesURL(
            document_url)) {
      if (error) {
        // TODO(crbug.com/40803363): What level of information should
        // we specify here? Policy host restrictions pass a descriptive error
        // back to the extension; is there any harm in doing so?
        *error = "Blocked";
      }
      return PageAccess::kDenied;
    }
  }

  if (tab_url_patterns && tab_url_patterns->MatchesURL(document_url))
    return PageAccess::kAllowed;

  if (permitted_url_patterns.MatchesURL(document_url))
    return PageAccess::kAllowed;

  if (withheld_url_patterns.MatchesURL(document_url))
    return PageAccess::kWithheld;

  if (error) {
    if (active_permissions_unsafe_->HasAPIPermission(APIPermissionID::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
    } else {
      *error = manifest_errors::kCannotAccessPage;
    }
  }

  return PageAccess::kDenied;
}

}  // namespace extensions
