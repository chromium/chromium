// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Used internally only. These bit positions have no relationship to any
// underlying OS and can be changed to accommodate finer-grained permissions.
enum ChildProcessSecurityPermissions {
  READ_FILE_PERMISSION             = 1 << 0,
  WRITE_FILE_PERMISSION            = 1 << 1,
  CREATE_NEW_FILE_PERMISSION       = 1 << 2,
  CREATE_OVERWRITE_FILE_PERMISSION = 1 << 3,
  DELETE_FILE_PERMISSION           = 1 << 4,

  // Used by Media Galleries API
  COPY_INTO_FILE_PERMISSION        = 1 << 5,
};

// Used internally only. Bitmasks that are actually used by the Grant* and Can*
// methods. These contain one or more ChildProcessSecurityPermissions.
enum ChildProcessSecurityGrants {
  READ_FILE_GRANT              = READ_FILE_PERMISSION,
  WRITE_FILE_GRANT             = WRITE_FILE_PERMISSION,

  CREATE_NEW_FILE_GRANT        = CREATE_NEW_FILE_PERMISSION |
                                 COPY_INTO_FILE_PERMISSION,

  CREATE_READ_WRITE_FILE_GRANT = CREATE_NEW_FILE_PERMISSION |
                                 CREATE_OVERWRITE_FILE_PERMISSION |
                                 READ_FILE_PERMISSION |
                                 WRITE_FILE_PERMISSION |
                                 COPY_INTO_FILE_PERMISSION |
                                 DELETE_FILE_PERMISSION,

  COPY_INTO_FILE_GRANT         = COPY_INTO_FILE_PERMISSION,
  DELETE_FILE_GRANT            = DELETE_FILE_PERMISSION,
};

// https://crbug.com/646278 Valid blob URLs should contain canonically
// serialized origins.
bool IsMalformedBlobUrl(const GURL& url) {
  if (!url.SchemeIsBlob())
    return false;

  // If the part after blob: survives a roundtrip through url::Origin, then
  // it's a normal blob URL.
  std::string canonical_origin = url::Origin::Create(url).Serialize();
  canonical_origin.append(1, '/');
  if (base::StartsWith(url.GetContent(), canonical_origin,
                       base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // This is a malformed blob URL.
  return true;
}

// Helper function that checks to make sure calls on
// CanAccessDataForOrigin() are only made on valid threads.
// TODO(acolwell): Expand the usage of this check to other
// ChildProcessSecurityPolicyImpl methods.
bool IsRunningOnExpectedThread() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    return true;
  }

  std::string thread_name(base::PlatformThread::GetName());

  // TODO(acolwell): Remove once all tests are updated to properly
  // identify that they are running on the UI or IO threads.
  if (thread_name.empty())
    return true;

  LOG(ERROR) << "Running on unexpected thread '" << thread_name << "'";
  return false;
}

base::debug::CrashKeyString* GetRequestedOriginCrashKey() {
  static auto* requested_origin_key = base::debug::AllocateCrashKeyString(
      "requested_origin", base::debug::CrashKeySize::Size256);
  return requested_origin_key;
}

base::debug::CrashKeyString* GetKilledProcessOriginLockKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "killed_process_origin_lock", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetCanAccessDataFailureReasonKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "can_access_data_failure_reason", base::debug::CrashKeySize::Size64);
  return crash_key;
}

