// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"

#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/url_info.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
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
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace features {

// TODO(https://crbug.com/324934416): Remove this killswitch once the new
// CanCommitURL restrictions finish rolling out.
BASE_FEATURE(kAdditionalNavigationCommitChecks,
             "AdditionalNavigationCommitChecks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(https://crbug.com/325410297): Remove this killswitch once the new
// sandboxed frame enforcements finish rolling out.
BASE_FEATURE(kSandboxedFrameEnforcements,
             "SandboxedFrameEnforcements",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

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

base::debug::CrashKeyString* GetExpectedProcessLockKey() {
  static auto* expected_process_lock_key = base::debug::AllocateCrashKeyString(
      "expected_process_lock", base::debug::CrashKeySize::Size64);
  return expected_process_lock_key;
}

base::debug::CrashKeyString* GetKilledProcessOriginLockKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "killed_process_origin_lock", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetCanAccessDataFailureReasonKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "can_access_data_failure_reason", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetCanAccessDataKeepAliveDurationKey() {
  static auto* keep_alive_duration_key = base::debug::AllocateCrashKeyString(
      "keep_alive_duration", base::debug::CrashKeySize::Size256);
  return keep_alive_duration_key;
}

base::debug::CrashKeyString* GetCanAccessDataShutdownDelayRefCountKey() {
  static auto* shutdown_delay_key = base::debug::AllocateCrashKeyString(
      "shutdown_delay_ref_count", base::debug::CrashKeySize::Size32);
  return shutdown_delay_key;
}

base::debug::CrashKeyString* GetCanAccessDataProcessRFHCount() {
  static auto* process_rfh_count_key = base::debug::AllocateCrashKeyString(
      "process_rfh_count", base::debug::CrashKeySize::Size32);
  return process_rfh_count_key;
}

void LogCanAccessDataForOriginCrashKeys(
    const std::string& expected_process_lock,
    const std::string& killed_process_origin_lock,
    const std::string& requested_origin,
    const std::string& failure_reason,
    const std::string& keep_alive_durations,
    const std::string& shutdown_delay_ref_count,
    const std::string& process_rfh_count) {
  base::debug::SetCrashKeyString(GetExpectedProcessLockKey(),
                                 expected_process_lock);
  base::debug::SetCrashKeyString(GetKilledProcessOriginLockKey(),
                                 killed_process_origin_lock);
  base::debug::SetCrashKeyString(GetRequestedOriginCrashKey(),
                                 requested_origin);
  base::debug::SetCrashKeyString(GetCanAccessDataFailureReasonKey(),
                                 failure_reason);
  base::debug::SetCrashKeyString(GetCanAccessDataKeepAliveDurationKey(),
                                 keep_alive_durations);
  base::debug::SetCrashKeyString(GetCanAccessDataShutdownDelayRefCountKey(),
                                 shutdown_delay_ref_count);
  base::debug::SetCrashKeyString(GetCanAccessDataProcessRFHCount(),
                                 process_rfh_count);
}

void LogCanCommitUrlFailureReason(const std::string& failure_reason) {
  static auto* const failure_reason_key = base::debug::AllocateCrashKeyString(
      "cpspi_can_commit_url_failure_reason", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(failure_reason_key, failure_reason);
}

// Checks whether a lock mismatch should be ignored to allow most visited tiles
// to commit in third-party NTP processes.
//
// TODO(crbug.com/40447789): This exception should be removed once these tiles
// can be loaded in OOPIFs on the NTP.
bool AllowProcessLockMismatchForNTP(const ProcessLock& expected_lock,
                                    const ProcessLock& actual_lock) {
  // First, ensure that the expected lock corresponds to a WebUI site that
  // does not require its process to be locked.  This should only be the case
  // for sites used to load most visited tiles.
  const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
  if (!base::Contains(webui_schemes, expected_lock.lock_url().scheme())) {
    return false;
  }
  if (GetContentClient()->browser()->DoesWebUIUrlRequireProcessLock(
          expected_lock.lock_url())) {
    return false;
  }

  // Now, check that the actual lock corresponds to an NTP process (using its
  // site_url() since this check relies on checking effective URLs for NTPs),
  // and that the expected lock (based on the URL for which we're doing the
  // access check) is allowed to stay in that process. This restricts the lock
  // mismatch to just NTP processes, disallowing most visited tiles from being
  // embedded on sites in other processes.
  return GetContentClient()->browser()->ShouldStayInParentProcessForNTP(
      expected_lock.lock_url(), actual_lock.site_url());
}

base::WeakPtr<ResourceContext> GetResourceContext(
    BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();
  return resource_context ? resource_context->GetWeakPtr() : nullptr;
}

}  // namespace

ChildProcessSecurityPolicyImpl::Handle::Handle()
    : child_id_(ChildProcessHost::kInvalidUniqueID) {}

ChildProcessSecurityPolicyImpl::Handle::Handle(int child_id,
                                               bool duplicating_handle)
    : child_id_(child_id) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->AddProcessReference(child_id_, duplicating_handle))
    child_id_ = ChildProcessHost::kInvalidUniqueID;
}

ChildProcessSecurityPolicyImpl::Handle::Handle(Handle&& rhs)
    : child_id_(rhs.child_id_) {
  rhs.child_id_ = ChildProcessHost::kInvalidUniqueID;
}

ChildProcessSecurityPolicyImpl::Handle
ChildProcessSecurityPolicyImpl::Handle::Duplicate() {
  return Handle(child_id_, /* duplicating_handle */ true);
}

ChildProcessSecurityPolicyImpl::Handle::~Handle() {
  if (child_id_ != ChildProcessHost::kInvalidUniqueID) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    policy->RemoveProcessReference(child_id_);
  }
}

ChildProcessSecurityPolicyImpl::Handle& ChildProcessSecurityPolicyImpl::Handle::
operator=(Handle&& rhs) {
  if (child_id_ != ChildProcessHost::kInvalidUniqueID &&
      child_id_ != rhs.child_id_) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    policy->RemoveProcessReference(child_id_);
  }
  child_id_ = rhs.child_id_;
  rhs.child_id_ = ChildProcessHost::kInvalidUniqueID;
  return *this;
}

bool ChildProcessSecurityPolicyImpl::Handle::is_valid() const {
  return child_id_ != ChildProcessHost::kInvalidUniqueID;
}

bool ChildProcessSecurityPolicyImpl::Handle::CanReadFile(
    const base::FilePath& file) {
  if (child_id_ == ChildProcessHost::kInvalidUniqueID)
    return false;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanReadFile(child_id_, file);
}

bool ChildProcessSecurityPolicyImpl::Handle::CanReadFileSystemFile(
    const storage::FileSystemURL& url) {
  if (child_id_ == ChildProcessHost::kInvalidUniqueID)
    return false;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanReadFileSystemFile(child_id_, url);
}

bool ChildProcessSecurityPolicyImpl::Handle::CanAccessDataForOrigin(
    const url::Origin& origin) {
  if (child_id_ == ChildProcessHost::kInvalidUniqueID) {
    LogCanAccessDataForOriginCrashKeys(
        "(unknown)", "(unknown)", origin.GetDebugString(), "handle_not_valid",
        "no_keep_alive_durations", "no shutdown delay ref count",
        "no process rfh count");
    return false;
  }

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanAccessDataForOrigin(child_id_, origin);
}

ChildProcessSecurityPolicyImpl::OriginAgentClusterOptInEntry::
    OriginAgentClusterOptInEntry(
        const OriginAgentClusterIsolationState& oac_isolation_state_in,
        const url::Origin& origin_in)
    : oac_isolation_state(oac_isolation_state_in), origin(origin_in) {}

ChildProcessSecurityPolicyImpl::OriginAgentClusterOptInEntry::
    OriginAgentClusterOptInEntry(const OriginAgentClusterOptInEntry&) = default;

