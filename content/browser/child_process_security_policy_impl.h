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
#include "content/browser/can_commit_status.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/isolation_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/resource_type.h"
#include "storage/common/file_system/file_system_types.h"
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
}  // namespace storage

namespace content {

class BrowserContext;
class IsolationContext;
class ResourceContext;
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
  void AddIsolatedOrigins(base::StringPiece origins_list,
                          IsolatedOriginSource source,
                          BrowserContext* browser_context = nullptr) override;
  void AddIsolatedOrigins(const std::vector<url::Origin>& origins,
                          IsolatedOriginSource source,
                          BrowserContext* browser_context = nullptr) override;
  bool IsGloballyIsolatedOriginForTesting(const url::Origin& origin) override;
  std::vector<url::Origin> GetIsolatedOrigins(
      base::Optional<IsolatedOriginSource> source = base::nullopt,
      BrowserContext* browser_context = nullptr) override;

  // Identical to the above method, but takes url::Origin as input.
  bool CanAccessDataForOrigin(int child_id, const url::Origin& origin);

  // Determines if the combination of |origin| & |url| is safe to commit to
  // the process associated with |child_id|.
  //
  // Returns CAN_COMMIT_ORIGIN_AND_URL if it is safe to commit the |origin| and
  // |url| combination to the process associated with |child_id|.
  // Returns CANNOT_COMMIT_URL if |url| is not safe to commit.
  // Returns CANNOT_COMMIT_ORIGIN if |origin| is not safe to commit.
  CanCommitStatus CanCommitOriginAndUrl(
      int child_id,
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      const GURL& url);

  // This function will check whether |origin| requires process isolation
  // within |isolation_context|, and if so, it will return true and put the
  // most specific matching isolated origin into |result|.
  //
  // Such origins may be registered with the --isolate-origins command-line
  // flag, via features::IsolateOrigins, via an IsolateOrigins enterprise
  // policy, or by a content/ embedder using
  // ContentBrowserClient::GetOriginsRequiringDedicatedProcess().
  //
  // If |origin| does not require process isolation, this function will return
  // false, and |result| will be a unique origin. This means that neither
  // |origin|, nor any origins for which |origin| is a subdomain, have been
  // registered as isolated origins.
  //
  // For example, if both https://isolated.com/ and
  // https://bar.foo.isolated.com/ are registered as isolated origins, then the
  // values returned in |result| are:
  //   https://isolated.com/             -->  https://isolated.com/
  //   https://foo.isolated.com/         -->  https://isolated.com/
  //   https://bar.foo.isolated.com/     -->  https://bar.foo.isolated.com/
  //   https://baz.bar.foo.isolated.com/ -->  https://bar.foo.isolated.com/
  //   https://unisolated.com/           -->  (unique origin)
  //
  // |isolation_context| is used to determine which origins are isolated in
  // this context.  For example, isolated origins that are dynamically added
  // will only affect future BrowsingInstances.
  bool GetMatchingIsolatedOrigin(const IsolationContext& isolation_context,
                                 const url::Origin& origin,
                                 url::Origin* result);