void LogCanAccessDataForOriginCrashKeys(
    const std::string& expected_process_lock,
    const std::string& killed_process_origin_lock,
    const std::string& requested_origin,
    const std::string& failure_reason) {
  base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                 expected_process_lock);
  base::debug::SetCrashKeyString(GetKilledProcessOriginLockKey(),
                                 killed_process_origin_lock);
  base::debug::SetCrashKeyString(GetRequestedOriginCrashKey(),
                                 requested_origin);
  base::debug::SetCrashKeyString(GetCanAccessDataFailureReasonKey(),
                                 failure_reason);
}

}  // namespace

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicyImpl::SecurityState {
 public:
  explicit SecurityState(BrowserContext* browser_context)
      : enabled_bindings_(0),
        can_read_raw_cookies_(false),
        can_send_midi_sysex_(false),
        browser_context_(browser_context),
        resource_context_(browser_context->GetResourceContext()) {}

  ~SecurityState() {
    storage::IsolatedContext* isolated_context =
        storage::IsolatedContext::GetInstance();
    for (auto iter = filesystem_permissions_.begin();
         iter != filesystem_permissions_.end(); ++iter) {
      isolated_context->RemoveReference(iter->first);
    }
  }

  // Grant permission to request and commit URLs with the specified origin.
  void GrantCommitOrigin(const url::Origin& origin) {
    if (origin.opaque())
      return;
    origin_map_[origin] = CommitRequestPolicy::kCommitAndRequest;
  }

  void GrantRequestOrigin(const url::Origin& origin) {
    if (origin.opaque())
      return;
    // Anything already in |origin_map_| must have at least request permission
    // already. In that case, the emplace() below will be a no-op.
    origin_map_.emplace(origin, CommitRequestPolicy::kRequestOnly);
  }

  void GrantCommitScheme(const std::string& scheme) {
    scheme_map_[scheme] = CommitRequestPolicy::kCommitAndRequest;
  }

  void GrantRequestScheme(const std::string& scheme) {
    // Anything already in |scheme_map_| must have at least request permission
    // already. In that case, the emplace() below will be a no-op.
    scheme_map_.emplace(scheme, CommitRequestPolicy::kRequestOnly);
  }

  // Grant certain permissions to a file.
  void GrantPermissionsForFile(const base::FilePath& file, int permissions) {
    base::FilePath stripped = file.StripTrailingSeparators();
    file_permissions_[stripped] |= permissions;
  }

  // Grant navigation to a file but not the file:// scheme in general.
  void GrantRequestOfSpecificFile(const base::FilePath &file) {
    request_file_set_.insert(file.StripTrailingSeparators());
  }

  // Revokes all permissions granted to a file.
  void RevokeAllPermissionsForFile(const base::FilePath& file) {
    base::FilePath stripped = file.StripTrailingSeparators();
    file_permissions_.erase(stripped);
    request_file_set_.erase(stripped);
  }

  // Grant certain permissions to a file.
  void GrantPermissionsForFileSystem(const std::string& filesystem_id,
                                     int permissions) {
    if (!base::Contains(filesystem_permissions_, filesystem_id))
      storage::IsolatedContext::GetInstance()->AddReference(filesystem_id);
    filesystem_permissions_[filesystem_id] |= permissions;
  }

  bool HasPermissionsForFileSystem(const std::string& filesystem_id,
                                   int permissions) {
    FileSystemMap::const_iterator it =
        filesystem_permissions_.find(filesystem_id);
    if (it == filesystem_permissions_.end())
      return false;
    return (it->second & permissions) == permissions;
  }

#if defined(OS_ANDROID)
  // Determine if the certain permissions have been granted to a content URI.
  bool HasPermissionsForContentUri(const base::FilePath& file,
                                   int permissions) {
    DCHECK(!file.empty());
    DCHECK(file.IsContentUri());
    if (!permissions)
      return false;
    base::FilePath file_path = file.StripTrailingSeparators();
    FileMap::const_iterator it = file_permissions_.find(file_path);
    if (it != file_permissions_.end())
      return (it->second & permissions) == permissions;
    return false;
  }
#endif

  void GrantBindings(int bindings) {
    enabled_bindings_ |= bindings;
  }

  void GrantReadRawCookies() {
    can_read_raw_cookies_ = true;
  }

  void RevokeReadRawCookies() {
    can_read_raw_cookies_ = false;
  }

  void GrantPermissionForMidiSysEx() {
    can_send_midi_sysex_ = true;
  }

  // Determine whether permission has been granted to commit |url|.
  bool CanCommitURL(const GURL& url) {
    DCHECK(!url.SchemeIsBlob() && !url.SchemeIsFileSystem())
        << "inner_url extraction should be done already.";
    // Having permission to a scheme implies permission to all of its URLs.
    auto scheme_judgment = scheme_map_.find(url.scheme());
    if (scheme_judgment != scheme_map_.end() &&
        scheme_judgment->second == CommitRequestPolicy::kCommitAndRequest) {
      return true;
    }

    // Check for permission for specific origin.
    if (CanCommitOrigin(url::Origin::Create(url)))
      return true;

    // file:// URLs may sometimes be more granular, e.g. dragging and dropping a
    // file from the local filesystem. The child itself may not have been
    // granted access to the entire file:// scheme, but it should still be
    // allowed to request the dragged and dropped file.
    if (url.SchemeIs(url::kFileScheme)) {
      base::FilePath path;
      if (net::FileURLToFilePath(url, &path))
        return base::Contains(request_file_set_, path);
    }

    return false;  // Unmentioned schemes are disallowed.
  }

  bool CanRequestURL(const GURL& url) {
    DCHECK(!url.SchemeIsBlob() && !url.SchemeIsFileSystem())
        << "inner_url extraction should be done already.";
    // Having permission to a scheme implies permission to all of its URLs.
    auto scheme_judgment = scheme_map_.find(url.scheme());
    if (scheme_judgment != scheme_map_.end())
      return true;

    if (CanRequestOrigin(url::Origin::Create(url)))
      return true;

    // Otherwise, delegate to CanCommitURL. Unmentioned schemes are disallowed.
    // TODO(dcheng): It would be nice to avoid constructing the origin twice.
    return CanCommitURL(url);
  }

  // Determine if the certain permissions have been granted to a file.
  bool HasPermissionsForFile(const base::FilePath& file, int permissions) {
#if defined(OS_ANDROID)
    if (file.IsContentUri())
      return HasPermissionsForContentUri(file, permissions);
#endif
    if (!permissions || file.empty() || !file.IsAbsolute())
      return false;
    base::FilePath current_path = file.StripTrailingSeparators();
    base::FilePath last_path;
    int skip = 0;
    while (current_path != last_path) {
      base::FilePath base_name = current_path.BaseName();
      if (base_name.value() == base::FilePath::kParentDirectory) {
        ++skip;
      } else if (skip > 0) {
        if (base_name.value() != base::FilePath::kCurrentDirectory)
          --skip;
      } else {
        FileMap::const_iterator it = file_permissions_.find(current_path);
        if (it != file_permissions_.end())
          return (it->second & permissions) == permissions;
      }
      last_path = current_path;
      current_path = current_path.DirName();
    }

    return false;
  }

  void LockToOrigin(const GURL& gurl, BrowsingInstanceId browsing_instance_id) {
    DCHECK(origin_lock_.is_empty());
    DCHECK_NE(SiteInstanceImpl::GetDefaultSiteURL(), gurl);
    origin_lock_ = gurl;
    lowest_browsing_instance_id_ = browsing_instance_id;
  }

  void SetLowestBrowsingInstanceId(
      BrowsingInstanceId new_browsing_instance_id_to_include) {
    DCHECK(!new_browsing_instance_id_to_include.is_null());
    if (lowest_browsing_instance_id_.is_null() ||
        (new_browsing_instance_id_to_include < lowest_browsing_instance_id_)) {
      lowest_browsing_instance_id_ = new_browsing_instance_id_to_include;
    }
  }

  const GURL& origin_lock() const { return origin_lock_; }

  BrowsingInstanceId lowest_browsing_instance_id() {
    return lowest_browsing_instance_id_;
  }

  bool has_web_ui_bindings() const {
    return enabled_bindings_ & kWebUIBindingsPolicyMask;
  }

  bool can_read_raw_cookies() const {
    return can_read_raw_cookies_;
  }

  bool can_send_midi_sysex() const {
    return can_send_midi_sysex_;
  }

  BrowserOrResourceContext GetBrowserOrResourceContext() const {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI) && browser_context_)
      return BrowserOrResourceContext(browser_context_);

    if (BrowserThread::CurrentlyOn(BrowserThread::IO) && resource_context_)
      return BrowserOrResourceContext(resource_context_);

    return BrowserOrResourceContext();
  }

  void ClearBrowserContext() { browser_context_ = nullptr; }

 private:
  enum class CommitRequestPolicy {
    kRequestOnly,
    kCommitAndRequest,
  };

  bool CanCommitOrigin(const url::Origin& origin) {
    auto it = origin_map_.find(origin);
    if (it == origin_map_.end())
      return false;
    return it->second == CommitRequestPolicy::kCommitAndRequest;
  }

  bool CanRequestOrigin(const url::Origin& origin) {
    // Anything already in |origin_map_| must have at least request permissions
    // already.
    return origin_map_.find(origin) != origin_map_.end();
  }

  typedef std::map<std::string, CommitRequestPolicy> SchemeMap;
  typedef std::map<url::Origin, CommitRequestPolicy> OriginMap;

  typedef int FilePermissionFlags;  // bit-set of base::File::Flags
  typedef std::map<base::FilePath, FilePermissionFlags> FileMap;
  typedef std::map<std::string, FilePermissionFlags> FileSystemMap;
  typedef std::set<base::FilePath> FileSet;

  // Maps URL schemes to commit/request policies the child process has been
  // granted. There is no provision for revoking.
  SchemeMap scheme_map_;

  // The map of URL origins to commit/request policies the child process has
  // been granted. There is no provision for revoking.
  OriginMap origin_map_;

  // The set of files the child process is permited to upload to the web.
  FileMap file_permissions_;

  // The set of files the child process is permitted to load.
  FileSet request_file_set_;

  int enabled_bindings_;

  bool can_read_raw_cookies_;

  bool can_send_midi_sysex_;

  GURL origin_lock_;

  // The ID of the BrowsingInstance which locked this process to |origin_lock|.
  // Only valid when |origin_lock_| is non-empty.
  //
  // After a process is locked, it might be reused by navigations from frames
  // in other BrowsingInstances, e.g., when we're over process limit and when
  // those navigations utilize the same process lock.  In those cases, this is
  // guaranteed to be the lowest ID of BrowsingInstances that share this
  // process.
  //
  // This is needed for security checks on the IO thread, where we only know
  // the process ID and need to compute the expected origin lock, which
  // requires knowing the set of applicable isolated origins.
  BrowsingInstanceId lowest_browsing_instance_id_;

  // The set of isolated filesystems the child process is permitted to access.
  FileSystemMap filesystem_permissions_;

  BrowserContext* browser_context_;
  ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::IsolatedOriginEntry(
    const url::Origin& origin,
    BrowsingInstanceId min_browsing_instance_id,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    bool isolate_all_subdomains,
    IsolatedOriginSource source)
    : origin_(origin),
      min_browsing_instance_id_(min_browsing_instance_id),
      browser_context_(browser_context),
      resource_context_(resource_context),
      isolate_all_subdomains_(isolate_all_subdomains),
      source_(source) {
  // If there is a BrowserContext, there must also be a ResourceContext
  // associated with this entry.
  DCHECK_EQ(!browser_context, !resource_context);
}

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::IsolatedOriginEntry(
    const IsolatedOriginEntry& other) = default;

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry&
ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::operator=(
    const IsolatedOriginEntry& other) = default;

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::IsolatedOriginEntry(
    IsolatedOriginEntry&& other) = default;

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry&
ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::operator=(
    IsolatedOriginEntry&& other) = default;

ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::~IsolatedOriginEntry() =
    default;