ChildProcessSecurityPolicyImpl::OriginAgentClusterOptInEntry::
    ~OriginAgentClusterOptInEntry() = default;

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicyImpl::SecurityState {
 public:
  typedef std::map<BrowsingInstanceId, OriginAgentClusterIsolationState>
      BrowsingInstanceDefaultIsolationStatesMap;

  explicit SecurityState(BrowserContext* browser_context)
      : can_read_raw_cookies_(false),
        can_send_midi_(false),
        can_send_midi_sysex_(false),
        browser_context_(browser_context),
        resource_context_(GetResourceContext(browser_context)) {
    if (!base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
      can_send_midi_ = true;
    }
  }

  SecurityState(const SecurityState&) = delete;
  SecurityState& operator=(const SecurityState&) = delete;

  ~SecurityState() {
    storage::IsolatedContext* isolated_context =
        storage::IsolatedContext::GetInstance();
    for (auto iter = filesystem_permissions_.begin();
         iter != filesystem_permissions_.end(); ++iter) {
      isolated_context->RemoveReference(iter->first);
    }
    UMA_HISTOGRAM_COUNTS_10000(
        "SiteIsolation.BrowsingInstance.MaxCountPerProcess",
        max_browsing_instance_count_);
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

#if BUILDFLAG(IS_ANDROID)
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

  void GrantBindings(BindingsPolicySet bindings) {
    enabled_bindings_.PutAll(bindings);
  }

  void GrantReadRawCookies() {
    can_read_raw_cookies_ = true;
  }

  void RevokeReadRawCookies() {
    can_read_raw_cookies_ = false;
  }

  void GrantOriginCheckExemptionForWebView(const url::Origin& origin) {
    // This should only be allowed for opaque origins with LoadDataWithBaseURL
    // and file origins with allow_universal_access_from_file_urls.
    CHECK(origin.opaque() || origin.scheme() == url::kFileScheme);
    webview_origin_exemption_set_.insert(origin);
  }

  bool HasOriginCheckExemptionForWebView(const url::Origin& origin) {
    // This should only be allowed for opaque origins with LoadDataWithBaseURL
    // and file origins with allow_universal_access_from_file_urls.
    CHECK(origin.opaque() || origin.scheme() == url::kFileScheme);
    return base::Contains(webview_origin_exemption_set_, origin);
  }

  void GrantPermissionForMidi() { can_send_midi_ = true; }

  void GrantPermissionForMidiSysEx() {
    can_send_midi_ = true;
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

    // file:// URLs may sometimes be more granular, e.g. dragging and dropping a
    // file from the local filesystem. The child itself may not have been
    // granted access to the entire file:// scheme, but it should still be
    // allowed to request the dragged and dropped file.
    if (url.SchemeIs(url::kFileScheme)) {
      base::FilePath path;
      if (net::FileURLToFilePath(url, &path)) {
        return base::Contains(request_file_set_, path);
      }
    }

#if BUILDFLAG(IS_ANDROID)
    if (url.SchemeIs(url::kContentScheme)) {
      return base::Contains(request_file_set_, base::FilePath(url.spec()));
    }
#endif

    // Otherwise, delegate to CanCommitURL. Unmentioned schemes are disallowed.
    // TODO(dcheng): It would be nice to avoid constructing the origin twice.
    return CanCommitURL(url);
  }

  // Determine if the certain permissions have been granted to a file.
  bool HasPermissionsForFile(const base::FilePath& file, int permissions) {
#if BUILDFLAG(IS_ANDROID)
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

  void SetProcessLock(const ProcessLock& lock_to_set,
                      const IsolationContext& context,
                      bool is_process_used) {
    CHECK(!lock_to_set.is_invalid());
    CHECK(!process_lock_.is_locked_to_site());
    CHECK_NE(SiteInstanceImpl::GetDefaultSiteURL(), lock_to_set.lock_url());

    if (process_lock_.is_invalid()) {
      DCHECK(browsing_instance_default_isolation_states_.empty());
      CHECK(lock_to_set.allows_any_site() || lock_to_set.is_locked_to_site());
    } else {
      // Verify that we are not trying to update the lock with different
      // COOP/COEP information.
      CHECK_EQ(process_lock_.GetWebExposedIsolationInfo(),
               lock_to_set.GetWebExposedIsolationInfo());

      if (process_lock_.allows_any_site()) {
        // TODO(acolwell): Remove ability to lock to an allows_any_site
        // lock multiple times. Legacy behavior allows the old "lock to site"
        // path to generate an "allow_any_site" lock if an empty URL is passed
        // to SiteInstanceImpl::SetSite().
        CHECK(lock_to_set.allows_any_site() || lock_to_set.is_locked_to_site());

        // Do not allow a lock to become more strict if the process has already
        // been used to render any pages.
        if (lock_to_set.is_locked_to_site()) {
          CHECK(!is_process_used)
              << "Cannot lock an already used process to " << lock_to_set;
        }
      } else {
        NOTREACHED_IN_MIGRATION() << "Unexpected lock type.";
      }
    }

    process_lock_ = lock_to_set;
    AddBrowsingInstanceInfo(context);
  }

  void AddBrowsingInstanceInfo(const IsolationContext& context) {
    DCHECK(!context.browsing_instance_id().is_null());
    browsing_instance_default_isolation_states_.insert(
        {context.browsing_instance_id(), context.default_isolation_state()});

    // Track the maximum number of BrowsingInstances in the process in case
    // we need to remove delayed cleanup and let the set grow unbounded.
    // Also track the default isolation state for this BrowsingInstance for
    // future access checks, since the global default can change over time.
    if (browsing_instance_default_isolation_states_.size() >
        max_browsing_instance_count_) {
      max_browsing_instance_count_ =
          browsing_instance_default_isolation_states_.size();
    }
  }

  const ProcessLock& process_lock() const { return process_lock_; }

  const BrowsingInstanceDefaultIsolationStatesMap&
  browsing_instance_default_isolation_states() {
    return browsing_instance_default_isolation_states_;
  }

  void ClearBrowsingInstanceId(const BrowsingInstanceId& id) {
    browsing_instance_default_isolation_states_.erase(id);
  }

  bool has_web_ui_bindings() const {
    return enabled_bindings_.HasAny(kWebUIBindingsPolicySet);
  }

  bool can_read_raw_cookies() const {
    return can_read_raw_cookies_;
  }

  bool CanSendMidi() const {
    if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
      // Ensure the flags are in a consistent state: we can only send SysEx
      // messages if we can also send non-SysEx messages
      CHECK(can_send_midi_ || !can_send_midi_sysex_);
      return can_send_midi_;
    } else {
      return true;
    }
  }

  bool CanSendMidiSysEx() const {
    if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
      // Ensure the flags are in a consistent state: we can only send SysEx
      // messages if we can also send non-SysEx messages
      CHECK(can_send_midi_ || !can_send_midi_sysex_);
    }
    return can_send_midi_sysex_;
  }

  BrowserOrResourceContext GetBrowserOrResourceContext() const {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI) && browser_context_)
      return BrowserOrResourceContext(browser_context_);

    if (BrowserThread::CurrentlyOn(BrowserThread::IO) && resource_context_)
      return BrowserOrResourceContext(resource_context_.get());

    return BrowserOrResourceContext();
  }

  void ClearBrowserContextIfMatches(const BrowserContext* browser_context) {
    if (browser_context == browser_context_)
      browser_context_ = nullptr;
  }

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
    return base::Contains(origin_map_, origin);
  }

  typedef std::map<std::string, CommitRequestPolicy> SchemeMap;
  typedef std::map<url::Origin, CommitRequestPolicy> OriginMap;

  typedef int FilePermissionFlags;  // bit-set of base::File::Flags
  typedef std::map<base::FilePath, FilePermissionFlags> FileMap;
  typedef std::map<std::string, FilePermissionFlags> FileSystemMap;
  typedef std::set<base::FilePath> FileSet;
  typedef std::set<url::Origin> OriginSet;

  // Maps URL schemes to commit/request policies the child process has been
  // granted. There is no provision for revoking.
  SchemeMap scheme_map_;

  // The map of URL origins to commit/request policies the child process has
  // been granted. There is no provision for revoking.
  OriginMap origin_map_;

  // The set of files the child process is permitted to upload to the web.
  FileMap file_permissions_;

  // The set of files the child process is permitted to load.
  FileSet request_file_set_;

  // The set of origins in Android WebView and <webview> tags that are allowed
  // to bypass some navigation checks. Limited to opaque origins loaded with
  // LoadDataWithBaseURL and file origins loaded with
  // allow_universal_access_from_file_urls.
  OriginSet webview_origin_exemption_set_;

  BindingsPolicySet enabled_bindings_;

  bool can_read_raw_cookies_;

  bool can_send_midi_;

  bool can_send_midi_sysex_;

  ProcessLock process_lock_;

  // A map containing the IDs of all BrowsingInstances with documents in this
  // process, along with their default OriginAgentClusterIsolationStates. Empty
  // when |process_lock_| is invalid, or if all BrowsingInstances in the
  // SecurityState have been destroyed.
  //
  // After a process is locked, it might be reused by navigations from frames
  // in other BrowsingInstances, e.g., when we're over process limit and when
  // those navigations utilize the same process lock. This set tracks all the
  // BrowsingInstances that share this process.
  //
  // This is needed for security checks on the IO thread, where we only know
  // the process ID and need to compute the expected origin lock, which
  // requires knowing the set of applicable isolated origins in each respective
  // BrowsingInstance.
  BrowsingInstanceDefaultIsolationStatesMap
      browsing_instance_default_isolation_states_;

  // The maximum number of BrowsingInstances that have been in this
  // SecurityState's RenderProcessHost, for metrics.
  unsigned max_browsing_instance_count_ = 0;

  // The set of isolated filesystems the child process is permitted to access.
  FileSystemMap filesystem_permissions_;

  raw_ptr<BrowserContext> browser_context_;
  base::WeakPtr<ResourceContext> resource_context_;
};

// IsolatedOriginEntry implementation.
ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::IsolatedOriginEntry(
    const url::Origin& origin,
    bool applies_to_future_browsing_instances,
    BrowsingInstanceId browsing_instance_id,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    bool isolate_all_subdomains,
    IsolatedOriginSource source)
    : origin_(origin),
      applies_to_future_browsing_instances_(
          applies_to_future_browsing_instances),
      browsing_instance_id_(browsing_instance_id),
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
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool ChildProcessSecurityPolicyImpl::IsolatedOriginEntry::
    MatchesBrowsingInstance(BrowsingInstanceId browsing_instance_id) const {
  if (applies_to_future_browsing_instances_)
    return browsing_instance_id_ <= browsing_instance_id;

  return browsing_instance_id_ == browsing_instance_id;
}

// Make sure BrowsingInstance state is cleaned up after the max amount of time
// RenderProcessHost might stick around for various IncrementKeepAliveRefCount
// calls. For now, track that as the KeepAliveHandleFactory timeout (the current
// longest value) plus the unload timeout, with a bit of an extra margin.
// // TODO(wjmaclean): Refactor IncrementKeepAliveRefCount to track how much
// time is needed rather than leaving the interval open ended, so that we can
// enforce a max delay here and in RenderProcessHost. https://crbug.com/1181838
ChildProcessSecurityPolicyImpl::ChildProcessSecurityPolicyImpl()
    : browsing_instance_cleanup_delay_(
          RenderProcessHostImpl::kKeepAliveHandleFactoryTimeout +
          base::Seconds(2)) {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(url::kHttpScheme);
  RegisterWebSafeScheme(url::kHttpsScheme);
#if BUILDFLAG(ENABLE_WEBSOCKETS)
  RegisterWebSafeScheme(url::kWsScheme);
  RegisterWebSafeScheme(url::kWssScheme);
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
  RegisterWebSafeScheme(url::kDataScheme);

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
  DCHECK_NE(child_id, ChildProcessHost::kInvalidUniqueID);
  base::AutoLock lock(lock_);
  if (base::Contains(security_state_, child_id)) {
    NOTREACHED_IN_MIGRATION() << "Add child process at most once.";
    return;
  }

  security_state_[child_id] = std::make_unique<SecurityState>(browser_context);
  CHECK(AddProcessReferenceLocked(child_id, /* duplicating_handle */ false));
}

void ChildProcessSecurityPolicyImpl::AddForTesting(
    int child_id,
    BrowserContext* browser_context) {
  Add(child_id, browser_context);
  LockProcess(IsolationContext(
                  BrowsingInstanceId(1), browser_context,
                  /*is_guest=*/false, /*is_fenced=*/false,
                  OriginAgentClusterIsolationState::CreateForDefaultIsolation(
                      browser_context)),
              child_id, /*is_process_used=*/false,
              ProcessLock::CreateAllowAnySite(
                  StoragePartitionConfig::CreateDefault(browser_context),
                  WebExposedIsolationInfo::CreateNonIsolated()));
}

