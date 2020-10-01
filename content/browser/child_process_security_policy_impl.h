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
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/child_process_security_policy.h"
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

// ProcessLock is a core part of Site Isolation, which is used to determine
// which documents are allowed to load in a process and which site data the
// process is allowed to access, based on the SiteInfo principal. If a process
// has a ProcessLock in the "invalid" state, then no SiteInstances have been
// associated with the process and access should not be granted to anything.
// Once a process is associated with its first SiteInstance, it transitions to
// the "locked_to_site" or "allow_any_site" state depending on whether the
// SiteInstance requires the process to be locked to a specific site or not.
// If the SiteInstance does not require the process to be locked to a site, the
// process will transition to the "allow_any_site" state and will allow any
// site to commit in the process. Such a process can later be upgraded to the
// "locked_to_site" state if something later determines that the process should
// only allow access to a single site. Once the process is in the
// "locked_to_site" state, the process will not be able to access site data from
// other sites.
//
// ProcessLock is currently defined in terms of a single SiteInfo with a process
// lock URL, but it could be possible to define it in terms of multiple
// SiteInfos that are compatible with each other (e.g., multiple extensions
// sharing an extension process).
//
// TODO(wjmaclean): Move this into its own .h file.
class CONTENT_EXPORT ProcessLock {
 public:
  // Error page processes are locked to a special error URL, to avoid loading
  // real pages into the process.
  static ProcessLock CreateForErrorPage();

  // Create a lock that that represents a process that is associated with at
  // least one SiteInstance, but is not locked to a specific site. Any request
  // that wants to commit in this process must have COOP/COEP information that
  // matches the values used to create this lock.
  static ProcessLock CreateAllowAnySite(
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>&
          coop_coep_cross_origin_isolated_origin);

  ProcessLock();
  explicit ProcessLock(const SiteInfo& site_info);
  ProcessLock(const ProcessLock& rhs);
  ProcessLock& operator=(const ProcessLock& rhs);

  ~ProcessLock();

  // Returns true if no information has been set on the lock.
  bool is_invalid() const { return !site_info_.has_value(); }

  // Returns true if the process is locked, but it is not restricted to a
  // specific site. Any site is allowed to commit in the process as long as
  // the request's COOP/COEP information matches the info provided when
  // the lock was created.
  bool allows_any_site() const {
    return site_info_.has_value() && site_info_->process_lock_url().is_empty();
  }

  // Returns true if the lock is restricted to a specific site and requires
  // the request's COOP/COEP information to match the values provided when
  // the lock was created.
  bool is_locked_to_site() const {
    return site_info_.has_value() && !site_info_->process_lock_url().is_empty();
  }

  // Returns the url that corresponds to the SiteInfo the lock is used with. It
  // will always be the same as the site URL, except in cases where effective
  // urls are in use. Always empty if the SiteInfo uses the default site url.
  // TODO(wjmaclean): Delete this accessor once we get to the point where we can
  // safely just compare ProcessLocks directly.
  const GURL lock_url() const {
    return site_info_.has_value() ? site_info_->process_lock_url() : GURL();
  }

  // Returns whether this ProcessLock is specific to an origin rather than
  // including subdomains, such as due to opt-in origin isolation. This resolves
  // an ambiguity of whether a process with a lock_url() like
  // "https://foo.example" is allowed to include "https://sub.foo.example" or
  // not.
  bool is_origin_keyed() const {
    return site_info_.has_value() && site_info_->is_origin_keyed();
  }

  // Representing agent cluster's "cross-origin isolated" concept.
  // https://html.spec.whatwg.org/multipage/webappapis.html#dom-crossoriginisolated
  // This property is renderer process global because we ensure that a
  // renderer process host only cross-origin isolated agents or only
  // non-cross-origin isolated agents, not both.
  bool is_coop_coep_cross_origin_isolated() const {
    return site_info_.has_value() &&
           site_info_->is_coop_coep_cross_origin_isolated();
  }

  // If is_coop_coep_cross_origin_isolated() returns true, this returns the
  // origin shared across all top level frames in the renderer process.
  base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin() const {
    return site_info_.has_value()
               ? site_info_->coop_coep_cross_origin_isolated_origin()
               : base::nullopt;
  }

  // Returns whether lock_url() is at least at the granularity of a site (i.e.,
  // a scheme plus eTLD+1, like https://google.com).  Also returns true if the
  // lock is to a more specific origin (e.g., https://accounts.google.com), but
  // not if the lock is empty or applies to an entire scheme (e.g., file://).
  bool IsASiteOrOrigin() const;