bool ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::
    AppliesToAllBrowserContexts() const {
  return !browser_context_;
}

bool ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::MatchesProfile(
    const BrowserOrResourceContext& browser_or_resource_context) const {
  DCHECK(IsRunningOnExpectedThread());

  // Globally isolated origins aren't associated with any particular profile
  // and should apply to all profiles.
  if (AppliesToAllBrowserContexts())
    return true;

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    return browser_context_ == browser_or_resource_context.ToBrowserContext();
  } else if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    return resource_context_ == browser_or_resource_context.ToResourceContext();
  }
  NOTREACHED();
  return false;
}

ChildProcessSecurityPolicyImpl::ChildProcessSecurityPolicyImpl() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(url::kHttpScheme);
  RegisterWebSafeScheme(url::kHttpsScheme);
#if BUILDFLAG(ENABLE_WEBSOCKETS)
  RegisterWebSafeScheme(url::kWsScheme);
  RegisterWebSafeScheme(url::kWssScheme);
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
  RegisterWebSafeScheme(url::kFtpScheme);
  RegisterWebSafeScheme(url::kDataScheme);
  RegisterWebSafeScheme("feed");

  // TODO(nick): https://crbug.com/651534 blob: and filesystem: schemes embed
  // other origins, so we should not treat them as web safe. Remove callers of
  // IsWebSafeScheme(), and then eliminate the next two lines.
  RegisterWebSafeScheme(url::kBlobScheme);
  RegisterWebSafeScheme(url::kFileSystemScheme);

  // We know about the following pseudo schemes and treat them specially.
  RegisterPseudoScheme(url::kAboutScheme);
  RegisterPseudoScheme(url::kJavaScriptScheme);
  RegisterPseudoScheme(kViewSourceScheme);
  RegisterPseudoScheme(kGoogleChromeScheme);
}

ChildProcessSecurityPolicyImpl::~ChildProcessSecurityPolicyImpl() {
}

// static
ChildProcessSecurityPolicy* ChildProcessSecurityPolicy::GetInstance() {
  return ChildProcessSecurityPolicyImpl::GetInstance();
}

ChildProcessSecurityPolicyImpl* ChildProcessSecurityPolicyImpl::GetInstance() {
  return base::Singleton<ChildProcessSecurityPolicyImpl>::get();
}

void ChildProcessSecurityPolicyImpl::Add(int child_id,
                                         BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  if (security_state_.count(child_id) != 0) {
    NOTREACHED() << "Add child process at most once.";
    return;
  }

  security_state_[child_id] = std::make_unique<SecurityState>(browser_context);
}

void ChildProcessSecurityPolicyImpl::Remove(int child_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->ClearBrowserContext();

  // Moving the existing SecurityState object into a pending map so
  // that we can preserve permission state and avoid mutations to this
  // state after Remove() has been called.
  pending_remove_state_[child_id] = std::move(state->second);
  security_state_.erase(child_id);

  // |child_id| could be inside tasks that are on the IO thread task queues. We
  // need to keep the |pending_remove_state_| entry around until we have
  // successfully executed a task on the IO thread. This should ensure that any
  // pending tasks on the IO thread will have completed before we remove the
  // entry.
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(
                     [](ChildProcessSecurityPolicyImpl* policy, int child_id) {
                       DCHECK_CURRENTLY_ON(BrowserThread::IO);
                       base::AutoLock lock(policy->lock_);
                       policy->pending_remove_state_.erase(child_id);
                     },
                     base::Unretained(this), child_id));
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Add schemes at most once.";
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme))
      << "Web-safe implies not pseudo.";

  schemes_okay_to_request_in_any_process_.insert(scheme);
  schemes_okay_to_commit_in_any_process_.insert(scheme);
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeIsolatedScheme(
    const std::string& scheme,
    bool always_allow_in_origin_headers) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Add schemes at most once.";
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme))
      << "Web-safe implies not pseudo.";

  schemes_okay_to_request_in_any_process_.insert(scheme);
  if (always_allow_in_origin_headers)
    schemes_okay_to_appear_as_origin_headers_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return base::Contains(schemes_okay_to_request_in_any_process_, scheme);
}

void ChildProcessSecurityPolicyImpl::RegisterPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme)) << "Add schemes at most once.";
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";
  DCHECK_EQ(0U, schemes_okay_to_commit_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return base::Contains(pseudo_schemes_, scheme);
}

void ChildProcessSecurityPolicyImpl::GrantCommitURL(int child_id,
                                                    const GURL& url) {
  // Can't grant the capability to commit invalid URLs.
  if (!url.is_valid())
    return;

  // Can't grant the capability to commit pseudo schemes.
  if (IsPseudoScheme(url.scheme()))
    return;

  url::Origin origin = url::Origin::Create(url);

  // Blob and filesystem URLs require special treatment; grant access to the
  // inner origin they embed instead.
  // TODO(dcheng): Can this logic be simplified to just derive an origin up
  // front and use that? That probably requires fixing GURL canonicalization of
  // blob URLs though. For now, be consistent with how CanRequestURL and
  // CanCommitURL normalize.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return;

    GrantCommitURL(child_id, GURL(origin.Serialize()));
  }

  // TODO(dcheng): In the future, URLs with opaque origins would ideally carry
  // around an origin with them, so we wouldn't need to grant commit access to
  // the entire scheme.
  if (!origin.opaque())
    GrantCommitOrigin(child_id, origin);

  // The scheme has already been whitelisted for every child process, so no need
  // to do anything else.
  if (IsWebSafeScheme(url.scheme()))
    return;

  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  if (origin.opaque()) {
    // If it's impossible to grant commit rights to just the origin (among other
    // things, URLs with non-standard schemes will be treated as opaque
    // origins), then grant access to commit all URLs of that scheme.
    state->second->GrantCommitScheme(url.scheme());
  } else {
    // When the child process has been commanded to request this scheme, grant
    // it the capability to request all URLs of that scheme.
    state->second->GrantRequestScheme(url.scheme());
  }
}

