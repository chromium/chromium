// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
#define CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/resource_type.h"
#include "storage/common/fileapi/file_system_types.h"
#include "url/origin.h"

class GURL;

namespace base {
class FilePath;
}

namespace network {
class ResourceRequestBody;
}

namespace storage {
class FileSystemContext;
class FileSystemURL;
}

namespace content {

class SiteInstance;

class CONTENT_EXPORT ChildProcessSecurityPolicyImpl
    : public ChildProcessSecurityPolicy {
 public:
  // Object can only be created through GetInstance() so the constructor is
  // private.
  ~ChildProcessSecurityPolicyImpl() override;

  static ChildProcessSecurityPolicyImpl* GetInstance();

  // ChildProcessSecurityPolicy implementation.
  void RegisterWebSafeScheme(const std::string& scheme) override;
  void RegisterWebSafeIsolatedScheme(
      const std::string& scheme,
      bool always_allow_in_origin_headers) override;
  bool IsWebSafeScheme(const std::string& scheme) override;
  void GrantReadFile(int child_id, const base::FilePath& file) override;
  void GrantCreateReadWriteFile(int child_id,
                                const base::FilePath& file) override;
  void GrantCopyInto(int child_id, const base::FilePath& dir) override;
  void GrantDeleteFrom(int child_id, const base::FilePath& dir) override;
  void GrantReadFileSystem(int child_id,
                           const std::string& filesystem_id) override;
  void GrantWriteFileSystem(int child_id,
                            const std::string& filesystem_id) override;
  void GrantCreateFileForFileSystem(int child_id,
                                    const std::string& filesystem_id) override;
  void GrantCreateReadWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) override;
  void GrantCopyIntoFileSystem(int child_id,
                               const std::string& filesystem_id) override;
  void GrantDeleteFromFileSystem(int child_id,
                                 const std::string& filesystem_id) override;
  void GrantCommitOrigin(int child_id, const url::Origin& origin) override;
  void GrantRequestOrigin(int child_id, const url::Origin& origin) override;
  void GrantRequestScheme(int child_id, const std::string& scheme) override;
  bool CanRequestURL(int child_id, const GURL& url) override;
  bool CanCommitURL(int child_id, const GURL& url) override;
  bool CanReadFile(int child_id, const base::FilePath& file) override;
  bool CanCreateReadWriteFile(int child_id,
                              const base::FilePath& file) override;
  bool CanReadFileSystem(int child_id,
                         const std::string& filesystem_id) override;
  bool CanReadWriteFileSystem(int child_id,
                              const std::string& filesystem_id) override;
  bool CanCopyIntoFileSystem(int child_id,
                             const std::string& filesystem_id) override;
  bool CanDeleteFromFileSystem(int child_id,
                               const std::string& filesystem_id) override;
  bool HasWebUIBindings(int child_id) override;
  void GrantSendMidiSysExMessage(int child_id) override;
  bool CanAccessDataForOrigin(int child_id, const GURL& url) override;
  bool GetMatchingIsolatedOrigin(const url::Origin& origin,
                                 url::Origin* result) override;

  // A version of GetMatchingIsolatedOrigin that takes in both the |origin| and
  // the |site_url| that |origin| corresponds to.  |site_url| is the key by
  // which |origin| will be looked up in |isolated_origins_|; this function
  // allows it to be passed in when it is already known to avoid recomputing it
  // internally.
  bool GetMatchingIsolatedOrigin(const url::Origin& origin,
                                 const GURL& site_url,
                                 url::Origin* result);

  // Returns if |child_id| can read all of the |files|.
  bool CanReadAllFiles(int child_id, const std::vector<base::FilePath>& files);

  // Validate that |child_id| in |file_system_context| is allowed to access
  // data in the POST body specified by |body|.  Can be called on any thread.
  bool CanReadRequestBody(
      int child_id,
      const storage::FileSystemContext* file_system_context,
      const scoped_refptr<network::ResourceRequestBody>& body);