void ChildProcessSecurityPolicyImpl::Remove(int child_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(child_id, ChildProcessHost::kInvalidUniqueID);
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  // Moving the existing SecurityState object into a pending map so
  // that we can preserve permission state and avoid mutations to this
  // state after Remove() has been called.
  pending_remove_state_[child_id] = std::move(state->second);
  security_state_.erase(child_id);

  RemoveProcessReferenceLocked(child_id);
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(schemes_lock_);
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
  base::AutoLock lock(schemes_lock_);
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
  base::AutoLock lock(schemes_lock_);

  return base::Contains(schemes_okay_to_request_in_any_process_, scheme);
}

void ChildProcessSecurityPolicyImpl::RegisterPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(schemes_lock_);
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme)) << "Add schemes at most once.";
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";
  DCHECK_EQ(0U, schemes_okay_to_commit_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(schemes_lock_);

  return base::Contains(pseudo_schemes_, scheme);
}

void ChildProcessSecurityPolicyImpl::ClearRegisteredSchemeForTesting(
    const std::string& scheme) {
  base::AutoLock lock(schemes_lock_);
  schemes_okay_to_request_in_any_process_.erase(scheme);
  schemes_okay_to_commit_in_any_process_.erase(scheme);
  pseudo_schemes_.erase(scheme);
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

void ChildProcessSecurityPolicyImpl::GrantRequestOfSpecificFile(
    int child_id,
    const base::FilePath& path) {
  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  if (state == security_state_.end()) {
    return;
  }

  // When the child process has been commanded to request a file:// URL,
  // then we grant it the capability for that URL only.
  state->second->GrantRequestOfSpecificFile(path);
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

void ChildProcessSecurityPolicyImpl::GrantSendMidiMessage(int child_id) {
  if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
    base::AutoLock lock(lock_);

    auto state = security_state_.find(child_id);
    if (state == security_state_.end()) {
      return;
    }

    state->second->GrantPermissionForMidi();
  }
  return;
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

void ChildProcessSecurityPolicyImpl::GrantWebUIBindings(
    int child_id,
    BindingsPolicySet bindings) {
  // Only WebUI bindings should come through here.
  CHECK(bindings.HasAny(kWebUIBindingsPolicySet));
  CHECK(Difference(bindings, kWebUIBindingsPolicySet).empty());

  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end()) {
    return;
  }

  state->second->GrantBindings(bindings);
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

void ChildProcessSecurityPolicyImpl::GrantOriginCheckExemptionForWebView(
    int child_id,
    const url::Origin& origin) {
  base::AutoLock lock(lock_);

  auto* state = GetSecurityState(child_id);
  if (!state) {
    return;
  }

  state->GrantOriginCheckExemptionForWebView(origin);
}

bool ChildProcessSecurityPolicyImpl::HasOriginCheckExemptionForWebView(
    int child_id,
    const url::Origin& origin) {
  base::AutoLock lock(lock_);

  auto* state = GetSecurityState(child_id);
  if (!state) {
    return false;
  }

  return state->HasOriginCheckExemptionForWebView(origin);
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

  // If |url| has WebUI scheme, the process must usually be locked, unless
  // running in single-process mode. Since this is a check whether the process
  // can request |url|, the check must operate based on scheme because one WebUI
  // should be able to request subresources from another WebUI of the same
  // scheme.
  const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
  if (!RenderProcessHost::run_renderer_in_process() &&
      base::Contains(webui_schemes, url.scheme())) {
    bool should_be_locked =
        GetContentClient()->browser()->DoesWebUIUrlRequireProcessLock(url);
    if (should_be_locked) {
      const ProcessLock lock = GetProcessLock(child_id);
      if (!lock.is_locked_to_site() || !lock.matches_scheme(url.scheme()))
        return false;
    }
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
  if (!url.is_valid()) {
    LogCanCommitUrlFailureReason("invalid_url");
    return false;  // Can't commit invalid URLs.
  }

  const std::string& scheme = url.scheme();

  // Of all the pseudo schemes, only about:blank and about:srcdoc are allowed to
  // commit.
  if (IsPseudoScheme(scheme)) {
    if (!url.IsAboutBlank() && !url.IsAboutSrcdoc()) {
      LogCanCommitUrlFailureReason("pseudo_scheme_non_blank_or_srcdoc");
      return false;
    } else {
      // TODO(crbug.com/324934416): Consider continuing with the checks below.
      return true;
    }
  }

  // Blob and filesystem URLs require special treatment; validate the inner
  // origin they embed.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url)) {
      LogCanCommitUrlFailureReason("malformed_blob_url");
      return false;
    }

    // No need to log a failure reason here, because it will be logged in the
    // sole recursive call if that call returns false.
    url::Origin origin = url::Origin::Create(url);
    return origin.opaque() || CanCommitURL(child_id, GURL(origin.Serialize()));
  }

  // Allow data URLs to commit in any process. Note that the precursor origin
  // should be checked separately.
  if (url.SchemeIs(url::kDataScheme)) {
    return true;
  }

  // With site isolation, a URL from a site may only be committed in a process
  // dedicated to that site.  This check will ensure that |url| can't commit if
  // the process is locked to a different site.
  //
  // We skip this check specifically for the error page URL,
  // chrome-error://chromewebdata, because it can commit in any process (due to
  // a lack of subframe error page isolation) and because it is difficult to
  // compute its expected process lock. We still verify in the
  // state->CanCommitURL call below that the process has actually been granted
  // access to this URL, rather than just returning true for it.
  if (url != GURL(kUnreachableWebDataURL) &&
      !CanAccessMaybeOpaqueOrigin(child_id, url,
                                  false /* url_is_precursor_of_opaque_origin */,
                                  AccessType::kCanCommitNewOrigin)) {
    LogCanCommitUrlFailureReason("cannot_access_origin");
    return false;
  }

  {
    base::AutoLock lock(lock_);

    // Most schemes can commit in any process. Note that we check
    // schemes_okay_to_commit_in_any_process_ here, which is stricter than
    // IsWebSafeScheme().
    //
    // TODO(creis, nick): https://crbug.com/515309: The line below does not
    // enforce that http pages cannot commit in an extension process.
    {
      base::AutoLock schemes_lock(schemes_lock_);
      if (base::Contains(schemes_okay_to_commit_in_any_process_, scheme)) {
        return true;
      }
    }

    auto* state = GetSecurityState(child_id);
    if (!state) {
      LogCanCommitUrlFailureReason("no_security_state_found");
      return false;
    }

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to commit the URL.
    bool can_commit = state->CanCommitURL(url);
    if (!can_commit) {
      LogCanCommitUrlFailureReason("cpsp_state_cannot_commit_url");
    }
    return can_commit;
  }
}

bool ChildProcessSecurityPolicyImpl::CanReadFile(int child_id,
                                                 const base::FilePath& file) {
  return HasPermissionsForFile(child_id, file, READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadAllFiles(
    int child_id,
    const std::vector<base::FilePath>& files) {
  return base::ranges::all_of(files,
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
      case network::DataElement::Tag::kFile:
        if (!CanReadFile(child_id,
                         element.As<network::DataElementFile>().path()))
          return false;
        break;

      case network::DataElement::Tag::kBytes:
        // Data is self-contained within |body| - no need to check access.
        break;

      case network::DataElement::Tag::kDataPipe:
        // Data is self-contained within |body| - no need to check access.
        break;

      default:
        // Fail safe - deny access.
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }
  return true;
}

bool ChildProcessSecurityPolicyImpl::CanReadRequestBody(
    RenderProcessHost* process,
    const scoped_refptr<network::ResourceRequestBody>& body) {
  CHECK(process);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return CanReadRequestBody(
      process->GetID(), process->GetStoragePartition()->GetFileSystemContext(),
      body);
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

bool ChildProcessSecurityPolicyImpl::CanMoveFileSystemFile(
    int child_id,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url) {
  return HasPermissionsForFileSystemFile(child_id, dest_url,
                                         CREATE_NEW_FILE_GRANT) &&
         HasPermissionsForFileSystemFile(child_id, src_url, READ_FILE_GRANT) &&
         HasPermissionsForFileSystemFile(child_id, src_url, DELETE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCopyFileSystemFile(
    int child_id,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url) {
  return HasPermissionsForFileSystemFile(child_id, src_url, READ_FILE_GRANT) &&
         HasPermissionsForFileSystemFile(child_id, dest_url,
                                         COPY_INTO_FILE_GRANT);
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
  auto* state = GetSecurityState(child_id);
  if (!state)
    return false;
  return state->HasPermissionsForFile(file, permissions);
}

size_t ChildProcessSecurityPolicyImpl::BrowsingInstanceIdCountForTesting(
    int child_id) {
  base::AutoLock lock(lock_);
  SecurityState* security_state = GetSecurityState(child_id);
  if (security_state)
    return security_state->browsing_instance_default_isolation_states().size();
  return 0;
}

CanCommitStatus ChildProcessSecurityPolicyImpl::CanCommitOriginAndUrl(
    int child_id,
    const IsolationContext& isolation_context,
    const UrlInfo& url_info) {
  DCHECK(url_info.origin.has_value());
  const url::Origin& origin = *url_info.origin;
  // First check whether the URL is allowed to commit, without considering the
  // origin. This involves scheme checks as well as CanAccessDataForOrigin.
  if (base::FeatureList::IsEnabled(
          features::kAdditionalNavigationCommitChecks) &&
      !CanCommitURL(child_id, url_info.url)) {
    // WebView's allow_universal_access_from_file_urls setting allows file
    // origins to access any other origin and bypass normal commit checks. When
    // this mode is enabled, RenderFrameHostImpl::ValidateURLAndOrigin returns
    // early before this function is called.
    //
    // However, there are also cases where WebView apps in the wild turn on this
    // mode, load one file:// document, then turn it off again and call
    // document.open on another file:// document, causing it to inherit a URL
    // that is not permitted by CanCommitURL anymore. We exempt these cases from
    // the CanCommitURL check specifically, by ignoring a failure if it occurs
    // in a file:// origin within a process which previously had universal
    // access. (This exemption could be done in ValidateURLAndOrigin alongside
    // the universal access check, but in practice no apps in the wild seem to
    // be failing any other types of validation, so doing it here is a narrower
    // exemption.) See https://crbug.com/326250356.
    bool exempt_due_to_webview_universal_access =
        (origin.scheme() == url::kFileScheme) &&
        HasOriginCheckExemptionForWebView(child_id, origin);

    // This enforcement is currently skipped on Android WebView due to crashes.
    // TODO(https://crbug.com/326250356): Diagnose and enable for Android
    // WebView as well.
    if (GetContentClient()->browser()->ShouldEnforceNewCanCommitUrlChecks() &&
        !exempt_due_to_webview_universal_access) {
      return CanCommitStatus::CANNOT_COMMIT_URL;
    }
  }

  // Next check whether the origin resolved from the URL is allowed to commit.
  const url::Origin url_origin = url::Origin::Resolve(url_info.url, origin);
  if (!CanAccessOrigin(child_id, url_origin, AccessType::kCanCommitNewOrigin)) {
    // Check for special cases, like blob:null/ and data: URLs, where the
    // origin does not contain information to match against the process lock,
    // but using the whole URL can result in a process lock match.  Note that
    // the origin being committed in `url_info.origin` will not actually be
    // used when computing `expected_process_lock` below in many cases; see
    // https://crbug.com/1320402.
    const auto expected_process_lock =
        ProcessLock::Create(isolation_context, url_info);
    const ProcessLock& actual_process_lock = GetProcessLock(child_id);
    if (actual_process_lock == expected_process_lock)
      return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // Finally check the origin on its own.
  if (!CanAccessOrigin(child_id, origin, AccessType::kCanCommitNewOrigin)) {
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
  }

  // Ensure that the origin derived from |url| is consistent with |origin|.
  // Note: We can't use origin.IsSameOriginWith() here because opaque origins
  // with precursors may have different nonce values.
  const auto url_tuple_or_precursor_tuple =
      url_origin.GetTupleOrPrecursorTupleIfOpaque();
  const auto origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();

  if (url_tuple_or_precursor_tuple.IsValid() &&
      origin_tuple_or_precursor_tuple.IsValid() &&
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

    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
  }

  return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

bool ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(
    int child_id,
    const url::Origin& origin) {
  return CanAccessOrigin(child_id, origin,
                         AccessType::kCanAccessDataForCommittedOrigin);
}

bool ChildProcessSecurityPolicyImpl::HostsOrigin(int child_id,
                                                 const url::Origin& origin) {
  return CanAccessOrigin(child_id, origin, AccessType::kHostsOrigin);
}

bool ChildProcessSecurityPolicyImpl::CanAccessOrigin(int child_id,
                                                     const url::Origin& origin,
                                                     AccessType access_type) {
  // Ensure this is only called on the UI thread, which is the only thread
  // with sufficient information to do the full set of checks.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL url_to_check;
  if (origin.opaque()) {
    auto precursor_tuple = origin.GetTupleOrPrecursorTupleIfOpaque();
    if (!precursor_tuple.IsValid()) {
      // Allow opaque origins w/o precursors (if the security state exists).
      // TODO(acolwell): Investigate all cases that trigger this path (e.g.,
      // browser-initiated navigations to data: URLs) and fix them so we have
      // precursor information (or the process lock is compatible with a missing
      // precursor). Remove this logic once that has been completed.
      base::AutoLock lock(lock_);
      SecurityState* security_state = GetSecurityState(child_id);
      return !!security_state;
    } else {
      url_to_check = precursor_tuple.GetURL();
    }
  } else {
    url_to_check = origin.GetURL();
  }
  bool success = CanAccessMaybeOpaqueOrigin(child_id, url_to_check,
                                            origin.opaque(), access_type);
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

bool ChildProcessSecurityPolicyImpl::IsAccessAllowedForSandboxedProcess(
    const ProcessLock& process_lock,
    const GURL& url,
    bool url_is_for_opaque_origin,
    AccessType access_type) {
  if (!base::FeatureList::IsEnabled(features::kSandboxedFrameEnforcements)) {
    return true;
  }

  switch (access_type) {
    case AccessType::kCanCommitNewOrigin:
      // TODO(crbug.com/325410297): Sandboxed frames may commit normal URLs, as
      // long as they commit them with an opaque origin. However, some existing
      // code paths leading here, such as CanCommitURL() and
      // CanCommitOriginAndUrl(), do not indicate anything about the future
      // origin being opaque. For now, don't restrict URLs from committing in
      // sandboxed processes here, but eventually this should be strengthened
      // by plumbing in the correct value for `url_is_for_opaque_origin` from
      // code paths like CanCommitURL().
      return true;
    case AccessType::kHostsOrigin:
      // Sandboxed frame processes should only be able to host opaque origins,
      // and only those origins should ever be used as a source or initiator
      // origin in things like postMessage.
      return url_is_for_opaque_origin;
    case AccessType::kCanAccessDataForCommittedOrigin:
      // Sandboxed frames should never access passwords, storage, or other data
      // for any origin.
      return false;
  }
}

bool ChildProcessSecurityPolicyImpl::IsAccessAllowedForPdfProcess(
    AccessType access_type) {
  if (!base::FeatureList::IsEnabled(features::kPdfEnforcements)) {
    return true;
  }

  // PDF processes are allowed to commit normal URLs, and they should be able to
  // claim that they host a regular origin for things like verifying source
  // origins for postMessage. However, PDF renderers should never need to access
  // passwords, storage, or other data for the PDF document's origin or any
  // other origin.
  switch (access_type) {
    case AccessType::kCanCommitNewOrigin:
    case AccessType::kHostsOrigin:
      return true;
    case AccessType::kCanAccessDataForCommittedOrigin:
      return false;
  }
}

bool ChildProcessSecurityPolicyImpl::PerformJailAndCitadelChecks(
    int child_id,
    SecurityState* security_state,
    const GURL& url,
    bool url_is_precursor_of_opaque_origin,
    AccessType access_type,
    ProcessLock& out_expected_process_lock,
    std::string& out_failure_reason) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProcessLock actual_process_lock = security_state->process_lock();

  BrowserOrResourceContext browser_or_resource_context =
      security_state->GetBrowserOrResourceContext();
  // The caller ensures that the `browser_or_resource_context` is valid.
  CHECK(browser_or_resource_context);

  // Loop over all BrowsingInstanceIDs in the SecurityState, and return true if
  // any of them would return true, otherwise return false. This allows the
  // checks to be slightly stricter in cases where all BrowsingInstances agree
  // (e.g., whether an origin is considered isolated and thus inaccessible from
  // a site-locked process).  When the BrowsingInstances do not agree, the check
  // might be slightly weaker (as the least common denominator), but the
  // differences must never violate the ProcessLock.
  if (security_state->browsing_instance_default_isolation_states().empty()) {
    // If no BrowsingInstances are found, then the some of the state we need to
    // perform an accurate check is unexpectedly missing, because there should
    // always be a BrowsingInstance for such requests, even from workers. Thus,
    // we should usually kill the process in this case, so that a compromised
    // renderer can't bypass checks by sending IPCs when no BrowsingInstances
    // are left.
    //
    // However, if the requested `url` is compatible with the current
    // ProcessLock, then there is no need to kill the process because the checks
    // would have passed anyway. To reduce the number of crashes while we debug
    // why no BrowsingInstances were found (in https://crbug.com/1148542), we'll
    // allow requests with an acceptable process lock to proceed.
    // TODO(crbug.com/40731345): Remove this when known cases of having no
    // BrowsingInstance IDs are solved.
    url::Origin origin(url::Origin::Create(url));
    bool matches_origin_keyed_process =
        actual_process_lock.is_origin_keyed_process() &&
        actual_process_lock.lock_url() == origin.GetURL();
    bool matches_site_keyed_process =
        !actual_process_lock.is_origin_keyed_process() &&
        actual_process_lock.lock_url() == SiteInfo::GetSiteForOrigin(origin);
    // ProcessLocks with is_pdf() = true actually means that the process is not
    // supposed to access certain resources from the lock's site/origin, so it's
    // safest here to fall through in that case. See discussion of
    // https://crbug.com/1271197 below.
    if (!actual_process_lock.is_pdf()) {
      // If the ProcessLock isn't locked to a site, we should fall through since
      // we have no way of knowing if the requested url was expecting to be in a
      // locked process.
      if (actual_process_lock.is_locked_to_site()) {
        if (matches_origin_keyed_process || matches_site_keyed_process) {
          return true;
        } else {
          out_failure_reason = base::StringPrintf(
              "No BrowsingInstanceIDs: Lock Mismatch. lock = %s vs. "
              "requested_url = %s ",
              actual_process_lock.ToString().c_str(), url.spec().c_str());
        }
      } else {
        out_failure_reason =
            "No BrowsingInstanceIDs: process not locked to site";
      }
    } else {
      out_failure_reason = "No BrowsingInstanceIDs: process lock is_pdf";
    }
    return false;
  }

  for (auto browsing_instance_info_entry :
       security_state->browsing_instance_default_isolation_states()) {
    auto& browsing_instance_id = browsing_instance_info_entry.first;
    auto& default_isolation_state = browsing_instance_info_entry.second;
    // In the case of multiple BrowsingInstances in the SecurityState, note that
    // failure reasons will only be reported if none of the BrowsingInstances
    // allow access. In that event, |failure_reason| contains the concatenated
    // reasons for each BrowsingInstance, each prefaced by its id.
    out_failure_reason +=
        base::StringPrintf("[BI=%d]", browsing_instance_id.GetUnsafeValue());

    // Use the actual process lock's state to compute `is_guest` and `is_fenced`
    // for the expected process lock's `isolation_context`. Guest status and
    // fenced status doesn't currently influence the outcome of this access
    // check, and even if it did, `url` wouldn't be sufficient to tell whether
    // the request belongs solely to a guest (or non-guest) or fenced process.
    // Note that a guest isn't allowed to access data outside of its own
    // StoragePartition, but this is enforced by other means (e.g., resource
    // access APIs can't name an alternate StoragePartition).
    IsolationContext isolation_context(
        browsing_instance_id, browser_or_resource_context,
        actual_process_lock.is_guest(), actual_process_lock.is_fenced(),
        default_isolation_state);

    // NOTE: If we're on the IO thread, the call to ProcessLock::Create() below
    // will return a ProcessLock with an (internally) identical site_url, one
    // that does not use effective URLs. That's ok in this instance since we
    // only ever look at the lock url.
    //
    // Since we are dealing with a valid ProcessLock at this point, we know the
    // lock contains a valid StoragePartitionConfig and COOP/COEP information
    // because that information must be provided when creating the locks.
    //
    // At this point, any origin opt-in isolation requests should be complete,
    // so to avoid the possibility of opting something set
    // |origin_isolation_request| to kNone below (this happens by default in
    // UrlInfoInit's ctor).  Note: We might need to revisit this if
    // CanAccessDataForOrigin() needs to be called while a SiteInstance is being
    // determined for a navigation, i.e. during
    // GetSiteInstanceForNavigationRequest().  If this happens, we'd need to
    // plumb UrlInfo::origin_isolation_request value from the ongoing
    // NavigationRequest into here. Also, we would likely need to attach the
    // BrowsingInstanceID to UrlInfo once the SiteInstance has been determined
    // in case the RenderProcess has multiple BrowsingInstances in it.
    // TODO(acolwell): Provide a way for callers, that know their request's
    // require COOP/COEP handling, to pass in their COOP/COEP information so it
    // can be used here instead of the values in |actual_process_lock|.
    // TODO(crbug.com/40205612): The code below is subtly incorrect in cases
    // where actual_process_lock.is_pdf() is true, since in the case of PDFs the
    // lock is intended to prevent access to the lock's site/origin, while still
    // allowing the navigation to commit.
    out_expected_process_lock = ProcessLock::Create(
        isolation_context,
        UrlInfo(
            UrlInfoInit(url)
                .WithStoragePartitionConfig(
                    actual_process_lock.GetStoragePartitionConfig())
                .WithWebExposedIsolationInfo(
                    actual_process_lock.GetWebExposedIsolationInfo())
                .WithIsPdf(actual_process_lock.is_pdf())
                .WithSandbox(actual_process_lock.is_sandboxed())
                .WithUniqueSandboxId(actual_process_lock.unique_sandbox_id())
                .WithCrossOriginIsolationKey(
                    actual_process_lock.agent_cluster_key()
                        ? actual_process_lock.agent_cluster_key()
                              ->GetCrossOriginIsolationKey()
                        : std::nullopt)));

    if (actual_process_lock.is_locked_to_site()) {
      // Jail-style enforcement - a process with a lock can only access data
      // from origins that require exactly the same lock.
      if (actual_process_lock == out_expected_process_lock) {
        return true;
      }

      // TODO(acolwell, nasko): https://crbug.com/1029092: Ensure the precursor
      // of opaque origins matches the renderer's origin lock.
      if (url_is_precursor_of_opaque_origin) {
        const GURL& lock_url = actual_process_lock.lock_url();
        // SitePerProcessBrowserTest.TwoBlobURLsWithNullOriginDontShareProcess.
        if (lock_url.SchemeIsBlob() &&
            base::StartsWith(lock_url.path_piece(), "null/")) {
          return true;
        }

        // DeclarativeApiTest.PersistRules.
        if (actual_process_lock.matches_scheme(url::kDataScheme)) {
          return true;
        }
      }

      // Make an exception to allow most visited tiles to commit in third-party
      // NTP processes.
      // TODO(crbug.com/40447789): This exception should be removed once these
      // tiles can be loaded in OOPIFs on the NTP.
      if (AllowProcessLockMismatchForNTP(out_expected_process_lock,
                                         actual_process_lock)) {
        return true;
      }

      // TODO(wjmaclean): We should update the ProcessLock comparison API to
      // return a reason why two locks differ.
      if (actual_process_lock.lock_url() !=
          out_expected_process_lock.lock_url()) {
        out_failure_reason += "lock_mismatch:url ";
        // If the actual lock is same-site to the expected lock, then this is an
        // isolated origins mismatch; in that case we add text to
        // |failure_reason| to make this case easy to search for. Note: We don't
        // compare ports, since the mismatch might be between isolated and
        // non-isolated.
        url::Origin actual_origin =
            url::Origin::Create(actual_process_lock.lock_url());
        url::Origin expected_origin =
            url::Origin::Create(out_expected_process_lock.lock_url());
        if (actual_process_lock.lock_url() ==
                SiteInfo::GetSiteForOrigin(expected_origin) ||
            out_expected_process_lock.lock_url() ==
                SiteInfo::GetSiteForOrigin(actual_origin)) {
          out_failure_reason += "[origin vs site mismatch] ";
        }
      } else {
        // TODO(wjmaclean,alexmos): Apparently this might not be true anymore,
        // since is_pdf() and web_exposed_isolation_info() have been added to
        // the ProcessLock. We need to update the code here to differentiate
        // these cases, as well as adding documentation (or some other
        // mechanism) to prevent these getting out of sync in future.
        out_failure_reason += "lock_mismatch:requires_origin_keyed_process ";
      }
    } else {
      // Citadel-style enforcement - an unlocked process should not be able to
      // access data from origins that require a lock.

      RenderProcessHost* process = RenderProcessHostImpl::FromID(child_id);
      if (process) {  // |process| can be null in unittests
        // Unlocked process can be legitimately used when navigating from an
        // unused process (about:blank, NTP on Android) to an isolated origin.
        // See also https://crbug.com/945399.  Returning |true| below will allow
        // such navigations to succeed (i.e. pass CanCommitOriginAndUrl checks).
        // We don't expect unused processes to be used outside of navigations
        // (e.g. when checking CanAccessDataForOrigin for localStorage, etc.).
        if (process->IsUnused()) {
          return true;
        }
      }

      // See the ProcessLock::Create() call above regarding why we pass kNone
      // for |origin_isolation_request| below.
      SiteInfo site_info = SiteInfo::Create(
          isolation_context,
          UrlInfo(UrlInfoInit(url).WithWebExposedIsolationInfo(
              actual_process_lock.GetWebExposedIsolationInfo())));

      // A process that's not locked to any site can only access data from
      // origins that do not require a locked process.
      if (!site_info.ShouldLockProcessToSite(isolation_context)) {
        return true;
      }

      out_failure_reason += " citadel_enforcement ";
      if (url_is_precursor_of_opaque_origin) {
        out_failure_reason += "for_precursor ";
      }

      // TODO(crbug.com/326251583): Log additional information for diagnosing
      // the bug. Remove once the investigation is complete.
      if (site_info.RequiresDedicatedProcess(isolation_context)) {
        out_failure_reason += "dedicated ";
        if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
          out_failure_reason += "spp ";
        }
        if (site_info.does_site_request_dedicated_process_for_coop()) {
          out_failure_reason += "coop ";
        }
        if (site_info.requires_origin_keyed_process()) {
          out_failure_reason += "oac ";
        }
        if (site_info.is_sandboxed()) {
          out_failure_reason += "sandbox ";
        }
        if (site_info.is_error_page()) {
          out_failure_reason += "error ";
        }
        if (site_info.is_pdf()) {
          out_failure_reason += "pdf ";
        }
        if (IsIsolatedOrigin(isolation_context,
                             url::Origin::Create(site_info.site_url()),
                             site_info.requires_origin_keyed_process())) {
          out_failure_reason += "io ";
        }
      }
      out_failure_reason +=
          "site=" + site_info.site_url().possibly_invalid_spec();
      out_failure_reason +=
          " next_bi=" +
          base::NumberToString(
              SiteInstanceImpl::NextBrowsingInstanceId().GetUnsafeValue());
      out_failure_reason +=
          " dis_oac=" + base::NumberToString(
                            default_isolation_state.is_origin_agent_cluster());
      out_failure_reason +=
          " dis_rokp=" +
          base::NumberToString(
              default_isolation_state.requires_origin_keyed_process()) +
          " ";
    }
  }

  return false;
}

bool ChildProcessSecurityPolicyImpl::CanAccessMaybeOpaqueOrigin(
    int child_id,
    const GURL& url,
    bool url_is_precursor_of_opaque_origin,
    AccessType access_type) {
  // Ensure this is only called on the UI thread, which is the only thread
  // with sufficient information to do the full set of checks.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);

  SecurityState* security_state = GetSecurityState(child_id);
  ProcessLock expected_process_lock;
  std::string failure_reason;

  if (!security_state) {
    failure_reason = "no_security_state";
  } else if (!security_state->GetBrowserOrResourceContext()) {
    failure_reason = "no_browser_or_resource_context";
  } else {
    ProcessLock actual_process_lock = security_state->process_lock();

    // Deny access if the process is unlocked. An unlocked process means that
    // the process has not been associated with a SiteInstance yet and therefore
    // this request is likely invalid.
    if (actual_process_lock.is_invalid()) {
      failure_reason = "process_lock_is_invalid";
    } else if (actual_process_lock.is_sandboxed() &&
               !IsAccessAllowedForSandboxedProcess(
                   actual_process_lock, url, url_is_precursor_of_opaque_origin,
                   access_type)) {
      failure_reason = "sandboxing_restrictions";
    } else if (actual_process_lock.is_pdf() &&
               !IsAccessAllowedForPdfProcess(access_type)) {
      failure_reason = "pdf_restrictions";
    } else {
      // Perform Jail and Citadel checks. See PerformJailAndCitadelChecks() for
      // more details. If these checks fail, collect crash keys below before
      // returning false.
      bool passes_jail_and_citadel_checks = PerformJailAndCitadelChecks(
          child_id, security_state, url, url_is_precursor_of_opaque_origin,
          access_type, expected_process_lock, failure_reason);
      if (passes_jail_and_citadel_checks) {
        return true;
      }
    }
  }

  // Record the duration of KeepAlive requests to include in the crash keys.
  std::string keep_alive_durations;
  std::string shutdown_delay_ref_count;
  std::string process_rfh_count;
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (auto* process = RenderProcessHostImpl::FromID(child_id)) {
      keep_alive_durations = process->GetKeepAliveDurations();
      shutdown_delay_ref_count =
          base::NumberToString(process->GetShutdownDelayRefCount());
      process_rfh_count =
          base::NumberToString(process->GetRenderFrameHostCount());
    }
  } else {
    keep_alive_durations = "no durations available: on IO thread.";
  }

  // Returning false here will result in a renderer kill.  Set some crash
  // keys that will help understand the circumstances of that kill.
  LogCanAccessDataForOriginCrashKeys(
      expected_process_lock.ToString(),
      GetKilledProcessOriginLock(security_state),
      url.DeprecatedGetOriginAsURL().spec(), failure_reason,
      keep_alive_durations, shutdown_delay_ref_count, process_rfh_count);
  return false;
}

void ChildProcessSecurityPolicyImpl::IncludeIsolationContext(
    int child_id,
    const IsolationContext& isolation_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  auto* state = GetSecurityState(child_id);
  DCHECK(state);
  state->AddBrowsingInstanceInfo(isolation_context);
}

void ChildProcessSecurityPolicyImpl::LockProcess(
    const IsolationContext& context,
    int child_id,
    bool is_process_used,
    const ProcessLock& process_lock) {
  // LockProcess should only be called on the UI thread (OTOH, it is okay to
  // call GetProcessLock from any thread).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  CHECK(state != security_state_.end(), base::NotFatalUntil::M130);
  state->second->SetProcessLock(process_lock, context, is_process_used);
}

void ChildProcessSecurityPolicyImpl::LockProcessForTesting(
    const IsolationContext& isolation_context,
    int child_id,
    const GURL& url) {
  SiteInfo site_info = SiteInfo::CreateForTesting(isolation_context, url);
  LockProcess(isolation_context, child_id, /* is_process_used=*/false,
              ProcessLock::FromSiteInfo(site_info));
}

ProcessLock ChildProcessSecurityPolicyImpl::GetProcessLock(int child_id) {
  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return ProcessLock();
  return state->second->process_lock();
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

  auto* state = GetSecurityState(child_id);
  if (!state)
    return false;
  return state->HasPermissionsForFileSystem(filesystem_id, permission);
}

void ChildProcessSecurityPolicyImpl::RegisterFileSystemPermissionPolicy(
    storage::FileSystemType type,
    int policy) {
  base::AutoLock lock(lock_);
  file_system_policy_map_[type] = policy;
}

bool ChildProcessSecurityPolicyImpl::CanSendMidiMessage(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end()) {
    return false;
  }

  return state->second->CanSendMidi();
}

bool ChildProcessSecurityPolicyImpl::CanSendMidiSysExMessage(int child_id) {
  base::AutoLock lock(lock_);

  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->CanSendMidiSysEx();
}

void ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins(
    const std::vector<url::Origin>& origins_to_add,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  std::vector<IsolatedOriginPattern> patterns;
  patterns.reserve(origins_to_add.size());
  base::ranges::transform(
      origins_to_add, std::back_inserter(patterns),
      [](const url::Origin& o) { return IsolatedOriginPattern(o); });
  AddFutureIsolatedOrigins(patterns, source, browser_context);
}

void ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins(
    std::string_view origins_to_add,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  std::vector<IsolatedOriginPattern> patterns =
      ParseIsolatedOrigins(origins_to_add);
  AddFutureIsolatedOrigins(patterns, source, browser_context);
}

void ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins(
    const std::vector<IsolatedOriginPattern>& patterns,
    IsolatedOriginSource source,
    BrowserContext* browser_context) {
  // This can only be called from the UI thread, as it reads state that's only
  // available (and is only safe to be retrieved) on the UI thread, such as
  // BrowsingInstance IDs.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  for (const IsolatedOriginPattern& pattern : patterns) {
    if (!pattern.is_valid()) {
      LOG(ERROR) << "Invalid isolated origin: " << pattern.pattern();
      continue;
    }

    url::Origin origin_to_add = pattern.origin();

    // Isolated origins added here should apply only to future
    // BrowsingInstances and processes.  Determine the first BrowsingInstance
    // ID to which they should apply.
    BrowsingInstanceId browsing_instance_id =
        SiteInstanceImpl::NextBrowsingInstanceId();

    AddIsolatedOriginInternal(browser_context, origin_to_add,
                              true /* applies_to_future_browsing_instances */,
                              browsing_instance_id,
                              pattern.isolate_all_subdomains(), source);
  }
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOriginInternal(
    BrowserContext* browser_context,
    const url::Origin& origin_to_add,
    bool applies_to_future_browsing_instances,
    BrowsingInstanceId browsing_instance_id,
    bool isolate_all_subdomains,
    IsolatedOriginSource source) {
  // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
  // up the isolated origin lookup.  This only performs a straightforward
  // translation of an origin to eTLD+1; it does *not* take into account
  // effective URLs, isolated origins, and other logic that's not needed
  // here, but *is* typically needed for making process model decisions. Be
  // very careful about using GetSiteForOrigin() elsewhere, and consider
  // whether you should be using SiteInfo::Create() instead.
  GURL key(SiteInfo::GetSiteForOrigin(origin_to_add));

  // Check if the origin to be added already exists, in which case it may not
  // need to be added again.
  bool should_add = true;
  for (const auto& entry : isolated_origins_[key]) {
    // TODO(alexmos): The exact origin comparison here allows redundant entries
    // with certain uses of `isolate_all_subdomains`.  See
    // https://crbug.com/1184580.
    if (entry.origin() != origin_to_add)
      continue;
    // If the added origin already exists for the same BrowserContext and
    // covers the same BrowsingInstances, don't re-add it.
    if (entry.browser_context() == browser_context) {
      if (entry.applies_to_future_browsing_instances() &&
          entry.browsing_instance_id() <= browsing_instance_id) {
        // If the existing entry applies to future BrowsingInstances, and it
        // has a lower/same BrowsingInstance ID, don't re-add the origin.  Note
        // that if the new isolated origin is also requested to apply to future
        // BrowsingInstances, the threshold ID must necessarily be greater than
        // the old ID, since NextBrowsingInstanceId() returns monotonically
        // increasing IDs.
        if (applies_to_future_browsing_instances)
          DCHECK_LE(entry.browsing_instance_id(), browsing_instance_id);
        should_add = false;
        break;
      } else if (!entry.applies_to_future_browsing_instances() &&
                 entry.browsing_instance_id() == browsing_instance_id) {
        // Otherwise, don't re-add the origin if the existing entry is for the
        // same BrowsingInstance ID.  Note that if an origin had been added for
        // a specific BrowsingInstance, we can't later receive a request to
        // isolate that origin within future BrowsingInstances that start at
        // the same (or lower) BrowsingInstance. Requests to isolate future
        // BrowsingInstances should always reference
        // SiteInstanceImpl::NextBrowsingInstanceId(), which always refers to
        // an ID that's greater than any existing BrowsingInstance ID.
        DCHECK(!applies_to_future_browsing_instances);

        should_add = false;
        break;
      }
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
    IsolatedOriginEntry entry(std::move(origin_to_add),
                              applies_to_future_browsing_instances,
                              browsing_instance_id, browser_context,
                              resource_context, isolate_all_subdomains, source);
    isolated_origins_[key].emplace_back(std::move(entry));
  }
}

void ChildProcessSecurityPolicyImpl::RemoveStateForBrowserContext(
    const BrowserContext& browser_context) {
  {
    base::AutoLock isolated_origins_lock(isolated_origins_lock_);

    for (auto& iter : isolated_origins_) {
      std::erase_if(iter.second,
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

  {
    base::AutoLock lock(lock_);
    for (auto& pair : security_state_)
      pair.second->ClearBrowserContextIfMatches(&browser_context);

    for (auto& pair : pending_remove_state_)
      pair.second->ClearBrowserContextIfMatches(&browser_context);
  }
}

bool ChildProcessSecurityPolicyImpl::IsIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    bool origin_requests_isolation) {
  url::Origin unused_result;
  return GetMatchingProcessIsolatedOrigin(
      isolation_context, origin, origin_requests_isolation, &unused_result);
}

bool ChildProcessSecurityPolicyImpl::IsGloballyIsolatedOriginForTesting(
    const url::Origin& origin) {
  BrowserOrResourceContext no_browser_context;
  BrowsingInstanceId null_browsing_instance_id;
  IsolationContext isolation_context(
      null_browsing_instance_id, no_browser_context, /*is_guest=*/false,
      /*is_fenced=*/false,
      OriginAgentClusterIsolationState::CreateNonIsolated());
  return IsIsolatedOrigin(isolation_context, origin, false);
}

std::vector<url::Origin> ChildProcessSecurityPolicyImpl::GetIsolatedOrigins(
    std::optional<IsolatedOriginSource> source,
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

      // Do not include origins that only apply to specific BrowsingInstances.
      if (!isolated_origin_entry.applies_to_future_browsing_instances())
        continue;

      origins.push_back(isolated_origin_entry.origin());
    }
  }
  return origins;
}

bool ChildProcessSecurityPolicyImpl::IsIsolatedSiteFromSource(
    const url::Origin& origin,
    IsolatedOriginSource source) {
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);
  GURL site_url = SiteInfo::GetSiteForOrigin(origin);
  auto it = isolated_origins_.find(site_url);
  if (it == isolated_origins_.end())
    return false;
  url::Origin site_origin = url::Origin::Create(site_url);
  for (const auto& entry : it->second) {
    if (entry.source() == source && entry.origin() == site_origin)
      return true;
  }
  return false;
}

bool ChildProcessSecurityPolicyImpl::GetMatchingProcessIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    bool requests_origin_keyed_process,
    url::Origin* result) {
  // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
  // up the isolated origin lookup.  This only performs a straightforward
  // translation of an origin to eTLD+1; it does *not* take into account
  // effective URLs, isolated origins, and other logic that's not needed
  // here, but *is* typically needed for making process model decisions. Be
  // very careful about using GetSiteForOrigin() elsewhere, and consider
  // whether you should be using GetSiteForURL() instead.
  return GetMatchingProcessIsolatedOrigin(
      isolation_context, origin, requests_origin_keyed_process,
      SiteInfo::GetSiteForOrigin(origin), result);
}

bool ChildProcessSecurityPolicyImpl::GetMatchingProcessIsolatedOrigin(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    bool requests_origin_keyed_process,
    const GURL& site_url,
    url::Origin* result) {
  DCHECK(IsRunningOnExpectedThread());

  *result = url::Origin();
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  // If |isolation_context| does not specify a BrowsingInstance ID (which should
  // only happen in tests), then assume that we want to retrieve the latest
  // applicable information; i.e., return the latest matching isolated origins
  // that would apply to future BrowsingInstances.  Using
  // NextBrowsingInstanceId() will match all available IsolatedOriginEntries.
  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());
  if (browsing_instance_id.is_null()) {
    browsing_instance_id = SiteInstanceImpl::NextBrowsingInstanceId();
  }

  // Check the opt-in isolation status of |origin| in |isolation_context|.
  // Note that while IsolatedOrigins considers any sub-origin of an isolated
  // origin as also being isolated, with opt-in we will always either return
  // false, or true with result set to |origin|. We give priority to origins
  // requesting opt-in isolation over command-line isolation.
  // Note: This should only return a full origin if we are doing
  // process-isolated Origin-keyed Agent Clusters, which will only be the case
  // when site-isolation is enabled. Otherwise we put the origin into its
  // corresponding site, even if Origin-keyed Agent Clusters will be enabled
  // on the renderer side.
  // TODO(wjmaclean,alexmos,acolwell): We should revisit this when we have
  // SiteInstanceGroups, since at that point we can again return an origin
  // here (and thus create a new SiteInstance) even when
  // IsProcessIsolationForOriginAgentClusterEnabled() returns false; in that
  // case a SiteInstanceGroup will allow a logical group of SiteInstances that
  // live same-process.
  if (SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()) {
    OriginAgentClusterIsolationState oac_isolation_state_request =
        requests_origin_keyed_process
            ? OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
                  true /* requires_origin_keyed_process */)
            : OriginAgentClusterIsolationState::CreateNonIsolated();
    OriginAgentClusterIsolationState oac_isolation_state_result =
        DetermineOriginAgentClusterIsolation(isolation_context, origin,
                                             oac_isolation_state_request);
    if (oac_isolation_state_result.requires_origin_keyed_process()) {
      *result = origin;
      return true;
    }
  }

  // Look up the list of origins corresponding to |origin|'s site.
  auto it = isolated_origins_.find(site_url);

  // Subtle corner case: if the site's host ends with a dot, do the lookup
  // without it.  A trailing dot shouldn't be able to bypass isolated origins:
  // if "https://foo.com" is an isolated origin, "https://foo.com." should
  // match it.
  if (it == isolated_origins_.end() && site_url.has_host() &&
      site_url.host_piece().back() == '.') {
    GURL::Replacements replacements;
    std::string_view host(site_url.host_piece());
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

      if (isolated_origin_entry.MatchesBrowsingInstance(browsing_instance_id) &&
          IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(
              origin, isolated_origin_entry.origin())) {
        // If a match has been found that requires all subdomains to be isolated
        // then return immediately. |origin| is returned to ensure proper
        // process isolation, e.g. https://a.b.c.isolated.com matches an
        // IsolatedOriginEntry constructed from http://[*.]isolated.com, so
        // https://a.b.c.isolated.com must be returned.
        if (isolated_origin_entry.isolate_all_subdomains()) {
          *result = origin;
          uint16_t default_port = url::DefaultPortForScheme(origin.scheme());

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

OriginAgentClusterIsolationState
ChildProcessSecurityPolicyImpl::DetermineOriginAgentClusterIsolation(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    const OriginAgentClusterIsolationState& requested_isolation_state) {
  if (!IsolatedOriginUtil::IsValidOriginForOptInIsolation(origin))
    return OriginAgentClusterIsolationState::CreateNonIsolated();

  // See if the same origin exists in the BrowsingInstance already, and if so
  // return its isolation status.
  // There are two cases we're worried about here: (i) we've previously seen the
  // origin and isolated it, in which case we should continue to isolate it, and
  // (ii) we've previously seen the origin and *not* isolated it, in which case
  // we should continue to not isolate it.
  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());

  if (!browsing_instance_id.is_null()) {
    base::AutoLock origins_isolation_opt_in_lock(
        origins_isolation_opt_in_lock_);

    // Look for |origin| in the isolation status list.
    OriginAgentClusterIsolationState* oac_isolation_state =
        LookupOriginIsolationState(browsing_instance_id, origin);

    if (oac_isolation_state)
      return *oac_isolation_state;
  }

  // If we get to this point, then |origin| is neither opted-in nor opted-out.
  // At this point we allow opting in if it's requested. This is true for
  // either logical OriginAgentCluster, or OriginAgentCluster with an
  // origin-keyed process.
  return requested_isolation_state;
}

bool ChildProcessSecurityPolicyImpl::
    HasOriginEverRequestedOriginAgentClusterValue(
        BrowserContext* browser_context,
        const url::Origin& origin) {
  base::AutoLock origins_isolation_opt_in_lock(origins_isolation_opt_in_lock_);
  return base::Contains(origin_isolation_opt_ins_and_outs_, browser_context) &&
         base::Contains(origin_isolation_opt_ins_and_outs_[browser_context],
                        origin);
}

OriginAgentClusterIsolationState*
ChildProcessSecurityPolicyImpl::LookupOriginIsolationState(
    const BrowsingInstanceId& browsing_instance_id,
    const url::Origin& origin) {
  auto it_isolation_by_browsing_instance =
      origin_isolation_by_browsing_instance_.find(browsing_instance_id);
  if (it_isolation_by_browsing_instance ==
      origin_isolation_by_browsing_instance_.end()) {
    return nullptr;
  }
  auto& origin_list = it_isolation_by_browsing_instance->second;
  auto it_origin_list = base::ranges::find(
      origin_list, origin, &OriginAgentClusterOptInEntry::origin);
  if (it_origin_list != origin_list.end())
    return &(it_origin_list->oac_isolation_state);
  return nullptr;
}

OriginAgentClusterIsolationState*
ChildProcessSecurityPolicyImpl::LookupOriginIsolationStateForTesting(
    const BrowsingInstanceId& browsing_instance_id,
    const url::Origin& origin) {
  base::AutoLock lock(origins_isolation_opt_in_lock_);
  return LookupOriginIsolationState(browsing_instance_id, origin);
}

void ChildProcessSecurityPolicyImpl::AddDefaultIsolatedOriginIfNeeded(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    bool is_global_walk_or_frame_removal) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsolatedOriginUtil::IsValidOriginForOptInIsolation(origin))
    return;

  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());
  // All callers to this function live on the UI thread, so the IsolationContext
  // should contain a BrowserContext*.
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);
  CHECK(!browsing_instance_id.is_null());

  base::AutoLock origins_isolation_opt_in_lock(origins_isolation_opt_in_lock_);

  // Commits of origins that have ever sent the OriginAgentCluster header in
  // this BrowserContext are tracked in every BrowsingInstance in this
  // BrowserContext, to avoid having to do multiple global walks. If the origin
  // isn't in the list of such origins (i.e., the common case), return early to
  // avoid unnecessary work, since this is called on every commit. Skip this
  // during global walks and frame removals, since we do want to track the
  // origin's non-isolated status in those cases.
  if (!is_global_walk_or_frame_removal &&
      !(base::Contains(origin_isolation_opt_ins_and_outs_, browser_context) &&
        base::Contains(origin_isolation_opt_ins_and_outs_[browser_context],
                       origin))) {
    return;
  }

  // If |origin| is already in the opt-in-out list, then we don't want to add it
  // to the list. Technically this check is unnecessary during global
  // walks (when the origin won't be in this list yet), but it matters during
  // frame removal (when we don't want to add an opted-in origin to the
  // list as non-isolated when its frame is removed).
  if (LookupOriginIsolationState(browsing_instance_id, origin)) {
    return;
  }

  // Since there was no prior record for this BrowsingInstance, track that this
  // origin should use the default isolation model in use by the
  // BrowsingInstance.
  origin_isolation_by_browsing_instance_[browsing_instance_id].emplace_back(
      isolation_context.default_isolation_state(), origin);
}

void ChildProcessSecurityPolicyImpl::
    RemoveOptInIsolatedOriginsForBrowsingInstance(
        const BrowsingInstanceId& browsing_instance_id) {
  // After a suitable delay, remove this BrowsingInstance's info from any
  // SecurityStates that are using it.
  // TODO(wjmaclean): Monitor the CanAccessDataForOrigin crash key in renderer
  // kills to see if we get post-BrowsingInstance-destruction ProcessLock
  // mismatches, indicating this cleanup should be further delayed.
  auto task_closure = [](const BrowsingInstanceId id) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->RemoveOptInIsolatedOriginsForBrowsingInstanceInternal(id);
  };
  if (browsing_instance_cleanup_delay_.is_positive()) {
    // Do the actual state cleanup after posting a task to the IO thread, to
    // give a chance for any last unprocessed tasks to be handled. The cleanup
    // itself locks the data structures and can safely happen from either
    // thread.
    GetIOThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(task_closure, browsing_instance_id),
        browsing_instance_cleanup_delay_);
  } else {
    // Since this is just used in tests, it's ok to do it on either thread.
    task_closure(browsing_instance_id);
  }
}