void ChildProcessSecurityPolicyImpl::GrantRequestSpecificFileURL(
    int child_id,
    const GURL& url) {
  if (!url.SchemeIs(url::kFileScheme))
    return;

  {
    base::AutoLock lock(lock_);
    auto state = security_state_.find(child_id);
    if (state == security_state_.end())
      return;

    // When the child process has been commanded to request a file:// URL,
    // then we grant it the capability for that URL only.
    base::FilePath path;
    if (net::FileURLToFilePath(url, &path))
      state->second->GrantRequestOfSpecificFile(path);
  }
}

void ChildProcessSecurityPolicyImpl::GrantReadFile(int child_id,
                                                   const base::FilePath& file) {
  GrantPermissionsForFile(child_id, file, READ_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFile(
    int child_id, const base::FilePath& file) {
  GrantPermissionsForFile(child_id, file, CREATE_READ_WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCopyInto(int child_id,
                                                   const base::FilePath& dir) {
  GrantPermissionsForFile(child_id, dir, COPY_INTO_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantDeleteFrom(
    int child_id, const base::FilePath& dir) {
  GrantPermissionsForFile(child_id, dir, DELETE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionsForFile(file, permissions);
}

void ChildProcessSecurityPolicyImpl::RevokeAllPermissionsForFile(
    int child_id, const base::FilePath& file) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeAllPermissionsForFile(file);
}

void ChildProcessSecurityPolicyImpl::GrantReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, READ_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateFileForFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, CREATE_NEW_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(
      child_id, filesystem_id, CREATE_READ_WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCopyIntoFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, COPY_INTO_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantDeleteFromFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, DELETE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantSendMidiSysExMessage(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionForMidiSysEx();
}

void ChildProcessSecurityPolicyImpl::GrantCommitOrigin(
    int child_id,
    const url::Origin& origin) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantCommitOrigin(origin);
}

void ChildProcessSecurityPolicyImpl::GrantRequestOrigin(
    int child_id,
    const url::Origin& origin) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantRequestOrigin(origin);
}

void ChildProcessSecurityPolicyImpl::GrantRequestScheme(
    int child_id,
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantRequestScheme(scheme);
}

void ChildProcessSecurityPolicyImpl::GrantWebUIBindings(int child_id,
                                                        int bindings) {
  // Only WebUI bindings should come through here.
  CHECK(bindings & kWebUIBindingsPolicyMask);
  CHECK_EQ(0, bindings & ~kWebUIBindingsPolicyMask);

  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(bindings);

  // Web UI bindings need the ability to request chrome: URLs.
  state->second->GrantRequestScheme(kChromeUIScheme);

  // Web UI pages can contain links to file:// URLs.
  state->second->GrantRequestScheme(url::kFileScheme);
}

void ChildProcessSecurityPolicyImpl::GrantReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantReadRawCookies();
}

void ChildProcessSecurityPolicyImpl::RevokeReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeReadRawCookies();
}

bool ChildProcessSecurityPolicyImpl::CanRequestURL(
    int child_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

  const std::string& scheme = url.scheme();

  // Every child process can request <about:blank>, <about:blank?foo>,
  // <about:blank/#foo> and <about:srcdoc>.
  //
  // URLs like <about:version>, <about:crash>, <view-source:...> shouldn't be
  // requestable by any child process.  Also, this case covers
  // <javascript:...>, which should be handled internally by the process and
  // not kicked up to the browser.
  // TODO(dcheng): Figure out why this check is different from CanCommitURL,
  // which checks for direct equality with kAboutBlankURL.
  if (IsPseudoScheme(scheme))
    return url.IsAboutBlank() || url.IsAboutSrcdoc();

  // Blob and filesystem URLs require special treatment; validate the inner
  // origin they embed.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return false;

    url::Origin origin = url::Origin::Create(url);
    return origin.opaque() || CanRequestURL(child_id, GURL(origin.Serialize()));
  }

  if (IsWebSafeScheme(scheme))
    return true;

  {
    base::AutoLock lock(lock_);

    auto state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to request the URL.
    if (state->second->CanRequestURL(url))
      return true;
  }

  // Also allow URLs destined for ShellExecute and not the browser itself.
  return !GetContentClient()->browser()->IsHandledURL(url);
}

bool ChildProcessSecurityPolicyImpl::CanRedirectToURL(const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't redirect to invalid URLs.

  const std::string& scheme = url.scheme();

  // Can't redirect to error pages.
  if (scheme == kChromeErrorScheme)
    return false;

  if (IsPseudoScheme(scheme)) {
    // Redirects to a pseudo scheme (about, javascript, view-source, ...) are
    // not allowed. An exception is made for <about:blank> and its variations.
    return url.IsAboutBlank();
  }

  // Note about redirects and special URLs:
  // * data-url: Blocked by net::DataProtocolHandler::IsSafeRedirectTarget().
  // * filesystem-url: Blocked by
  // storage::FilesystemProtocolHandler::IsSafeRedirectTarget().
  // Depending on their inner origins and if the request is browser-initiated or
  // renderer-initiated, blob-urls might get blocked by CanCommitURL or in
  // DocumentLoader::RedirectReceived. If not blocked, a 'file not found'
  // response will be generated in net::BlobURLRequestJob::DidStart().

  return true;
}

bool ChildProcessSecurityPolicyImpl::CanCommitURL(int child_id,
                                                  const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't commit invalid URLs.

  const std::string& scheme = url.scheme();

  // Of all the pseudo schemes, only about:blank and about:srcdoc are allowed to
  // commit.
  if (IsPseudoScheme(scheme))
    return url.IsAboutBlank() || url.IsAboutSrcdoc();

  // Blob and filesystem URLs require special treatment; validate the inner
  // origin they embed.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return false;

    url::Origin origin = url::Origin::Create(url);
    return origin.opaque() || CanCommitURL(child_id, GURL(origin.Serialize()));
  }

  // With site isolation, a URL from a site may only be committed in a process
  // dedicated to that site.  This check will ensure that |url| can't commit if
  // the process is locked to a different site.
  if (!CanAccessDataForOrigin(child_id, url))
    return false;

  {
    base::AutoLock lock(lock_);

    // Most schemes can commit in any process. Note that we check
    // schemes_okay_to_commit_in_any_process_ here, which is stricter than
    // IsWebSafeScheme().
    //
    // TODO(creis, nick): https://crbug.com/515309: The line below does not
    // enforce that http pages cannot commit in an extension process.
    if (base::Contains(schemes_okay_to_commit_in_any_process_, scheme))
      return true;

    auto state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to commit the URL.
    return state->second->CanCommitURL(url);
  }
}

bool ChildProcessSecurityPolicyImpl::CanReadFile(int child_id,
                                                 const base::FilePath& file) {
  return HasPermissionsForFile(child_id, file, READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadAllFiles(
    int child_id,
    const std::vector<base::FilePath>& files) {
  return std::all_of(files.begin(), files.end(),
                     [this, child_id](const base::FilePath& file) {
                       return CanReadFile(child_id, file);
                     });
}

bool ChildProcessSecurityPolicyImpl::CanReadRequestBody(
    int child_id,
    const storage::FileSystemContext* file_system_context,
    const scoped_refptr<network::ResourceRequestBody>& body) {
  if (!body)
    return true;

  for (const network::DataElement& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementType::kFile:
        if (!CanReadFile(child_id, element.path()))
          return false;
        break;

      case network::mojom::DataElementType::kBytes:
        // Data is self-contained within |body| - no need to check access.
        break;

      case network::mojom::DataElementType::kBlob:
        // No need to validate - the unguessability of the uuid of the blob is a
        // sufficient defense against access from an unrelated renderer.
        break;

      case network::mojom::DataElementType::kDataPipe:
        // Data is self-contained within |body| - no need to check access.
        break;

      case network::mojom::DataElementType::kUnknown:
      default:
        // Fail safe - deny access.
        NOTREACHED();
        return false;
    }
  }
  return true;
}

bool ChildProcessSecurityPolicyImpl::CanReadRequestBody(
    SiteInstance* site_instance,
    const scoped_refptr<network::ResourceRequestBody>& body) {
  DCHECK(site_instance);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int child_id = site_instance->GetProcess()->GetID();

  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      site_instance->GetBrowserContext(), site_instance);
  const storage::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();

  return CanReadRequestBody(child_id, file_system_context, body);
}

bool ChildProcessSecurityPolicyImpl::CanCreateReadWriteFile(
    int child_id,
    const base::FilePath& file) {
  return HasPermissionsForFile(child_id, file, CREATE_READ_WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id, READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     READ_FILE_GRANT | WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCopyIntoFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     COPY_INTO_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanDeleteFromFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     DELETE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  base::AutoLock lock(lock_);
  return ChildProcessHasPermissionsForFile(child_id, file, permissions);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url,
    int permissions) {
  if (!filesystem_url.is_valid())
    return false;

  if (filesystem_url.path().ReferencesParent())
    return false;

  // Any write access is disallowed on the root path.
  if (storage::VirtualPath::IsRootPath(filesystem_url.path()) &&
      (permissions & ~READ_FILE_GRANT)) {
    return false;
  }

  if (filesystem_url.mount_type() == storage::kFileSystemTypeIsolated) {
    // When Isolated filesystems is overlayed on top of another filesystem,
    // its per-filesystem permission overrides the underlying filesystem
    // permissions).
    return HasPermissionsForFileSystem(
        child_id, filesystem_url.mount_filesystem_id(), permissions);
  }

  // If |filesystem_url.origin()| is not committable in this process, then this
  // page should not be able to place content in that origin via the filesystem
  // API either.
  // TODO(lukasza): Audit whether CanAccessDataForOrigin can be used directly
  // here.
  if (!CanCommitURL(child_id, filesystem_url.origin().GetURL()))
    return false;

  int found_permissions = 0;
  {
    base::AutoLock lock(lock_);
    auto found = file_system_policy_map_.find(filesystem_url.type());
    if (found == file_system_policy_map_.end())
      return false;
    found_permissions = found->second;
  }

  if ((found_permissions & storage::FILE_PERMISSION_READ_ONLY) &&
      permissions & ~READ_FILE_GRANT) {
    return false;
  }

  // Note that HasPermissionsForFile (called below) will internally acquire the
  // |lock_|, therefore the |lock_| has to be released before the call (since
  // base::Lock is not reentrant).
  if (found_permissions & storage::FILE_PERMISSION_USE_FILE_PERMISSION)
    return HasPermissionsForFile(child_id, filesystem_url.path(), permissions);

  if (found_permissions & storage::FILE_PERMISSION_SANDBOX)
    return true;

  return false;
}

bool ChildProcessSecurityPolicyImpl::CanReadFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCreateFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         CREATE_NEW_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCreateReadWriteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         CREATE_READ_WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCopyIntoFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         COPY_INTO_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanDeleteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         DELETE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::HasWebUIBindings(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_web_ui_bindings();
}

bool ChildProcessSecurityPolicyImpl::CanReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->can_read_raw_cookies();
}

bool ChildProcessSecurityPolicyImpl::ChildProcessHasPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFile(file, permissions);
}