  // Validate that the renderer process for |site_instance| is allowed to access
  // data in the POST body specified by |body|.  Has to be called on the UI
  // thread.
  bool CanReadRequestBody(
      SiteInstance* site_instance,
      const scoped_refptr<network::ResourceRequestBody>& body);

  // Pseudo schemes are treated differently than other schemes because they
  // cannot be requested like normal URLs.  There is no mechanism for revoking
  // pseudo schemes.
  void RegisterPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as pseudo scheme.
  bool IsPseudoScheme(const std::string& scheme);

  // Upon creation, child processes should register themselves by calling this
  // this method exactly once.
  void Add(int child_id);

  // Upon creation, worker thread child processes should register themselves by
  // calling this this method exactly once. Workers that are not shared will
  // inherit permissions from their parent renderer process identified with
  // |main_render_process_id|.
  void AddWorker(int worker_child_id, int main_render_process_id);

  // Upon destruction, child processess should unregister themselves by caling
  // this method exactly once.
  void Remove(int child_id);

  // Whenever the browser processes commands the child process to commit a URL,
  // it should call this method to grant the child process the capability to
  // commit anything from the URL's origin, along with permission to request all
  // URLs of the same scheme.
  void GrantCommitURL(int child_id, const GURL& url);

  // Whenever the browser process drops a file icon on a tab, it should call
  // this method to grant the child process the capability to request this one
  // file:// URL, but not all urls of the file:// scheme.
  void GrantRequestSpecificFileURL(int child_id, const GURL& url);

  // Revokes all permissions granted to the given file.
  void RevokeAllPermissionsForFile(int child_id, const base::FilePath& file);

  // Grant the child process the ability to use Web UI Bindings where |bindings|
  // is either BINDINGS_POLICY_WEB_UI or BINDINGS_POLICY_MOJO_WEB_UI or both.
  void GrantWebUIBindings(int child_id, int bindings);

  // Grant the child process the ability to read raw cookies.
  void GrantReadRawCookies(int child_id);

  // Revoke read raw cookies permission.
  void RevokeReadRawCookies(int child_id);

  // A version of the public ChildProcessSecurityPolicy::CanCommitURL() which
  // takes an additional bool |check_origin_lock|, specifying whether to
  // reject |url| if it does not match the origin lock on process |child_id|.
  // Passing true for |check_origin_lock| provides stronger enforcement with
  // strict site isolation; it is only set to false by features (e.g., Origin
  // header validation) that aren't yet ready for this enforcement. This
  // function should *not* be used by new features; use the public
  // ChildProcessSecurityPolicy::CanCommitURL() instead, which internally calls
  // this with |check_origin_lock| being true.
  //
  // TODO(alexmos): Remove |check_origin_lock| and check origin locks
  // unconditionally once https://crbug.com/515309 is fixed.
  bool CanCommitURL(int child_id, const GURL& url, bool check_origin_lock);

  // Whether the given origin is valid for an origin header. Valid origin
  // headers are commitable URLs.
  bool CanSetAsOriginHeader(int child_id, const GURL& url);

  // Explicit permissions checks for FileSystemURL specified files.
  bool CanReadFileSystemFile(int child_id,
                             const storage::FileSystemURL& filesystem_url);
  bool CanWriteFileSystemFile(int child_id,
                              const storage::FileSystemURL& filesystem_url);
  bool CanCreateFileSystemFile(int child_id,
                               const storage::FileSystemURL& filesystem_url);
  bool CanCreateReadWriteFileSystemFile(
      int child_id,
      const storage::FileSystemURL& filesystem_url);
  bool CanCopyIntoFileSystemFile(int child_id,
                                 const storage::FileSystemURL& filesystem_url);
  bool CanDeleteFileSystemFile(int child_id,
                               const storage::FileSystemURL& filesystem_url);

  // Returns true if the specified child_id has been granted ReadRawCookies.
  bool CanReadRawCookies(int child_id);