void ChildProcessSecurityPolicyImpl::
    RemoveOptInIsolatedOriginsForBrowsingInstanceInternal(
        const BrowsingInstanceId browsing_instance_id) {
  // If a BrowsingInstance is destructing, we should always have an id for it.
  CHECK(!browsing_instance_id.is_null());

  {
    // content_unittests don't always report being on the IO thread.
    DCHECK(IsRunningOnExpectedThread());
    base::AutoLock lock(lock_);
    for (auto& it : security_state_)
      it.second->ClearBrowsingInstanceId(browsing_instance_id);
    // Note: if the BrowsingInstanceId set is empty at the end of this function,
    // we must never remove the ProcessLock in case the associated RenderProcess
    // is compromised, in which case we wouldn't want to reuse it for another
    // origin.
  }

  {
    base::AutoLock origins_isolation_opt_in_lock(
        origins_isolation_opt_in_lock_);
    origin_isolation_by_browsing_instance_.erase(browsing_instance_id);
  }

  {
    base::AutoLock isolated_origins_lock(isolated_origins_lock_);
    for (auto& iter : isolated_origins_) {
      std::erase_if(iter.second, [&browsing_instance_id](
                                     const IsolatedOriginEntry& entry) {
        // Remove entries that are specific to `browsing_instance_id` and
        // do not apply to future BrowsingInstances.
        return (entry.browsing_instance_id() == browsing_instance_id &&
                !entry.applies_to_future_browsing_instances());
      });
    }
  }
}