CanCommitStatus ChildProcessSecurityPolicyImpl::CanCommitOriginAndUrl(
    int child_id,
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    const GURL& url) {
  const url::Origin url_origin = url::Origin::Resolve(url, origin);
  if (!CanAccessDataForOrigin(child_id, url_origin)) {
    // Allow opaque origins w/o precursors to commit.
    // TODO(acolwell): Investigate all cases that trigger this path and fix
    // them so we have precursor information. Remove this logic once that has
    // been completed.
    if (url_origin.opaque() &&
        url_origin.GetTupleOrPrecursorTupleIfOpaque().IsInvalid()) {
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
    }

    // Check for special cases, like blob:null/ and data: URLs, where the
    // origin does not contain information to match against the process lock,
    // but using the whole URL can result in a process lock match.
    const GURL expected_origin_lock =
        SiteInstanceImpl::DetermineProcessLockURL(isolation_context, url);
    const GURL actual_origin_lock = GetOriginLock(child_id);
    if (actual_origin_lock == expected_origin_lock)
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

    // Allow about: pages to commit in a process that does not match the opaque
    // origin's precursor information.
    // TODO(acolwell): Remove this once process selection for about: URLs has
    // been fixed to always match the precursor info.
    if (url_origin.opaque() && url.IsAboutBlank() &&
        !actual_origin_lock.is_empty()) {
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
    }

    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  if (!CanAccessDataForOrigin(child_id, origin)) {
    // Allow opaque origins w/o precursors to commit.
    // TODO(acolwell): Investigate all cases that trigger this path and fix
    // them so we have precursor information. Remove this logic once that has
    // been completed.
    if (origin.opaque() &&
        origin.GetTupleOrPrecursorTupleIfOpaque().IsInvalid()) {
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
    }
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
  }

  // Ensure that the origin derived from |url| is consistent with |origin|.
  // Note: We can't use origin.IsSameOriginWith() here because opaque origins
  // with precursors may have different nonce values.
  const auto url_tuple_or_precursor_tuple =
      url_origin.GetTupleOrPrecursorTupleIfOpaque();
  const auto origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();

  if (!url_tuple_or_precursor_tuple.IsInvalid() &&
      !origin_tuple_or_precursor_tuple.IsInvalid() &&
      origin_tuple_or_precursor_tuple != url_tuple_or_precursor_tuple) {
    // Allow a WebView specific exception for origins that have a data scheme.
    // WebView converts data: URLs into non-opaque data:// origins which is
    // different than what all other builds do. This causes the consistency
    // check to fail because we try to compare a data:// origin with an opaque
    // origin that contains precursor info.
    if (url_tuple_or_precursor_tuple.scheme() == url::kDataScheme &&
        url::AllowNonStandardSchemesForAndroidWebView()) {
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
    }

    // Allow "no access" schemes to commit even though |url_origin| and
    // |origin| tuples don't match. We have to allow this because Blink's
    // SecurityOrigin::CreateWithReferenceOrigin() and url::Origin::Resolve()
    // handle "no access" URLs differently. CreateWithReferenceOrigin() treats
    // "no access" like data: URLs and returns an opaque origin with |origin|
    // as a precursor. Resolve() returns a non-opaque origin consisting of the
    // scheme and host portions of the original URL.
    //
    // TODO(1020201): Make CreateWithReferenceOrigin() & Resolve() consistent
    // with each other and then remove this exception.
    if (base::Contains(url::GetNoAccessSchemes(), url.scheme()))
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
  }

  return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

bool ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(
    int child_id,
    const url::Origin& origin) {
  GURL url_to_check;
  if (origin.opaque()) {
    auto precursor_tuple = origin.GetTupleOrPrecursorTupleIfOpaque();
    if (precursor_tuple.IsInvalid()) {
      // We don't have precursor information so we only allow access if
      // the process lock isn't set yet.
      base::AutoLock lock(lock_);
      SecurityState* security_state = GetSecurityState(child_id);

      if (security_state && security_state->origin_lock().is_empty())
        return true;

      LogCanAccessDataForOriginCrashKeys(
          "(empty)" /* expected_process_lock */,
          GetKilledProcessOriginLock(security_state), origin.GetDebugString(),
          "opaque_origin_without_precursor_in_locked_process");

      return false;
    } else {
      url_to_check = precursor_tuple.GetURL();
    }
  } else {
    url_to_check = origin.GetURL();
  }
  bool success = CanAccessDataForOrigin(child_id, url_to_check);
  if (success)
    return true;

  // Note: LogCanAccessDataForOriginCrashKeys() is called in the
  // CanAccessDataForOrigin() call above. The code below overrides the origin
  // crash key set in that call with data from |origin| because it provides
  // more accurate information than the origin derived from |url_to_check|.
  auto* requested_origin_key = GetRequestedOriginCrashKey();
  base::debug::SetCrashKeyString(requested_origin_key, origin.GetDebugString());
  return false;
}

bool ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(int child_id,
                                                            const GURL& url) {
  DCHECK(IsRunningOnExpectedThread());
  base::AutoLock lock(lock_);

  SecurityState* security_state = GetSecurityState(child_id);
  BrowserOrResourceContext browser_or_resource_context;
  if (security_state)
    browser_or_resource_context = security_state->GetBrowserOrResourceContext();

  GURL expected_process_lock;
  std::string failure_reason;

  if (!security_state) {
    failure_reason = "no_security_state";
  } else if (!browser_or_resource_context) {
    failure_reason = "no_browser_or_resource_context";
  } else {
    IsolationContext isolation_context(
        security_state->lowest_browsing_instance_id(),
        browser_or_resource_context);
    expected_process_lock =
        SiteInstanceImpl::DetermineProcessLockURL(isolation_context, url);

    GURL actual_process_lock = security_state->origin_lock();
    if (!actual_process_lock.is_empty()) {
      // Jail-style enforcement - a process with a lock can only access data
      // from origins that require exactly the same lock.
      if (actual_process_lock == expected_process_lock)
        return true;
      failure_reason = "lock_mismatch";
    } else {
      // Citadel-style enforcement - an unlocked process should not be able to
      // access data from origins that require a lock.
#if !defined(OS_ANDROID)
      // TODO(lukasza): https://crbug.com/566091: Once remote NTP is capable of
      // embedding OOPIFs, start enforcing citadel-style checks on desktop
      // platforms.
      // TODO(lukasza): https://crbug.com/614463: Enforce isolation within
      // GuestView (once OOPIFs are supported within GuestView).
      return true;
#else
      // TODO(acolwell, lukasza): https://crbug.com/764958: Make it possible to
      // call ShouldLockToOrigin (and GetSiteForURL?) on the IO thread.
      if (BrowserThread::CurrentlyOn(BrowserThread::IO))
        return true;
      DCHECK_CURRENTLY_ON(BrowserThread::UI);

      // TODO(lukasza): Consider making the checks below IO-thread-friendly, by
      // storing |is_unused| inside SecurityState.
      RenderProcessHost* process = RenderProcessHostImpl::FromID(child_id);
      if (process) {  // |process| can be null in unittests
        // Unlocked process can be legitimately used when navigating from an
        // unused process (about:blank, NTP on Android) to an isolated origin.
        // See also https://crbug.com/945399.  Returning |true| below will allow
        // such navigations to succeed (i.e. pass CanCommitOriginAndUrl checks).
        // We don't expect unused processes to be used outside of navigations
        // (e.g. when checking CanAccessDataForOrigin for localStorage, etc.).
        if (process->IsUnused())
          return true;
      }

      // TODO(alexmos, lukasza): https://crbug.com/764958: Consider making
      // ShouldLockToOrigin work with |expected_process_lock| instead of
      // |site_url|.
      GURL site_url = SiteInstanceImpl::GetSiteForURL(isolation_context, url);

      // A process with no lock can only access data from origins that do not
      // require a locked process.
      bool should_lock_target =
          SiteInstanceImpl::ShouldLockToOrigin(isolation_context, site_url);
      if (!should_lock_target)
        return true;
      failure_reason = " citadel_enforcement";
#endif
    }
  }

  // Returning false here will result in a renderer kill.  Set some crash
  // keys that will help understand the circumstances of that kill.
  LogCanAccessDataForOriginCrashKeys(
      expected_process_lock.possibly_invalid_spec(),
      GetKilledProcessOriginLock(security_state), url.GetOrigin().spec(),
      failure_reason);
  return false;
}

void ChildProcessSecurityPolicyImpl::IncludeIsolationContext(
    int child_id,
    const IsolationContext& isolation_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  auto* state = GetSecurityState(child_id);
  DCHECK(state);
  state->SetLowestBrowsingInstanceId(isolation_context.browsing_instance_id());
}

void ChildProcessSecurityPolicyImpl::LockToOrigin(
    const IsolationContext& context,
    int child_id,
    const GURL& gurl) {
  // LockToOrigin should only be called on the UI thread (OTOH, it is okay to
  // call GetOriginLock from any thread).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if DCHECK_IS_ON()
  // Sanity-check that the |gurl| argument can be used as a lock.
  RenderProcessHost* rph = RenderProcessHostImpl::FromID(child_id);
  if (rph)  // |rph| can be null in unittests.
    DCHECK_EQ(SiteInstanceImpl::DetermineProcessLockURL(context, gurl), gurl);
#endif

  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  DCHECK(state != security_state_.end());
  state->second->LockToOrigin(gurl, context.browsing_instance_id());
}

GURL ChildProcessSecurityPolicyImpl::GetOriginLock(int child_id) {
  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return GURL();
  return state->second->origin_lock();
}

void ChildProcessSecurityPolicyImpl::GrantPermissionsForFileSystem(
    int child_id,
    const std::string& filesystem_id,
    int permission) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;
  state->second->GrantPermissionsForFileSystem(filesystem_id, permission);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystem(
    int child_id,
    const std::string& filesystem_id,
    int permission) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFileSystem(filesystem_id, permission);
}

void ChildProcessSecurityPolicyImpl::RegisterFileSystemPermissionPolicy(
    storage::FileSystemType type,
    int policy) {
  base::AutoLock lock(lock_);
  file_system_policy_map_[type] = policy;
}

bool ChildProcessSecurityPolicyImpl::CanSendMidiSysExMessage(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->can_send_midi_sysex();
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOrigins(
    const std::vector<url::Origin>& origins_to_add,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  std::vector<IsolatedOriginPattern> patterns;
  patterns.reserve(origins_to_add.size());
  std::transform(origins_to_add.cbegin(), origins_to_add.cend(),
                 std::back_inserter(patterns),
                 [](const url::Origin& o) -> IsolatedOriginPattern {
                   return IsolatedOriginPattern(o);
                 });
  AddIsolatedOrigins(patterns, source, browser_context);
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOrigins(
    base::StringPiece origins_to_add,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  std::vector<IsolatedOriginPattern> patterns =
      ParseIsolatedOrigins(origins_to_add);
  AddIsolatedOrigins(patterns, source, browser_context);
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOrigins(
    const std::vector<IsolatedOriginPattern>& patterns,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  // This can only be called from the UI thread, as it reads state that's only
  // available (and is only safe to be retrieved) on the UI thread, such as
  // BrowsingInstance IDs.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsolatedOriginSource::COMMAND_LINE == source) {
    size_t number_of_origins = std::count_if(
        patterns.cbegin(), patterns.cend(),
        [](const IsolatedOriginPattern& p) { return p.is_valid(); });
    UMA_HISTOGRAM_COUNTS_1000("SiteIsolation.IsolateOrigins.Size",
                              number_of_origins);
  }

  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  for (const IsolatedOriginPattern& pattern : patterns) {
    if (!pattern.is_valid()) {
      LOG(ERROR) << "Invalid isolated origin: " << pattern.pattern();
      continue;
    }

    url::Origin origin_to_add = pattern.origin();

    // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
    // up the isolated origin lookup.  This only performs a straightforward
    // translation of an origin to eTLD+1; it does *not* take into account
    // effective URLs, isolated origins, and other logic that's not needed
    // here, but *is* typically needed for making process model decisions. Be
    // very careful about using GetSiteForOrigin() elsewhere, and consider
    // whether you should be using GetSiteForURL() instead.
    GURL key(SiteInstanceImpl::GetSiteForOrigin(origin_to_add));

    // Isolated origins should apply only to future BrowsingInstances and
    // processes.  Save the first BrowsingInstance ID to which they should
    // apply along with the actual origin.
    BrowsingInstanceId min_browsing_instance_id =
        SiteInstanceImpl::NextBrowsingInstanceId();

    // Check if the origin to be added already exists, in which case it may not
    // need to be added again.
    bool should_add = true;
    for (const auto& entry : isolated_origins_[key]) {
      if (entry.origin() != origin_to_add)
        continue;

      // If the added origin already exists for the same BrowserContext, don't
      // re-add it. Note that in this case, it must necessarily have a
      // lower/same BrowsingInstance ID: it's impossible for it to be
      // isolated with a higher ID, since NextBrowsingInstanceId() returns
      // monotonically increasing IDs.
      if (entry.browser_context() == browser_context) {
        DCHECK_LE(entry.min_browsing_instance_id(), min_browsing_instance_id);
        should_add = false;
        break;
      }

      // Otherwise, allow the origin to be added again for a different profile
      // (or globally for all profiles), possibly with a different
      // BrowsingInstance ID cutoff.  Note that a particular origin might have
      // multiple entries, each one for a different profile, so we must loop
      // over all such existing entries before concluding that |origin| really
      // needs to be added.
    }

    if (should_add) {
      ResourceContext* resource_context =
          browser_context ? browser_context->GetResourceContext() : nullptr;
      IsolatedOriginEntry entry(
          std::move(origin_to_add), min_browsing_instance_id, browser_context,
          resource_context, pattern.isolate_all_subdomains(), source);
      isolated_origins_[key].emplace_back(std::move(entry));
    }
  }
}

void ChildProcessSecurityPolicyImpl::RemoveIsolatedOriginsForBrowserContext(
    const BrowserContext& browser_context) {
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  for (auto& iter : isolated_origins_) {
    base::EraseIf(iter.second,
                  [&browser_context](const IsolatedOriginEntry& entry) {
                    // Remove if BrowserContext matches.
                    return (entry.browser_context() == &browser_context);
                  });
  }

  // Also remove map entries for site URLs which no longer have any
  // IsolatedOriginEntries remaining.
  base::EraseIf(isolated_origins_,
                [](const auto& pair) { return pair.second.empty(); });
}

bool ChildProcessSecurityPolicyImpl::IsIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin) {
  url::Origin unused_result;
  return GetMatchingIsolatedOrigin(isolation_context, origin, &unused_result);
}

bool ChildProcessSecurityPolicyImpl::IsGloballyIsolatedOriginForTesting(
    const url::Origin& origin) {
  BrowserOrResourceContext no_browser_context;
  BrowsingInstanceId null_browsing_instance_id;
  IsolationContext isolation_context(null_browsing_instance_id,
                                     no_browser_context);
  return IsIsolatedOrigin(isolation_context, origin);
}

std::vector<url::Origin> ChildProcessSecurityPolicyImpl::GetIsolatedOrigins(
    base::Optional<IsolatedOriginSource> source,
    BrowserContext* browser_context) {
  std::vector<url::Origin> origins;
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);
  for (const auto& iter : isolated_origins_) {
    for (const auto& isolated_origin_entry : iter.second) {
      if (source && source.value() != isolated_origin_entry.source())
        continue;

      // If browser_context is specified, ensure that the entry matches it.  If
      // the browser_context is not specified, only consider entries that are
      // not associated with a profile (i.e., which apply globally to the
      // entire browser).
      bool matches_profile =
          browser_context ? isolated_origin_entry.MatchesProfile(
                                BrowserOrResourceContext(browser_context))
                          : isolated_origin_entry.AppliesToAllBrowserContexts();
      if (!matches_profile)
        continue;

      origins.push_back(isolated_origin_entry.origin());
    }
  }
  return origins;
}

bool ChildProcessSecurityPolicyImpl::GetMatchingIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    url::Origin* result) {
  // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
  // up the isolated origin lookup.  This only performs a straightforward
  // translation of an origin to eTLD+1; it does *not* take into account
  // effective URLs, isolated origins, and other logic that's not needed
  // here, but *is* typically needed for making process model decisions. Be
  // very careful about using GetSiteForOrigin() elsewhere, and consider
  // whether you should be using GetSiteForURL() instead.
  return GetMatchingIsolatedOrigin(isolation_context, origin,
                                   SiteInstanceImpl::GetSiteForOrigin(origin),
                                   result);
}