  bool matches_scheme(const std::string& scheme) const {
    return scheme == lock_url().scheme();
  }

  // Returns true if lock_url() has an opaque origin.
  bool HasOpaqueOrigin() const;

  // Returns true if |origin| matches the lock's origin.
  bool MatchesOrigin(const url::Origin& origin) const;

  // Returns true if the COOP/COEP origin isolation information in this lock
  // is set and matches the information in |site_info|.
  bool IsCompatibleWithCoopCoepCrossOriginIsolation(
      const SiteInfo& site_info) const;

  bool operator==(const ProcessLock& rhs) const;
  bool operator!=(const ProcessLock& rhs) const;

  std::string ToString() const;

 private:
  // TODO(creis): Consider tracking multiple compatible SiteInfos in ProcessLock
  // (e.g., multiple extensions). This can better restrict what the process has
  // access to in cases that we don't currently use a ProcessLock.
  base::Optional<SiteInfo> site_info_;
};

class CONTENT_EXPORT ChildProcessSecurityPolicyImpl
    : public ChildProcessSecurityPolicy {
 public:
  // Handle used to access the security state for a specific process.
  //
  // Objects that require the security state to be preserved beyond the
  // lifetime of the RenderProcessHostImpl should hold an instance of this
  // object and use it to answer security policy questions. (e.g. Mojo services
  // created by RPHI that can receive calls after RPHI destruction). This
  // object should only be called on the UI and IO threads.
  //
  // Note: Some security methods, like CanAccessDataForOrigin(), require
  // information from the BrowserContext to make its decisions. These methods
  // will fall back to failsafe values if called after BrowserContext
  // destruction. Callers should be prepared to gracefully handle this or
  // ensure that they don't make any calls after BrowserContext destruction.
  class CONTENT_EXPORT Handle {
   public:
    Handle();
    Handle(Handle&&);
    Handle(const Handle&) = delete;
    ~Handle();

    Handle& operator=(const Handle&) = delete;
    Handle& operator=(Handle&&);

    // Create a new instance of Handle, holding another reference to the same
    // process ID as the current one.
    Handle Duplicate();

    // Returns true if this object has a valid process ID.
    // Returns false if this object was created with the default constructor,
    // the contents of this object was transferred to another Handle via
    // std::move(), or ChildProcessSecurityPolicyImpl::CreateHandle()
    // created this object after the process has already been destructed.
    bool is_valid() const;

    // Whether the process is allowed to commit a document from the given URL.
    bool CanCommitURL(const GURL& url);

    // Before servicing a child process's request to upload a file to the web,
    // the browser should call this method to determine whether the process has
    // the capability to upload the requested file.
    bool CanReadFile(const base::FilePath& file);

    // Explicit read permissions check for FileSystemURL specified files.
    bool CanReadFileSystemFile(const storage::FileSystemURL& url);

    // Returns true if the process is permitted to read and modify the data for
    // the origin of |url|. This is currently used to protect data such as
    // cookies, passwords, and local storage. Does not affect cookies attached
    // to or set by network requests.
    //
    // This can only return false for processes locked to a particular origin,
    // which can happen for any origin when the --site-per-process flag is used,
    // or for isolated origins that require a dedicated process (see
    // AddIsolatedOrigins).
    bool CanAccessDataForOrigin(const GURL& url);
    bool CanAccessDataForOrigin(const url::Origin& origin);

   private:
    friend class ChildProcessSecurityPolicyImpl;
    // |child_id| - The ID of the process that this Handle is being created
    // for, or ChildProcessHost::kInvalidUniqueID if an invalid handle is being
    // created.
    // |duplicating_handle| - True if the handle is being created by a
    // Duplicate() call. Otherwise false. This is used to trigger special
    // behavior for handle duplication that is not allowed for Handles created
    // by other means.
    Handle(int child_id, bool duplicating_handle);

    // The ID of the child process that this handle is associated with or
    // ChildProcessHost::kInvalidUniqueID if the handle is no longer valid.
    int child_id_;
  };

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
  void ClearIsolatedOriginsForTesting() override;

  // Identical to the above method, but takes url::Origin as input.
  bool CanAccessDataForOrigin(int child_id, const url::Origin& origin);

  // Shared helper for GURL and url::Origin processing.
  bool CanAccessDataForOrigin(int child_id,
                              const GURL& url,
                              bool url_is_precursor_of_opaque_origin);

  // Determines if the combination of |origin|, |url|,
  // |is_coop_coep_cross_origin_isolated|, and
  // |coop_coep_cross_origin_isolated_origin| is safe to commit to the process
  // associated with |child_id|.
  //
  // Returns CAN_COMMIT_ORIGIN_AND_URL if it is safe to commit the |origin| and
  // |url| combination to the process associated with |child_id|.
  // Returns CANNOT_COMMIT_URL if |url| is not safe to commit.
  // Returns CANNOT_COMMIT_ORIGIN if |origin| is not safe to commit.
  CanCommitStatus CanCommitOriginAndUrl(
      int child_id,
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      const UrlInfo& url_info,
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>&
          coop_coep_cross_origin_isolated_origin);

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
                                 bool origin_requests_isolation,
                                 url::Origin* result);

  // Removes any origin isolation opt-in entries associated with the
  // |isolation_context| of the BrowsingInstance.
  void RemoveOptInIsolatedOriginsForBrowsingInstance(
      const IsolationContext& isolation_context);

  // Registers |origin|'s isolation status with respect to the BrowsingInstance
  // associated with |isolation_context|. If it has already been registered,
  // then nothing will be changed by this call.
  void AddOptInIsolatedOriginForBrowsingInstance(
      const IsolationContext& isolation_context,
      const url::Origin& origin);

  // This function will check whether |origin| has opted-in to process isolation
  // (via OriginPolicy), with respect to the current state of the
  // |isolation_context|. It is different from IsIsolatedOrigin() in that it
  // only deals with OriginPolicy isolation status, whereas IsIsolatedOrigin()
  // considers all possible mechanisms for requesting isolation.
  // It will check for two things:
  // 1) whether |origin| already is assigned to a SiteInstance in the
  //    |isolation_context| by being tracked in either
  //    |origin_isolation_non_isolated_by_browsing_instance_| or
  //    |origin_isolation_by_browsing_instance_|, in which case we follow the
  //    same policy, or
  // 2) if it's not currently tracked as described above, whether |origin| is
  //    currently requesting isolation via |origin_requests_isolation|.
  bool ShouldOriginGetOptInIsolation(const IsolationContext& isolation_context,
                                     const url::Origin& origin,
                                     bool origin_requests_isolation);

  // This function adds |origin| to the master list of origins that have
  // ever requested opt-in isolation, either via an OriginPolicy or opt-in
  // header. Returns true if |origin| is not already in the list.
  bool UpdateOriginIsolationOptInListIfNecessary(const url::Origin& origin);

  // A version of GetMatchingIsolatedOrigin that takes in both the |origin| and
  // the |site_url| that |origin| corresponds to.  |site_url| is the key by
  // which |origin| will be looked up in |isolated_origins_| within
  // |isolation_context|; this function allows it to be passed in when it is
  // already known to avoid recomputing it internally.
  bool GetMatchingIsolatedOrigin(const IsolationContext& isolation_context,
                                 const url::Origin& origin,
                                 bool origin_requests_isolation,
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

  // Helper method for unit tests that calls Add() and
  // LockProcess() with an "allow_any_site" lock. This ensures that the process
  // policy is always in a state where it is valid to call
  // CanAccessDataForOrigin().
  void AddForTesting(int child_id, BrowserContext* browser_context);

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
  // for the origin specified by |site_info|'s process_lock_url(). Most callers
  // should use RenderProcessHostImpl::SetProcessLock instead of calling this
  // directly. |isolation_context| provides the context, such as
  // BrowsingInstance, from which this process locked was created. This
  // information is used when making isolation decisions for this process, such
  // as determining which isolated origins pertain to it.
  void LockProcess(const IsolationContext& isolation_context,
                   int child_id,
                   const ProcessLock& process_lock);

  // Testing helper method that generates a lock_url from |url| and then
  // calls LockProcess() with that lock URL.
  void LockProcessForTesting(const IsolationContext& isolation_context,
                             int child_id,
                             const GURL& url);

  // Retrieves the current ProcessLock of process |child_id|.  Returns an empty
  // lock if the process does not exist or if it is not locked.
  ProcessLock GetProcessLock(int child_id);

  // Register FileSystem type and permission policy which should be used
  // for the type.  The |policy| must be a bitwise-or'd value of
  // storage::FilePermissionPolicy.
  void RegisterFileSystemPermissionPolicy(storage::FileSystemType type,
                                          int policy);

  // Returns true if sending system exclusive messages is allowed.
  bool CanSendMidiSysExMessage(int child_id);

  // Remove all isolated origins associated with |browser_context| and clear any
  // pointers that may reference |browser_context|.  This is
  // typically used when |browser_context| is being destroyed and assumes that
  // no processes are running or will run for that profile; this makes the
  // isolated origin removal safe.  Note that |browser_context| cannot be null;
  // i.e., isolated origins that apply globally to all profiles cannot
  // currently be removed, since that is not safe to do at runtime.
  void RemoveStateForBrowserContext(const BrowserContext& browser_context);

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
  // will only affect future BrowsingInstances. |origin_requests_isolation| may
  // be true during navigation requests, and allows us to correctly determine
  // isolation status for an origin that may not have had its isolation status
  // recorded in the BrowsingInstance yet.
  bool IsIsolatedOrigin(const IsolationContext& isolation_context,
                        const url::Origin& origin,
                        bool origin_requests_isolation);

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

  // Sets "killed_process_origin_lock" crash key with lock info for the
  // process associated with |child_id|.
  void LogKilledProcessOriginLock(int child_id);

  // Creates a Handle object for a specific child process ID.
  //
  // This handle can be used to extend the lifetime of policy state beyond
  // the Remove() call for |child_id|. This should be used by objects that can
  // outlive the RenderProcessHostImpl object associated with |child_id| and
  // need to be able to make policy decisions after RPHI destruction. (e.g.
  // Mojo services created by RPHI)
  //
  // Returns a valid Handle for any |child_id| that is present in
  // |security_state_|. Otherwise it returns a Handle that returns false for
  // all policy checks.
  Handle CreateHandle(int child_id);

  // Returns true if we have seen an isolation request for this origin before
  // in any BrowsingInstance.
  bool HasOriginEverRequestedOptInIsolation(const url::Origin& origin);

  // Adds |origin| to the non-isolated list for the BrowsingInstance specified
  // by |isolation_context|, if we need to track it and it's not already in the
  // list. |is_global_walk_or_frame_removal| should be set to true during the
  // global walk that is triggered when |origin| first requests opt-in
  // isolation, so that the function can skip safety checks that will be
  // unnecessary during the global walk. It is also set to true if this function
  // is called when removing a FrameNavigationEntry, since that entry won't be
  // available to any subsequent global walks.
  void AddNonIsolatedOriginIfNeeded(const IsolationContext& isolation_context,
                                    const url::Origin& origin,
                                    bool is_global_walk_or_frame_removal);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  friend class ChildProcessSecurityPolicyTest;
  friend class ChildProcessSecurityPolicyImpl::Handle;
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

  bool AddProcessReference(int child_id, bool duplicating_handle);
  bool AddProcessReferenceLocked(int child_id, bool duplicating_handle)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RemoveProcessReference(int child_id);
  void RemoveProcessReferenceLocked(int child_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

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

  // Contains a mapping between child process ID and the number of outstanding
  // references that want to keep the SecurityState for each process alive.
  // This object and Handles created by this object increment/decrement
  // the counts in this map and only destroy a SecurityState object for a
  // process when its count goes to zero.
  std::map<int, int> process_reference_counts_ GUARDED_BY(lock_);

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

  // TODO(wjmaclean): Move these lists into a per-BrowserContext container, to
  // prevent any record of sites visible in one profile from being visible to
  // another profile.
  base::Lock origins_isolation_opt_in_lock_;
  // The set of all origins that have ever requested opt-in isolation. This is
  // tracked so we know which origins need to be tracked when non-isolated in
  // any given BrowsingInstance. Origins requesting isolation, if successful,
  // are marked as isolated via ShouldOriginGetOptInIsolation's checking
  // |origin_requests_isolation|.
  base::flat_set<url::Origin> origin_isolation_opt_ins_
      GUARDED_BY(origins_isolation_opt_in_lock_);
  // A map to track origins that have been isolated within a given
  // BrowsingInstance.
  base::flat_map<BrowsingInstanceId, std::vector<url::Origin>>
      origin_isolation_by_browsing_instance_
          GUARDED_BY(origins_isolation_opt_in_lock_);
  // A map to track origins that have been loaded in a BrowsingInstance without
  // isolation, but that have requested isolation in at least one other
  // BrowsingInstance. This map makes sure we don't try to isolate the origin
  // in the associated BrowsingInstance at a later time, in order to keep the
  // isolation consistent over the lifetime of the BrowsingInstance.
  base::flat_map<BrowsingInstanceId, std::vector<url::Origin>>
      origin_isolation_non_isolated_by_browsing_instance_
          GUARDED_BY(origins_isolation_opt_in_lock_);

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