void ChildProcessSecurityPolicyImpl::AddCoopIsolatedOriginForBrowsingInstance(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    IsolatedOriginSource source) {
  // We ought to have validated the origin prior to getting here.  If the
  // origin isn't valid at this point, something has gone wrong.
  CHECK(IsolatedOriginUtil::IsValidIsolatedOrigin(origin))
      << "Trying to isolate invalid origin: " << origin;

  // This can only be called from the UI thread, as it reads state that's only
  // available (and is only safe to be retrieved) on the UI thread, such as
  // BrowsingInstance IDs.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());
  // This function should only be called when a BrowsingInstance is registering
  // a new SiteInstance, so |browsing_instance_id| should always be defined.
  CHECK(!browsing_instance_id.is_null());

  // For site-keyed isolation, add `origin` to the isolated_origins_ map (which
  // supports subdomain matching).
  // Ensure that `origin` is a site (scheme + eTLD+1) rather than any origin.
  auto site_origin = url::Origin::Create(SiteInfo::GetSiteForOrigin(origin));
  CHECK_EQ(origin, site_origin);

  base::AutoLock isolated_origins_lock(isolated_origins_lock_);

  // Explicitly set `applies_to_future_browsing_instances` to false to only
  // isolate `origin` within the provided BrowsingInstance, but not future
  // ones.  Note that it's possible for `origin` to also become isolated for
  // future BrowsingInstances if AddFutureIsolatedOrigins() is called for it
  // later.
  AddIsolatedOriginInternal(
      isolation_context.browser_or_resource_context().ToBrowserContext(),
      origin, false /* applies_to_future_browsing_instances */,
      isolation_context.browsing_instance_id(),
      false /* isolate_all_subdomains */, source);
}