bool ChildProcessSecurityPolicyImpl::GetMatchingIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    const GURL& site_url,
    url::Origin* result) {
  DCHECK(IsRunningOnExpectedThread());

  *result = url::Origin();
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  // If |isolation_context| does not specify a BrowsingInstance ID, then assume
  // that we want to retrieve the latest applicable information; i.e., return
  // the latest matching isolated origins that would apply to future
  // BrowsingInstances.  Using NextBrowsingInstanceId() will match all
  // available IsolatedOriginEntries.
  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());
  if (browsing_instance_id.is_null())
    browsing_instance_id = SiteInstanceImpl::NextBrowsingInstanceId();

  // Look up the list of origins corresponding to |origin|'s site.
  auto it = isolated_origins_.find(site_url);

  // Subtle corner case: if the site's host ends with a dot, do the lookup
  // without it.  A trailing dot shouldn't be able to bypass isolated origins:
  // if "https://foo.com" is an isolated origin, "https://foo.com." should
  // match it.
  if (it == isolated_origins_.end() && site_url.has_host() &&
      site_url.host_piece().back() == '.') {
    GURL::Replacements replacements;
    base::StringPiece host(site_url.host_piece());
    host.remove_suffix(1);
    replacements.SetHostStr(host);
    it = isolated_origins_.find(site_url.ReplaceComponents(replacements));
  }

  // Looks for all isolated origins that were already isolated at the time
  // |isolation_context| was created. If multiple isolated origins are
  // registered with a common domain suffix, return the most specific one.  For
  // example, if foo.isolated.com and isolated.com are both isolated origins,
  // bar.foo.isolated.com should return foo.isolated.com.
  bool found = false;
  if (it != isolated_origins_.end()) {
    for (const auto& isolated_origin_entry : it->second) {
      // If this isolated origin applies only to a specific profile, don't
      // use it for a different profile.
      if (!isolated_origin_entry.MatchesProfile(
              isolation_context.browser_or_resource_context()))
        continue;

      bool matches_browsing_instance_id =
          isolated_origin_entry.min_browsing_instance_id() <=
          browsing_instance_id;
      if (matches_browsing_instance_id &&
          IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(
              origin, isolated_origin_entry.origin())) {
        // If a match has been found that requires all subdomains to be isolated
        // then return immediately. |origin| is returned to ensure proper
        // process isolation, e.g. https://a.b.c.isolated.com matches an
        // IsolatedOriginEntry constructed from http://[*.]isolated.com, so
        // https://a.b.c.isolated.com must be returned.
        if (isolated_origin_entry.isolate_all_subdomains()) {
          *result = origin;
          uint16_t default_port = url::DefaultPortForScheme(
              origin.scheme().data(), origin.scheme().length());

          if (origin.port() != default_port) {
            *result = url::Origin::Create(GURL(origin.scheme() +
                                               url::kStandardSchemeSeparator +
                                               origin.host()));
          }

          return true;
        }

        if (!found || result->host().length() <
                          isolated_origin_entry.origin().host().length()) {
          *result = isolated_origin_entry.origin();
          found = true;
        }
      }
    }
  }

  return found;
}