  // Sets the process as only permitted to use and see the cookies for the
  // given origin. Most callers should use RenderProcessHostImpl::LockToOrigin
  // instead of calling this directly.
  void LockToOrigin(int child_id, const GURL& lock_url);

  // Used to indicate the result of comparing a process's origin lock to
  // another value:
  enum class CheckOriginLockResult {
    // The process does not exist, or it has no origin lock.
    NO_LOCK,
    // The process has an origin lock and it matches the passed-in value.
    HAS_EQUAL_LOCK,
    // The process has an origin lock and it does not match the passed-in
    // value.
    HAS_WRONG_LOCK,
  };

  // Check the origin lock of the process specified by |child_id| against
  // |site_url|.  See the definition of |CheckOriginLockResult| for possible
  // returned values.
  CheckOriginLockResult CheckOriginLock(int child_id, const GURL& site_url);

  // Retrieves the current origin lock of process |child_id|.  Returns an empty
  // GURL if the process does not exist or if it is not locked to an origin.
  GURL GetOriginLock(int child_id);

  // Register FileSystem type and permission policy which should be used
  // for the type.  The |policy| must be a bitwise-or'd value of
  // storage::FilePermissionPolicy.
  void RegisterFileSystemPermissionPolicy(storage::FileSystemType type,
                                          int policy);

  // Returns true if sending system exclusive messages is allowed.
  bool CanSendMidiSysExMessage(int child_id);

  // Add |origins| to the list of origins that require process isolation.
  // When making process model decisions for such origins, the full
  // scheme+host+port tuple rather than scheme and eTLD+1 will be used.
  // SiteInstances for these origins will also use the full origin as site URL.
  //
  // Subdomains of an isolated origin are considered to be part of that
  // origin's site.  For example, if https://isolated.foo.com is added as an
  // isolated origin, then https://bar.isolated.foo.com will be considered part
  // of the site for https://isolated.foo.com.
  //
  // Note that origins from |origins| must not be unique - URLs that render with
  // unique origins, such as data: URLs, are not supported. Non-standard
  // schemes are also not supported.  Sandboxed frames (e.g., <iframe sandbox>)
  // *are* supported, since process placement decisions will be based on the
  // URLs such frames navigate to, and not the origin of committed documents
  // (which might be unique).  If an isolated origin opens an about:blank
  // popup, it will stay in the isolated origin's process. Nested URLs
  // (filesystem: and blob:) retain process isolation behavior of their inner
  // origin.
  //
  // Note that it is okay if |origins| contains duplicates - the set of origins
  // will be deduplicated inside the method.
  void AddIsolatedOrigins(std::vector<url::Origin> origins);

  // Check whether |origin| requires origin-wide process isolation.
  //
  // Subdomains of an isolated origin are considered part of that isolated
  // origin.  Thus, if https://isolated.foo.com/ had been added as an isolated
  // origin, this will return true for https://isolated.foo.com/,
  // https://bar.isolated.foo.com/, or https://baz.bar.isolated.foo.com/; and
  // it will return false for https://foo.com/ or https://unisolated.foo.com/.
  //
  // Note that unlike site URLs for regular web sites, isolated origins care
  // about port.
  bool IsIsolatedOrigin(const url::Origin& origin);

  // Removes a previously added isolated origin, currently only used in tests.
  //
  // TODO(alexmos): Exposing this more generally will require extra care, such
  // as ensuring that there are no active SiteInstances in that origin.
  void RemoveIsolatedOriginForTesting(const url::Origin& origin);

