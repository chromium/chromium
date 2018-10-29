// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

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

}  // namespace

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicyImpl::SecurityState {
 public:
  SecurityState()
    : enabled_bindings_(0),
      can_read_raw_cookies_(false),
      can_send_midi_sysex_(false) { }

  ~SecurityState() {
    storage::IsolatedContext* isolated_context =
        storage::IsolatedContext::GetInstance();
    for (auto iter = filesystem_permissions_.begin();
         iter != filesystem_permissions_.end(); ++iter) {
      isolated_context->RemoveReference(iter->first);
    }
    UMA_HISTOGRAM_COUNTS_1M(
        "ChildProcessSecurityPolicy.PerChildFilePermissions",
        file_permissions_.size());
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
    UMA_HISTOGRAM_COUNTS_1M(
        "ChildProcessSecurityPolicy.FilePermissionPathLength",
        stripped.value().size());
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
    if (!base::ContainsKey(filesystem_permissions_, filesystem_id))
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
        return base::ContainsKey(request_file_set_, path);
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

  bool CanAccessDataForOrigin(const GURL& site_url) {
    if (origin_lock_.is_empty())
      return true;
    return origin_lock_ == site_url;
  }

  void LockToOrigin(const GURL& gurl) {
    origin_lock_ = gurl;
  }

  const GURL& origin_lock() { return origin_lock_; }

  ChildProcessSecurityPolicyImpl::CheckOriginLockResult CheckOriginLock(
      const GURL& gurl) {
    if (origin_lock_.is_empty())
      return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::NO_LOCK;

    if (origin_lock_ == gurl) {
      return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::
          HAS_EQUAL_LOCK;
    }

    return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::
        HAS_WRONG_LOCK;
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

  // The set of isolated filesystems the child process is permitted to access.
  FileSystemMap filesystem_permissions_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicyImpl::ChildProcessSecurityPolicyImpl() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(url::kHttpScheme);
  RegisterWebSafeScheme(url::kHttpsScheme);
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

void ChildProcessSecurityPolicyImpl::Add(int child_id) {
  base::AutoLock lock(lock_);
  AddChild(child_id);
}

void ChildProcessSecurityPolicyImpl::AddWorker(int child_id,
                                               int main_render_process_id) {
  base::AutoLock lock(lock_);
  AddChild(child_id);
  worker_map_[child_id] = main_render_process_id;
}

void ChildProcessSecurityPolicyImpl::Remove(int child_id) {
  base::AutoLock lock(lock_);
  security_state_.erase(child_id);
  worker_map_.erase(child_id);
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

  return base::ContainsKey(schemes_okay_to_request_in_any_process_, scheme);
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

  return base::ContainsKey(pseudo_schemes_, scheme);
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
    return url.IsAboutBlank() || url == kAboutSrcDocURL;

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
  return !GetContentClient()->browser()->IsHandledURL(url) &&
         !net::URLRequest::IsHandledURL(url);
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
                                                  const GURL& url,
                                                  bool check_origin_locks) {
  if (!url.is_valid())
    return false;  // Can't commit invalid URLs.

  const std::string& scheme = url.scheme();

  // Of all the pseudo schemes, only about:blank and about:srcdoc are allowed to
  // commit.
  if (IsPseudoScheme(scheme))
    return url == url::kAboutBlankURL || url == kAboutSrcDocURL;

  // Blob and filesystem URLs require special treatment; validate the inner
  // origin they embed.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return false;

    url::Origin origin = url::Origin::Create(url);
    return origin.opaque() ||
           CanCommitURL(child_id, GURL(origin.Serialize()), check_origin_locks);
  }

  // With site isolation, a URL from a site may only be committed in a process
  // dedicated to that site.  This check will ensure that |url| can't commit if
  // the process is locked to a different site.  Note that this check is only
  // effective for processes that are locked to a site, but even with strict
  // site isolation, currently not all processes are locked (e.g., extensions
  // or <webview> tags - see ShouldLockToOrigin()).
  if (check_origin_locks && !CanAccessDataForOrigin(child_id, url))
    return false;

  {
    base::AutoLock lock(lock_);

    // Most schemes can commit in any process. Note that we check
    // schemes_okay_to_commit_in_any_process_ here, which is stricter than
    // IsWebSafeScheme().
    //
    // TODO(creis, nick): https://crbug.com/515309: The line below does not
    // enforce that http pages cannot commit in an extension process.
    if (base::ContainsKey(schemes_okay_to_commit_in_any_process_, scheme))
      return true;

    auto state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to commit the URL.
    return state->second->CanCommitURL(url);
  }
}

bool ChildProcessSecurityPolicyImpl::CanCommitURL(int child_id,
                                                  const GURL& url) {
  return CanCommitURL(child_id, url, true /* check_origin_lock */);
}

bool ChildProcessSecurityPolicyImpl::CanSetAsOriginHeader(int child_id,
                                                          const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't set invalid URLs as origin headers.

  // about:srcdoc cannot be used as an origin
  if (url == kAboutSrcDocURL)
    return false;

  // If this process can commit |url|, it can use |url| as an origin for
  // outbound requests.
  //
  // TODO(alexmos): This should eventually also check the origin lock, but
  // currently this is not done due to certain corner cases involving HTML
  // imports and layout tests that simulate requests from isolated worlds.  See
  // https://crbug.com/515309.
  if (CanCommitURL(child_id, url, false /* check_origin_lock */))
    return true;

  // Allow schemes which may come from scripts executing in isolated worlds;
  // XHRs issued by such scripts reflect the script origin rather than the
  // document origin.
  {
    base::AutoLock lock(lock_);
    if (base::ContainsKey(schemes_okay_to_appear_as_origin_headers_,
                          url.scheme())) {
      return true;
    }
  }
  return false;
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
      case network::DataElement::TYPE_FILE:
        if (!CanReadFile(child_id, element.path()))
          return false;
        break;

      case network::DataElement::TYPE_BYTES:
        // Data is self-contained within |body| - no need to check access.
        break;

      case network::DataElement::TYPE_BLOB:
        // No need to validate - the unguessability of the uuid of the blob is a
        // sufficient defense against access from an unrelated renderer.
        break;

      case network::DataElement::TYPE_DATA_PIPE:
        // Data is self-contained within |body| - no need to check access.
        break;

      case network::DataElement::TYPE_UNKNOWN:
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
  bool result = ChildProcessHasPermissionsForFile(child_id, file, permissions);
  if (!result) {
    // If this is a worker thread that has no access to a given file,
    // let's check that its renderer process has access to that file instead.
    auto iter = worker_map_.find(child_id);
    if (iter != worker_map_.end() && iter->second != 0) {
      result = ChildProcessHasPermissionsForFile(iter->second,
                                                 file,
                                                 permissions);
    }
  }
  return result;
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
  if (!CanCommitURL(child_id, filesystem_url.origin())) {
    UMA_HISTOGRAM_BOOLEAN("FileSystem.OriginFailedCanCommitURL", true);
    return false;
  }

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

void ChildProcessSecurityPolicyImpl::AddChild(int child_id) {
  if (security_state_.count(child_id) != 0) {
    NOTREACHED() << "Add child process at most once.";
    return;
  }

  security_state_[child_id] = std::make_unique<SecurityState>();
}

bool ChildProcessSecurityPolicyImpl::ChildProcessHasPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFile(file, permissions);
}

bool ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(int child_id,
                                                            const GURL& url) {
  // It's important to call DetermineProcessLockURL before
  // acquiring |lock_|, since DetermineProcessLockURL consults
  // IsIsolatedOrigin, which needs to grab the same lock.
  GURL expected_process_lock =
      SiteInstanceImpl::DetermineProcessLockURL(nullptr, url);

  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  if (state == security_state_.end()) {
    // TODO(nick): Returning true instead of false here is a temporary
    // workaround for https://crbug.com/600441
    return true;
  }
  bool can_access =
      state->second->CanAccessDataForOrigin(expected_process_lock);
  if (!can_access) {
    // Returning false here will result in a renderer kill.  Set some crash
    // keys that will help understand the circumstances of that kill.
    base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                   expected_process_lock.spec());
    base::debug::SetCrashKeyString(bad_message::GetKilledProcessOriginLockKey(),
                                   state->second->origin_lock().spec());

    static auto* requested_origin_key = base::debug::AllocateCrashKeyString(
        "requested_origin", base::debug::CrashKeySize::Size64);
    base::debug::SetCrashKeyString(requested_origin_key,
                                   url.GetOrigin().spec());
  }
  return can_access;
}

void ChildProcessSecurityPolicyImpl::LockToOrigin(int child_id,
                                                  const GURL& gurl) {
  // LockToOrigin should only be called on the UI thread (OTOH, it is okay to
  // call GetOriginLock or CheckOriginLock from any thread).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if DCHECK_IS_ON()
  // Sanity-check that the |gurl| argument can be used as a lock.
  RenderProcessHost* rph = RenderProcessHostImpl::FromID(child_id);
  if (rph) {  // |rph| can be null in unittests.
    DCHECK_EQ(SiteInstanceImpl::DetermineProcessLockURL(
                  rph->GetBrowserContext(), gurl),
              gurl);
  }
#endif

  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  DCHECK(state != security_state_.end());
  state->second->LockToOrigin(gurl);
}

ChildProcessSecurityPolicyImpl::CheckOriginLockResult
ChildProcessSecurityPolicyImpl::CheckOriginLock(int child_id,
                                                const GURL& site_url) {
  base::AutoLock lock(lock_);
  auto state = security_state_.find(child_id);
  if (state == security_state_.end())
    return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::NO_LOCK;
  return state->second->CheckOriginLock(site_url);
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
    std::vector<url::Origin> origins_to_add) {
  // Filter out origins that cannot be used as an isolated origin.
  base::EraseIf(origins_to_add, [](const url::Origin& origin) {
    if (IsolatedOriginUtil::IsValidIsolatedOrigin(origin))
      return false;  // Don't remove.

    LOG(ERROR) << "Invalid isolated origin: " << origin;
    return true;  // Remove.
  });

  base::AutoLock lock(lock_);
  for (const url::Origin& origin : origins_to_add) {
    // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
    // up the isolated origin lookup.  This only performs a straightforward
    // translation of an origin to eTLD+1; it does *not* take into account
    // effective URLs, isolated origins, and other logic that's not needed
    // here, but *is* typically needed for making process model decisions. Be
    // very careful about using GetSiteForOrigin() elsewhere, and consider
    // whether you should be using GetSiteForURL() instead.
    GURL key(SiteInstanceImpl::GetSiteForOrigin(origin));
    isolated_origins_[key].insert(origin);
  }
}

bool ChildProcessSecurityPolicyImpl::IsIsolatedOrigin(
    const url::Origin& origin) {
  url::Origin unused_result;
  return GetMatchingIsolatedOrigin(origin, &unused_result);
}

bool ChildProcessSecurityPolicyImpl::GetMatchingIsolatedOrigin(
    const url::Origin& origin,
    url::Origin* result) {
  // GetSiteForOrigin() is used to look up the site URL of |origin| to speed
  // up the isolated origin lookup.  This only performs a straightforward
  // translation of an origin to eTLD+1; it does *not* take into account
  // effective URLs, isolated origins, and other logic that's not needed
  // here, but *is* typically needed for making process model decisions. Be
  // very careful about using GetSiteForOrigin() elsewhere, and consider
  // whether you should be using GetSiteForURL() instead.
  return GetMatchingIsolatedOrigin(
      origin, SiteInstanceImpl::GetSiteForOrigin(origin), result);
}

bool ChildProcessSecurityPolicyImpl::GetMatchingIsolatedOrigin(
    const url::Origin& origin,
    const GURL& site_url,
    url::Origin* result) {
  *result = url::Origin();
  base::AutoLock lock(lock_);

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

  // If multiple isolated origins are registered with a common domain suffix,
  // return the most specific one.  For example, if foo.isolated.com and
  // isolated.com are both isolated origins, bar.foo.isolated.com should
  // return foo.isolated.com.
  bool found = false;
  if (it != isolated_origins_.end()) {
    for (const url::Origin& isolated_origin : it->second) {
      if (IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(origin,
                                                            isolated_origin)) {
        if (!found ||
            result->host().length() < isolated_origin.host().length()) {
          *result = isolated_origin;
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
  base::AutoLock lock(lock_);
  isolated_origins_[key].erase(origin);
  if (isolated_origins_[key].empty())
    isolated_origins_.erase(key);
}

}  // namespace content