void ChildProcessSecurityPolicyImpl::RemoveIsolatedOriginForTesting(
    const url::Origin& origin) {
  GURL key(SiteInstanceImpl::GetSiteForOrigin(origin));
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);
  base::EraseIf(isolated_origins_[key],
                [&origin](const IsolatedOriginEntry& entry) {
                  // Remove if origin matches.
                  return (entry.origin() == origin);
                });
  if (isolated_origins_[key].empty())
    isolated_origins_.erase(key);
}

bool ChildProcessSecurityPolicyImpl::HasSecurityState(int child_id) {
  base::AutoLock lock(lock_);
  return GetSecurityState(child_id) != nullptr;
}

ChildProcessSecurityPolicyImpl::SecurityState*
ChildProcessSecurityPolicyImpl::GetSecurityState(int child_id) {
  auto itr = security_state_.find(child_id);
  if (itr != security_state_.end())
    return itr->second.get();

  // Check to see if |child_id| is in the pending removal map since this
  // may be a call that was already on the IO thread's task queue when the
  // Remove() call occurred.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    itr = pending_remove_state_.find(child_id);
    if (itr != pending_remove_state_.end())
      return itr->second.get();
  }

  return nullptr;
}

std::vector<IsolatedOriginPattern>
ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
    base::StringPiece pattern_list) {
  std::vector<base::StringPiece> origin_strings = base::SplitStringPiece(
      pattern_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<IsolatedOriginPattern> patterns;
  patterns.reserve(origin_strings.size());

  for (const base::StringPiece& origin_string : origin_strings)
    patterns.emplace_back(origin_string);

  return patterns;
}

// static
std::string ChildProcessSecurityPolicyImpl::GetKilledProcessOriginLock(
    const SecurityState* security_state) {
  std::string killed_process_origin_lock;
  if (!security_state)
    return "(child id not found)";

  if (!security_state->GetBrowserOrResourceContext())
    return "(context is null)";

  if (security_state->origin_lock().is_empty())
    return "(none)";

  return security_state->origin_lock().possibly_invalid_spec();
}

void ChildProcessSecurityPolicyImpl::LogKilledProcessOriginLock(int child_id) {
  base::AutoLock lock(lock_);
  const auto itr = security_state_.find(child_id);
  const SecurityState* security_state =
      itr != security_state_.end() ? itr->second.get() : nullptr;

  base::debug::SetCrashKeyString(GetKilledProcessOriginLockKey(),
                                 GetKilledProcessOriginLock(security_state));
}

}  // namespace content