  // Returns false for redirects that must be blocked no matter which renderer
  // process initiated the request (if any).
  // Note: Checking CanRedirectToURL is not enough. CanRequestURL(child_id, url)
  //       represents a stricter subset. It must also be used for
  //       renderer-initiated navigations.
  bool CanRedirectToURL(const GURL& url);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  friend class ChildProcessSecurityPolicyTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, FilePermissions);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, AddIsolatedOrigins);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, std::unique_ptr<SecurityState>> SecurityStateMap;
  typedef std::map<int, int> WorkerToMainProcessMap;
  typedef std::map<storage::FileSystemType, int> FileSystemPermissionPolicyMap;

  // Obtain an instance of ChildProcessSecurityPolicyImpl via GetInstance().
  ChildProcessSecurityPolicyImpl();
  friend struct base::DefaultSingletonTraits<ChildProcessSecurityPolicyImpl>;

  // Adds child process during registration.
  void AddChild(int child_id) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Determines if certain permissions were granted for a file to given child
  // process. |permissions| is an internally defined bit-set.
  bool ChildProcessHasPermissionsForFile(int child_id,
                                         const base::FilePath& file,
                                         int permissions)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Grant a particular permission set for a file. |permissions| is an
  // internally defined bit-set.
  void GrantPermissionsForFile(int child_id,
                               const base::FilePath& file,
                               int permissions);

  // Grants access permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for
  // ChildProcessSecurityPolicy::GrantReadFileSystem() for more details.
  void GrantPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

  // Determines if certain permissions were granted for a file. |permissions|
  // is an internally defined bit-set. If |child_id| is a worker process,
  // this returns true if either the worker process or its parent renderer
  // has permissions for the file.
  bool HasPermissionsForFile(int child_id,
                             const base::FilePath& file,
                             int permissions);

  // Determines if certain permissions were granted for a file in FileSystem
  // API. |permissions| is an internally defined bit-set.
  bool HasPermissionsForFileSystemFile(
      int child_id,
      const storage::FileSystemURL& filesystem_url,
      int permissions);

  // Determines if certain permissions were granted for a file system.
  // |permissions| is an internally defined bit-set.
  bool HasPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  base::Lock lock_;

  // These schemes are white-listed for all child processes in various contexts.
  // These sets are protected by |lock_|.
  SchemeSet schemes_okay_to_commit_in_any_process_ GUARDED_BY(lock_);
  SchemeSet schemes_okay_to_request_in_any_process_ GUARDED_BY(lock_);
  SchemeSet schemes_okay_to_appear_as_origin_headers_ GUARDED_BY(lock_);

  // These schemes do not actually represent retrievable URLs.  For example,
  // the the URLs in the "about" scheme are aliases to other URLs.  This set is
  // protected by |lock_|.
  SchemeSet pseudo_schemes_ GUARDED_BY(lock_);

  // This map holds a SecurityState for each child process.  The key for the
  // map is the ID of the ChildProcessHost.  The SecurityState objects are
  // owned by this object and are protected by |lock_|.  References to them must
  // not escape this class.
  SecurityStateMap security_state_ GUARDED_BY(lock_);

  // This maps keeps the record of which js worker thread child process
  // corresponds to which main js thread child process.
  WorkerToMainProcessMap worker_map_ GUARDED_BY(lock_);

  FileSystemPermissionPolicyMap file_system_policy_map_ GUARDED_BY(lock_);

  // Tracks origins for which the entire origin should be treated as a site
  // when making process model decisions, rather than the origin's scheme and
  // eTLD+1. Each of these origins requires a dedicated process.  This set is
  // protected by |lock_|.
  //
  // The origins are stored in a map indexed by a site URL computed for each
  // origin.  For example, adding https://foo.com, https://bar.foo.com, and
  // https://www.bar.com would result in the following structure:
  //   https://foo.com -> { https://foo.com, https://bar.foo.com }
  //   https://bar.com -> { https://www.bar.com }
  // This organization speeds up lookups of isolated origins. The site can be
  // found in O(log n) time, and the corresponding list of origins to search
  // using the expensive DoesOriginMatchIsolatedOrigin() comparison is
  // typically small.
  base::flat_map<GURL, base::flat_set<url::Origin>> isolated_origins_
      GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