  // A version of GetMatchingIsolatedOrigin that takes in both the |origin| and
  // the |site_url| that |origin| corresponds to.  |site_url| is the key by
  // which |origin| will be looked up in |isolated_origins_| within
  // |isolation_context|; this function allows it to be passed in when it is
  // already known to avoid recomputing it internally.
  bool GetMatchingIsolatedOrigin(const IsolationContext& isolation_context,
                                 const url::Origin& origin,
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
  // this method exactly once. This call must be made on the UI thread.
  void Add(int child_id, BrowserContext* browser_context);

  // Upon destruction, child processes should unregister themselves by calling
  // this method exactly once. This call must be made on the UI thread.
  //
  // Note: Pre-Remove() permissions remain in effect on the IO thread until
  // the task posted to the IO thread by this call runs and removes the entry
  // from |pending_remove_state_|.
  // This UI -> IO task sequence ensures that any pending tasks, on the IO
  // thread, for this |child_id| are allowed to run before access is completely
  // revoked.
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

  // Notifies security state of |child_id| about the IsolationContext it will
  // host.  The main side effect is proper setting of the lowest
  // BrowsingInstanceId associated with the security state.
  void IncludeIsolationContext(int child_id,
                               const IsolationContext& isolation_context);

  // Sets the process identified by |child_id| as only permitted to access data
  // for the origin specified by |lock_url|. Most callers should use
  // RenderProcessHostImpl::LockToOrigin instead of calling this directly.
  // |isolation_context| provides the context, such as BrowsingInstance, from
  // which this process was locked to origin.  This information is used when
  // making isolation decisions for this process, such as determining which
  // isolated origins pertain to it.
  void LockToOrigin(const IsolationContext& isolation_context,
                    int child_id,
                    const GURL& lock_url);

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

  // Remove all isolated origins associated with |browser_context|.  This is
  // typically used when |browser_context| is being destroyed and assumes that
  // no processes are running or will run for that profile; this makes the
  // isolated origin removal safe.  Note that |browser_context| cannot be null;
  // i.e., isolated origins that apply globally to all profiles cannot
  // currently be removed, since that is not safe to do at runtime.
  void RemoveIsolatedOriginsForBrowserContext(
      const BrowserContext& browser_context);

  // Check whether |origin| requires origin-wide process isolation within
  // |isolation_context|.
  //
  // Subdomains of an isolated origin are considered part of that isolated
  // origin.  Thus, if https://isolated.foo.com/ had been added as an isolated
  // origin, this will return true for https://isolated.foo.com/,
  // https://bar.isolated.foo.com/, or https://baz.bar.isolated.foo.com/; and
  // it will return false for https://foo.com/ or https://unisolated.foo.com/.
  //
  // |isolation_context| is used to determine which origins are isolated in
  // this context.  For example, isolated origins that are dynamically added
  // will only affect future BrowsingInstances.
  bool IsIsolatedOrigin(const IsolationContext& isolation_context,
                        const url::Origin& origin);

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

  // Returns true if the policy object has security state information for
  // |child_id|. This is essentially a way to determine if the policy object
  // is actively tracking permissions for |child_id|. This method can be called
  // from the UI & IO threads.
  //
  // DO NOT ADD NEW CALLERS OF THIS METHOD.
  // TODO(933089): Remove this method once a better long term solution is
  // implemented for the one caller doing Blob URL revocation.
  bool HasSecurityState(int child_id);

  // Sets "killed_process_origin_lock" crash key with lock info for the
  // process associated with |child_id|.
  void LogKilledProcessOriginLock(int child_id);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  friend class ChildProcessSecurityPolicyTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, FilePermissions);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, AddIsolatedOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           DynamicIsolatedOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsForSpecificBrowserContexts);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsRemovedWhenBrowserContextDestroyed);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolateAllSuborigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           WildcardAndNonWildcardOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           WildcardAndNonWildcardEmbedded);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           ParseIsolatedOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, WildcardDefaultPort);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, std::unique_ptr<SecurityState>> SecurityStateMap;
  typedef std::map<storage::FileSystemType, int> FileSystemPermissionPolicyMap;

  // This class holds an isolated origin along with information such as which
  // BrowsingInstances and profile it applies to.  See |isolated_origins_|
  // below for more details.
  class CONTENT_EXPORT IsolatedOriginEntry {
   public:
    IsolatedOriginEntry(const url::Origin& origin,
                        BrowsingInstanceId min_browsing_instance_id,
                        BrowserContext* browser_context,
                        ResourceContext* resource_context,
                        bool isolate_all_subdomains,
                        IsolatedOriginSource source);
    // Copyable and movable.
    IsolatedOriginEntry(const IsolatedOriginEntry& other);
    IsolatedOriginEntry& operator=(const IsolatedOriginEntry& other);
    IsolatedOriginEntry(IsolatedOriginEntry&& other);
    IsolatedOriginEntry& operator=(IsolatedOriginEntry&& other);
    ~IsolatedOriginEntry();

    // Allow this class to be used as a key in STL.
    bool operator<(const IsolatedOriginEntry& other) const {
      return std::tie(origin_, min_browsing_instance_id_, browser_context_,
                      resource_context_, isolate_all_subdomains_, source_) <
             std::tie(other.origin_, other.min_browsing_instance_id_,
                      other.browser_context_, other.resource_context_,
                      other.isolate_all_subdomains_, source_);
    }

    bool operator==(const IsolatedOriginEntry& other) const {
      return origin_ == other.origin_ &&
             min_browsing_instance_id_ == other.min_browsing_instance_id_ &&
             browser_context_ == other.browser_context_ &&
             resource_context_ == other.resource_context_ &&
             isolate_all_subdomains_ == other.isolate_all_subdomains_ &&
             source_ == other.source_;
    }

    // True if this isolated origin applies globally to all profiles.
    bool AppliesToAllBrowserContexts() const;

    // True if (1) this entry is associated with the same profile as
    // |browser_or_resource_context|, or (2) this entry applies to all
    // profiles.  May be used on UI or IO threads.
    bool MatchesProfile(
        const BrowserOrResourceContext& browser_or_resource_context) const;

    const url::Origin& origin() const { return origin_; }

    BrowsingInstanceId min_browsing_instance_id() const {
      return min_browsing_instance_id_;
    }

    const BrowserContext* browser_context() const { return browser_context_; }

    bool isolate_all_subdomains() const { return isolate_all_subdomains_; }

    IsolatedOriginSource source() const { return source_; }

   private:
    url::Origin origin_;
    BrowsingInstanceId min_browsing_instance_id_;

    // Optional information about the profile where the isolated origin
    // applies.  |browser_context_| may be used on the UI thread, and
    // |resource_context_| may be used on the IO thread.  If these are null,
    // then the isolated origin applies globally to all profiles.
    BrowserContext* browser_context_;
    ResourceContext* resource_context_;

    // True if origins at this or lower level should be treated as distinct
    // isolated origins, effectively isolating all domains below a given domain,
    // e.g. if the origin is https://foo.com and isolate_all_subdomains_ is
    // true, then https://bar.foo.com, https://qux.bar.foo.com and all
    // subdomains of the form https://<<any pattern here>>.foo.com are
    // considered isolated origins.
    bool isolate_all_subdomains_;

    // This tracks the source of each isolated origin entry, e.g., to
    // distinguish those that should be displayed to the user from those that
    // should not.  See https://crbug.com/920911.
    IsolatedOriginSource source_;
  };

  // Obtain an instance of ChildProcessSecurityPolicyImpl via GetInstance().
  ChildProcessSecurityPolicyImpl();
  friend struct base::DefaultSingletonTraits<ChildProcessSecurityPolicyImpl>;

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
  void GrantPermissionsForFileSystem(int child_id,
                                     const std::string& filesystem_id,
                                     int permission);

  // Determines if certain permissions were granted for a file. |permissions|
  // is an internally defined bit-set.
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
  bool HasPermissionsForFileSystem(int child_id,
                                   const std::string& filesystem_id,
                                   int permission);

  // Gets the SecurityState object associated with |child_id|.
  // Note: Returned object is only valid for the duration the caller holds
  // |lock_|.
  SecurityState* GetSecurityState(int child_id) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Convert a list of comma separated isolated origins in |pattern_list|,
  // specified either as wildcard origins, non-wildcard origins or a mix of the
  // two into IsolatedOriginPatterns, suitable for addition via
  // AddIsolatedOrigins().
  static std::vector<IsolatedOriginPattern> ParseIsolatedOrigins(
      base::StringPiece pattern_list);

  void AddIsolatedOrigins(const std::vector<IsolatedOriginPattern>& patterns,
                          IsolatedOriginSource source,
                          BrowserContext* browser_context = nullptr);

  // Creates the value to place in the "killed_process_origin_lock" crash key
  // based on the contents of |security_state|.
  static std::string GetKilledProcessOriginLock(
      const SecurityState* security_state);

  // You must acquire this lock before reading or writing any members of this
  // class, except for isolated_origins_ which uses its own lock.  You must not
  // block while holding this lock.
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

  // This map holds the SecurityState for a child process after Remove()
  // is called on the UI thread. An entry stays in this map until a task has
  // run on the IO thread. This is necessary to provide consistent security
  // decisions and avoid races between the UI & IO threads during child process
  // shutdown. This separate map is used to preserve SecurityState info AND
  // preventing mutation of that state after Remove() is called.
  SecurityStateMap pending_remove_state_ GUARDED_BY(lock_);

  FileSystemPermissionPolicyMap file_system_policy_map_ GUARDED_BY(lock_);

  // You must acquire this lock before reading or writing isolated_origins_.
  // You must not block while holding this lock.
  //
  // It is allowed to hold both |lock_| and |isolated_origins_lock_|, but in
  // this case, |lock_| should always be acquired first to prevent deadlock.
  base::Lock isolated_origins_lock_ ACQUIRED_AFTER(lock_);

  // Tracks origins for which the entire origin should be treated as a site
  // when making process model decisions, rather than the origin's scheme and
  // eTLD+1. Each of these origins requires a dedicated process.  This set is
  // protected by |isolated_origins_lock_|.
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
  //
  // Each origin entry stores information about:
  //   1. Which BrowsingInstances it applies to, in the form of a minimum
  //      BrowsingInstance ID.  This is looked up at the time the isolated
  //      origin is added.  The isolated origin will apply only to future
  //      BrowsingInstances, which will have IDs equal to or greater than the
  //      threshold ID (called |min_browsing_instance_id|) in each origin's
  //      IsolatedOriginEntry.
  //   2. Optionally, which BrowserContext (profile) it applies to.  When the
  //      |browser_context| field in the IsolatedOriginEntry is non-null, a
  //      particular isolated origin entry only applies to that BrowserContext.
  //      A ResourceContext, BrowserContext's representation on the IO thread,
  //      is also stored in the entry to facilitate checks on the IO thread.
  //      Note that the same origin may be isolated in different profiles,
  //      possibly with different BrowsingInstance ID cut-offs.  For example:
  //        https://foo.com -> { [https://test.foo.com profile1 4],
  //                             [https://test.foo.com profile2 7] }
  //      represents https://test.foo.com being isolated in profile1 starting
  //      with BrowsingInstance ID 4, and also in profile2 starting with
  //      BrowsingInstance ID 7.
  base::flat_map<GURL, std::vector<IsolatedOriginEntry>> isolated_origins_
      GUARDED_BY(isolated_origins_lock_);

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