void ChildProcessSecurityPolicyImpl::AddOriginIsolationStateForBrowsingInstance(
    const IsolationContext& isolation_context,
    const url::Origin& origin,
    bool is_origin_agent_cluster,
    bool requires_origin_keyed_process) {
  // This can only be called from the UI thread, as it reads state that's only
  // available (and is only safe to be retrieved) on the UI thread, such as
  // BrowsingInstance IDs.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(
      is_origin_agent_cluster ||
      SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(
          isolation_context.browser_or_resource_context().ToBrowserContext()));

  // We ought to have validated the origin prior to getting here.  If the
  // origin isn't valid at this point, something has gone wrong.
  CHECK((is_origin_agent_cluster &&
         IsolatedOriginUtil::IsValidOriginForOptInIsolation(origin)) ||
        // The second part of this check is specific to OAC-by-default, and is
        // required to allow explicit opt-outs for HTTP schemed origins. See
        // OriginAgentClusterInsecureEnabledBrowserTest.DocumentDomain_Disabled.
        IsolatedOriginUtil::IsValidOriginForOptOutIsolation(origin))
      << "Trying to isolate invalid origin: " << origin;

  BrowsingInstanceId browsing_instance_id(
      isolation_context.browsing_instance_id());
  // This function should only be called when a BrowsingInstance is registering
  // a new SiteInstance, so |browsing_instance_id| should always be defined.
  CHECK(!browsing_instance_id.is_null());

  // For origin-keyed isolation, use the origin_isolation_by_browsing_instance_
  // map.
  base::AutoLock origins_isolation_opt_in_lock(origins_isolation_opt_in_lock_);
  auto it = origin_isolation_by_browsing_instance_.find(browsing_instance_id);
  if (it == origin_isolation_by_browsing_instance_.end()) {
    std::tie(it, std::ignore) = origin_isolation_by_browsing_instance_.emplace(
        browsing_instance_id, std::vector<OriginAgentClusterOptInEntry>());
  }

  // We only support adding new entries, not modifying existing ones. If at
  // some point in the future we allow isolation status to change during the
  // lifetime of a BrowsingInstance, then this will need to be updated.
  if (!base::Contains(it->second, origin,
                      &OriginAgentClusterOptInEntry::origin)) {
    it->second.emplace_back(
        is_origin_agent_cluster
            ? OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
                  requires_origin_keyed_process)
            : OriginAgentClusterIsolationState::CreateNonIsolated(),
        origin);
  }
}

bool ChildProcessSecurityPolicyImpl::UpdateOriginIsolationOptInListIfNecessary(
    BrowserContext* browser_context,
    const url::Origin& origin) {
  if (!IsolatedOriginUtil::IsValidOriginForOptInIsolation(origin))
    return false;

  base::AutoLock origins_isolation_opt_in_lock(origins_isolation_opt_in_lock_);

  if (base::Contains(origin_isolation_opt_ins_and_outs_, browser_context) &&
      base::Contains(origin_isolation_opt_ins_and_outs_[browser_context],
                     origin)) {
    return false;
  }

  origin_isolation_opt_ins_and_outs_[browser_context].insert(origin);
  return true;
}

void ChildProcessSecurityPolicyImpl::RemoveIsolatedOriginForTesting(
    const url::Origin& origin) {
  GURL key(SiteInfo::GetSiteForOrigin(origin));
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);
  std::erase_if(isolated_origins_[key],
                [&origin](const IsolatedOriginEntry& entry) {
                  // Remove if origin matches.
                  return (entry.origin() == origin);
                });
  if (isolated_origins_[key].empty())
    isolated_origins_.erase(key);
}

void ChildProcessSecurityPolicyImpl::ClearIsolatedOriginsForTesting() {
  base::AutoLock isolated_origins_lock(isolated_origins_lock_);
  isolated_origins_.clear();
}

ChildProcessSecurityPolicyImpl::SecurityState*
ChildProcessSecurityPolicyImpl::GetSecurityState(int child_id) {
  auto itr = security_state_.find(child_id);
  if (itr != security_state_.end())
    return itr->second.get();

  auto pending_itr = pending_remove_state_.find(child_id);
  if (pending_itr == pending_remove_state_.end())
    return nullptr;

  // At this point the SecurityState in the map is being kept alive
  // by a Handle object or we are waiting for the deletion task to be run on
  // the IO thread.
  SecurityState* pending_security_state = pending_itr->second.get();

  auto count_itr = process_reference_counts_.find(child_id);
  if (count_itr != process_reference_counts_.end()) {
    // There must be a Handle that still holds a reference to this
    // pending state so it is safe to return. The assumption is that the
    // owner of this Handle is making a security check.
    return pending_security_state;
  }

  // Since we don't have an entry in |process_reference_counts_| it means
  // that we are waiting for the deletion task posted to the IO thread to run.
  // Only allow the state to be accessed by the IO thread in this situation.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return pending_security_state;

  return nullptr;
}

std::vector<IsolatedOriginPattern>
ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
    std::string_view pattern_list) {
  std::vector<std::string_view> origin_strings = base::SplitStringPiece(
      pattern_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<IsolatedOriginPattern> patterns;
  patterns.reserve(origin_strings.size());

  for (std::string_view origin_string : origin_strings) {
    patterns.emplace_back(origin_string);
  }

  return patterns;
}

// static
std::string ChildProcessSecurityPolicyImpl::GetKilledProcessOriginLock(
    const SecurityState* security_state) {
  if (!security_state)
    return "(child id not found)";

  if (!security_state->GetBrowserOrResourceContext())
    return "(empty and null context)";

  return security_state->process_lock().ToString();
}

void ChildProcessSecurityPolicyImpl::LogKilledProcessOriginLock(int child_id) {
  base::AutoLock lock(lock_);
  const auto itr = security_state_.find(child_id);
  const SecurityState* security_state =
      itr != security_state_.end() ? itr->second.get() : nullptr;

  base::debug::SetCrashKeyString(GetKilledProcessOriginLockKey(),
                                 GetKilledProcessOriginLock(security_state));
}

ChildProcessSecurityPolicyImpl::Handle
ChildProcessSecurityPolicyImpl::CreateHandle(int child_id) {
  return Handle(child_id, /* duplicating_handle */ false);
}

bool ChildProcessSecurityPolicyImpl::AddProcessReference(
    int child_id,
    bool duplicating_handle) {
  base::AutoLock lock(lock_);
  return AddProcessReferenceLocked(child_id, duplicating_handle);
}

bool ChildProcessSecurityPolicyImpl::AddProcessReferenceLocked(
    int child_id,
    bool duplicating_handle) {
  if (child_id == ChildProcessHost::kInvalidUniqueID)
    return false;

  // Check to see if the SecurityState has been removed from |security_state_|
  // via a Remove() call. This corresponds to the process being destroyed.
  if (!base::Contains(security_state_, child_id)) {
    if (!duplicating_handle) {
      // Do not allow Handles to be created after the process has been
      // destroyed, unless they are being duplicated.
      return false;
    }

    // The process has been destroyed but we are allowing an existing Handle
    // to be duplicated. Verify that the process reference count is available
    // and indicates another Handle has a reference.
    auto itr = process_reference_counts_.find(child_id);
    CHECK(itr != process_reference_counts_.end());
    CHECK_GT(itr->second, 0);
  }

  ++process_reference_counts_[child_id];
  return true;
}

void ChildProcessSecurityPolicyImpl::RemoveProcessReference(int child_id) {
  base::AutoLock lock(lock_);
  RemoveProcessReferenceLocked(child_id);
}

void ChildProcessSecurityPolicyImpl::RemoveProcessReferenceLocked(
    int child_id) {
  auto itr = process_reference_counts_.find(child_id);
  CHECK(itr != process_reference_counts_.end());

  if (itr->second > 1) {
    itr->second--;
    return;
  }

  DCHECK_EQ(itr->second, 1);
  process_reference_counts_.erase(itr);

  // |child_id| could be inside tasks that are on the IO thread task queues. We
  // need to keep the |pending_remove_state_| entry around until we have
  // successfully executed a task on the IO thread. This should ensure that any
  // pending tasks on the IO thread will have completed before we remove the
  // entry.
  // TODO(acolwell): Remove this call once all objects on the IO thread have
  // been converted to use Handles.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ChildProcessSecurityPolicyImpl* policy, int child_id) {
                       DCHECK_CURRENTLY_ON(BrowserThread::IO);
                       base::AutoLock lock(policy->lock_);
                       policy->pending_remove_state_.erase(child_id);
                     },
                     base::Unretained(this), child_id));
}

}  // namespace content
