// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
#define CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/can_commit_status.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/isolation_context.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/bindings_policy.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace content {

class BrowserContext;
class IsolationContext;
class ProcessLock;
class ResourceContext;
struct UrlInfo;

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

    // Before servicing a child process's request to upload a file to the web,
    // the browser should call this method to determine whether the process has
    // the capability to upload the requested file.
    bool CanReadFile(const base::FilePath& file);

    // Explicit read permissions check for FileSystemURL specified files.
    bool CanReadFileSystemFile(const storage::FileSystemURL& url);

    // Returns true if the process is permitted to read and modify the data for
    // the given `origin`. For more details, see
    // ChildProcessSecurityPolicy::CanAccessDataForOrigin().
    bool CanAccessDataForOrigin(const url::Origin& origin);

    // Returns the original `child_id` used to create the handle.
    int child_id() { return child_id_; }

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

  ChildProcessSecurityPolicyImpl(const ChildProcessSecurityPolicyImpl&) =
      delete;
  ChildProcessSecurityPolicyImpl& operator=(
      const ChildProcessSecurityPolicyImpl&) = delete;

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
  void GrantSendMidiMessage(int child_id) override;
  void GrantSendMidiSysExMessage(int child_id) override;
  bool CanAccessDataForOrigin(int child_id, const url::Origin& origin) override;
  bool HostsOrigin(int child_id, const url::Origin& origin) override;
  void AddFutureIsolatedOrigins(
      std::string_view origins_list,
      IsolatedOriginSource source,
      BrowserContext* browser_context = nullptr) override;
  void AddFutureIsolatedOrigins(
      const std::vector<url::Origin>& origins,
      IsolatedOriginSource source,
      BrowserContext* browser_context = nullptr) override;
  bool IsGloballyIsolatedOriginForTesting(const url::Origin& origin) override;
  std::vector<url::Origin> GetIsolatedOrigins(
      std::optional<IsolatedOriginSource> source = std::nullopt,
      BrowserContext* browser_context = nullptr) override;
  bool IsIsolatedSiteFromSource(const url::Origin& origin,
                                IsolatedOriginSource source) override;
  void ClearIsolatedOriginsForTesting() override;

  // Centralized internal implementation of site isolation enforcements,
  // including CanAccessDataForOrigin and HostsOrigin. It supports the following
  // types of access checks, in order of increasing strictness:
  enum class AccessType {
    // Whether the process can commit a navigation to an origin, allowing a
    // document with that origin to be hosted in this process. This is
    // specifically about whether a particular new origin may be introduced
    // into a given process.
    kCanCommitNewOrigin,
    // Whether the process has previously committed a document or instantiated a
    // worker with the particular origin. This can be used to verify whether a
    // particular origin can be used as an initiator or source origin, e.g. in
    // postMessage or other IPCs sent from this process. Unlike
    // kCanCommitNewOrigin, this check assumes that the origin must already
    // exist in the process. Because a document/worker destruction may race with
    // processing legitimate IPCs on behalf of `origin`, this check also allows
    // the case where an origin has been hosted by the process in the past, but
    // not necessarily now.
    kHostsOrigin,
    // Whether the process can access data belonging to an origin already
    // committed in the process, such as passwords, localStorage, or cookies.
    // Similarly to kHostsOrigin, this check assumes that the origin must
    // already
    // exist in the process, but it is more strict for certain kinds of
    // processes that aren't supposed to access any data. For example, sandboxed
    // frame processes (which contain only opaque origins) or PDF processes
    // cannot access data for any origin.
    kCanAccessDataForCommittedOrigin,
  };
  bool CanAccessOrigin(int child_id,
                       const url::Origin& origin,
                       AccessType access_type);

  // Determines if the combination of origin, url and web_exposed_isolation_info
  // bundled in `url_info` are safe to commit to the process associated with
  // `child_id`.
  //
  // Returns CAN_COMMIT_ORIGIN_AND_URL if it is safe to commit `url_info` origin
  // and `url_info`'s url combination to the process associated with `child_id`.
  // Returns CANNOT_COMMIT_URL if `url_info` url is not safe to commit.
  // Returns CANNOT_COMMIT_ORIGIN if `url_info` origin is not safe to commit.
  CanCommitStatus CanCommitOriginAndUrl(
      int child_id,
      const IsolationContext& isolation_context,
      const UrlInfo& url_info);

  // Whether the process is allowed to commit a document from the given URL.
  // This is more restrictive than CanRequestURL, since CanRequestURL allows
  // requests that might lead to cross-process navigations or external protocol
  // handlers. Used primarily as a helper for CanCommitOriginAndUrl and thus not
  // exposed publicly.
  bool CanCommitURL(int child_id, const GURL& url);

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
  bool GetMatchingProcessIsolatedOrigin(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      bool requests_origin_keyed_process,
      url::Origin* result);

  // Removes any origin isolation opt-in entries associated with the
  // |browsing_instance_id| of the BrowsingInstance.
  void RemoveOptInIsolatedOriginsForBrowsingInstance(
      const BrowsingInstanceId& browsing_instance_id);

  // Registers |origin| isolation state in the BrowsingInstance associated
  // with |isolation_context|.
  //
  // |is_origin_agent_cluster| is used to indicate |origin| will receive (at
  // least) logical isolation via OriginAgentCluster in the renderer. If it is
  // false, then |requires_origin_keyed_process| must also be false.
  //
  // If |requires_origin_keyed_process| is true, then |origin| will be
  // registered as an origin-keyed process; that is, subdomains of |origin|
  // won't be automatically grouped with |origin|. In particular, this can be
  // used for cases using the Origin-Agent-Cluster header.
  //
  // If |requires_origin_keyed_process| is false, then subdomains of |origin|
  // will be grouped together with |origin| in the same process. |origin| is
  // required to be a site (scheme and eTLD+1) in this case.
  //
  // If this function is called with differing values of
  // |requires_origin_keyed_process| for
  // the same IsolationContext and origin, then origin-keyed process isolation
  // takes precedence for |origin|, though site-keyed process isolation will
  // still be used for subdomains of |origin|.
  //
  // If |origin| has already been registered as isolated for the same
  // BrowsingInstance amd the same value of |requires_origin_keyed_process|,
  // then nothing will be changed by this call.
  void AddOriginIsolationStateForBrowsingInstance(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      bool is_origin_agent_cluster,
      bool requires_origin_keyed_process);

  // Adds `origin` to the IsolatedOrigins list for only the BrowsingInstance of
  // `isolation_context`, without isolating all subdomains. For use when the
  // isolation is triggered by COOP headers.
  void AddCoopIsolatedOriginForBrowsingInstance(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      IsolatedOriginSource source);

  // This function will check whether |origin| has opted-in to logical or
  // process isolation (via the Origin-Agent-Cluster header), with respect to
  // the current state of the |isolation_context|. It is different from
  // IsIsolatedOrigin() in that it only deals with Origin-Agent-Cluster
  // isolation status, whereas IsIsolatedOrigin() considers all possible
  // mechanisms for requesting isolation. It will check for two things:
  // 1) whether |origin| already is assigned to a SiteInstance in the
  //    |isolation_context| by being tracked in
  //    |origin_isolation_by_browsing_instance_|, in which case we follow the
  //    same policy, or
  // 2) if it's not currently tracked as described above, whether |origin| is
  //    currently requesting isolation via |requested_isolation_state|.
  OriginAgentClusterIsolationState DetermineOriginAgentClusterIsolation(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      const OriginAgentClusterIsolationState& requested_isolation_state);

  // This function adds |origin| to the master list of origins that have
  // ever requested opt-in isolation in the given |browser_context|, either via
  // an OriginPolicy or opt-in header. Returns true if |origin| is not already
  // in the list.
  bool UpdateOriginIsolationOptInListIfNecessary(
      BrowserContext* browser_context,
      const url::Origin& origin);

  // A version of GetMatchingProcessIsolatedOrigin that takes in both the
  // |origin| and the |site_url| that |origin| corresponds to.  |site_url| is
  // the key by which |origin| will be looked up in |isolated_origins_| within
  // |isolation_context|; this function allows it to be passed in when it is
  // already known to avoid recomputing it internally.
  bool GetMatchingProcessIsolatedOrigin(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      bool requests_origin_keyed_process,
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

  // Validate that `process` is allowed to access data in the POST body
  // specified by |body|.  Has to be called on the UI thread.
  bool CanReadRequestBody(
      RenderProcessHost* process,
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
  // file:// URL (or content:// URL in android), but not all urls of the file://
  // scheme.
  void GrantRequestOfSpecificFile(int child_id, const base::FilePath& file);

  // Revokes all permissions granted to the given file.
  void RevokeAllPermissionsForFile(int child_id, const base::FilePath& file);

  // Grant the child process the ability to use Web UI Bindings.
  void GrantWebUIBindings(int child_id, BindingsPolicySet bindings);

  // Grant the child process the ability to read raw cookies.
  void GrantReadRawCookies(int child_id);

  // Revoke read raw cookies permission.
  void RevokeReadRawCookies(int child_id);

  // Some APIs for Android WebView and <webview> tags allow bypassing some
  // security checks, such as which URLs are allowed to commit. This method
  // grants that ability to any document with an origin used with these APIs,
  // because the exemption is needed for about:blank frames that inherit the
  // same origin.
  //
  // For safety, this is limited to opaque origins used with LoadDataWithBaseURL
  // in unlocked processes, as well as file origins used with
  // allow_universal_access_from_file_urls.
  //
  // Note that LoadDataWithBaseURL can be used with non-opaque origins as well,
  // but in that case the bypass is only allowed for the document and not the
  // entire origin, to prevent other code in the origin from bypassing checks.
  void GrantOriginCheckExemptionForWebView(int child_id,
                                           const url::Origin& origin);

  // Returns whether the given opaque or file origin was granted an exemption
  // due to Android WebView and <webview> APIs, allowing its documents to bypass
  // certain URL and origin checks.
  bool HasOriginCheckExemptionForWebView(int child_id,
                                         const url::Origin& origin);

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
  bool CanMoveFileSystemFile(int child_id,
                             const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url);
  bool CanCopyFileSystemFile(int child_id,
                             const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url);

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
  // as determining which isolated origins pertain to it. |is_process_used|
  // indicates whether any content has been loaded in the process already.
  void LockProcess(const IsolationContext& isolation_context,
                   int child_id,
                   bool is_process_used,
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

  // Returns true if sending MIDI messages is allowed.
  bool CanSendMidiMessage(int child_id);

  // Returns true if sending system exclusive (SysEx) MIDI messages is allowed.
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

  // Returns true if we have seen an explicit Origin-Agent-Cluster header
  // (either opt-in or opt-out) for this |origin| in the given |browser_context|
  // before in any BrowsingInstance.
  bool HasOriginEverRequestedOriginAgentClusterValue(
      BrowserContext* browser_context,
      const url::Origin& origin);

  // Adds |origin| to the opt-in-out list as having the default isolation state
  // for the BrowsingInstance specified by |isolation_context|, if we need to
  // track it and it's not already in the list.
  // |is_global_walk_or_frame_removal| should be set to true during the global
  // walk that is triggered when |origin| first requests opt-in isolation, so
  // that the function can skip safety checks that will be unnecessary during
  // the global walk. It is also set to true if this function is called when
  // removing a FrameNavigationEntry, since that entry won't be available to any
  // subsequent global walks.
  void AddDefaultIsolatedOriginIfNeeded(
      const IsolationContext& isolation_context,
      const url::Origin& origin,
      bool is_global_walk_or_frame_removal);

  // Allows tests to modify the delay in cleaning up BrowsingInstanceIds. If the
  // delay is set to zero, cleanup happens immediately.
  void SetBrowsingInstanceCleanupDelayForTesting(int64_t delay_in_seconds) {
    browsing_instance_cleanup_delay_ = base::Seconds(delay_in_seconds);
  }

  // Allows tests to query the number of BrowsingInstanceIds associated with a
  // child process.
  size_t BrowsingInstanceIdCountForTesting(int child_id);

  void ClearRegisteredSchemeForTesting(const std::string& scheme);

  // Exposes LookupOriginIsolationState() for tests.
  OriginAgentClusterIsolationState* LookupOriginIsolationStateForTesting(
      const BrowsingInstanceId& browsing_instance_id,
      const url::Origin& origin);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  friend class ChildProcessSecurityPolicyTest;
  friend class ChildProcessSecurityPolicyImpl::Handle;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest, FilePermissions);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           AddFutureIsolatedOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           DynamicIsolatedOrigins);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsForSpecificBrowserContexts);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsForSpecificBrowsingInstances);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsForCurrentAndFutureBrowsingInstances);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginsRemovedWhenBrowserContextDestroyed);
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolateAllSuborigins);
  FRIEND_TEST_ALL_PREFIXES(
      ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
      WildcardAndNonWildcardOrigins);
  FRIEND_TEST_ALL_PREFIXES(
      ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
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
                        bool applies_to_future_browsing_instances,
                        BrowsingInstanceId browsing_instance_id,
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
      return std::tie(origin_, applies_to_future_browsing_instances_,
                      browsing_instance_id_, browser_context_,
                      resource_context_, isolate_all_subdomains_, source_) <
             std::tie(other.origin_,
                      other.applies_to_future_browsing_instances_,
                      other.browsing_instance_id_, other.browser_context_,
                      other.resource_context_, other.isolate_all_subdomains_,
                      source_);
    }

    bool operator==(const IsolatedOriginEntry& other) const {
      return origin_ == other.origin_ &&
             applies_to_future_browsing_instances_ ==
                 other.applies_to_future_browsing_instances_ &&
             browsing_instance_id_ == other.browsing_instance_id_ &&
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

    // True if this entry applies to the BrowsingInstance specified by
    // `browsing_instance_id`.  See `applies_to_future_browsing_instances_` and
    // `browsing_instance_id_` for more details.
    bool MatchesBrowsingInstance(BrowsingInstanceId browsing_instance_id) const;

    const url::Origin& origin() const { return origin_; }

    // See the declaration of `applies_to_future_browsing_instances_` for
    // details.
    bool applies_to_future_browsing_instances() const {
      return applies_to_future_browsing_instances_;
    }

    // See the declaration of `browsing_instance_id_` for details.
    BrowsingInstanceId browsing_instance_id() const {
      return browsing_instance_id_;
    }

    const BrowserContext* browser_context() const { return browser_context_; }

    bool isolate_all_subdomains() const { return isolate_all_subdomains_; }

    IsolatedOriginSource source() const { return source_; }

   private:
    url::Origin origin_;

    // If this is false, the origin is isolated only in the BrowsingInstance
    // specified by `browsing_instance_id_`.  If this is true, the origin is
    // isolated in all BrowsingInstances that have an ID equal to or
    // greater than `browsing_instance_id_`.
    bool applies_to_future_browsing_instances_;

    // Specifies which BrowsingInstance(s) this IsolatedOriginEntry applies to.
    // When `applies_to_future_browsing_instances_` is false, this refers to a
    // specific BrowsingInstance.  Otherwise, it specifies the minimum
    // BrowsingInstance ID, and the origin is isolated in all
    // BrowsingInstances with IDs greater than or equal to this value.
    BrowsingInstanceId browsing_instance_id_;

    // Optional information about the profile where the isolated origin
    // applies.  |browser_context_| may be used on the UI thread, and
    // |resource_context_| may be used on the IO thread.  If these are null,
    // then the isolated origin applies globally to all profiles.
    raw_ptr<BrowserContext> browser_context_;
    raw_ptr<ResourceContext> resource_context_;

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

  // A struct to hold the OAC opted-in origins and their isolation state. It
  // associates a specific |origin| with its OriginAgentClusterIsolationState,
  // and is tracked in |origin_isolation_by_browsing_instance_|.
  struct OriginAgentClusterOptInEntry {
    OriginAgentClusterOptInEntry(
        const OriginAgentClusterIsolationState& oac_isolation_state_in,
        const url::Origin& origin_in);
    OriginAgentClusterOptInEntry(const OriginAgentClusterOptInEntry&);
    ~OriginAgentClusterOptInEntry();

    OriginAgentClusterIsolationState oac_isolation_state;
    url::Origin origin;
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
  // AddFutureIsolatedOrigins().
  static std::vector<IsolatedOriginPattern> ParseIsolatedOrigins(
      std::string_view pattern_list);

  void AddFutureIsolatedOrigins(
      const std::vector<IsolatedOriginPattern>& patterns,
      IsolatedOriginSource source,
      BrowserContext* browser_context = nullptr);

  // Internal helper used for adding a particular isolated origin.  See
  // IsolatedOriginEntry for descriptions of various parameters.
  void AddIsolatedOriginInternal(BrowserContext* browser_context,
                                 const url::Origin& origin,
                                 bool applies_to_future_browsing_instances,
                                 BrowsingInstanceId browsing_instance_id,
                                 bool isolate_all_subdomains,
                                 IsolatedOriginSource source)
      EXCLUSIVE_LOCKS_REQUIRED(isolated_origins_lock_);

  bool AddProcessReference(int child_id, bool duplicating_handle);
  bool AddProcessReferenceLocked(int child_id, bool duplicating_handle)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RemoveProcessReference(int child_id);
  void RemoveProcessReferenceLocked(int child_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal helper for RemoveOptInIsolatedOriginsForBrowsingInstance().
  void RemoveOptInIsolatedOriginsForBrowsingInstanceInternal(
      const BrowsingInstanceId browsing_instance_id);

  // Creates the value to place in the "killed_process_origin_lock" crash key
  // based on the contents of |security_state|.
  static std::string GetKilledProcessOriginLock(
      const SecurityState* security_state);

  // Helper for CanAccessMaybeOpaqueOrigin, to perform two security checks:
  //  - Jail check: a process locked to a particular site shouldn't access data
  //    belonging to other sites.
  //  - Citadel check: a process not locked to any site shouldn't access data
  //    belonging to sites that require a dedicated process.
  //
  // These checks are performed by comparing the actual ProcessLock of the
  // process represented by `child_id` and `security_state` to an expected
  // ProcessLock computed from `url`, which takes into account factors such as
  // whether `url` should be site-isolated or origin-isolated (or not isolated,
  // e.g. on Android). Determining site-vs-origin isolation is non-trivial: the
  // answer may differ depending on BrowsingInstance (e.g., OriginAgentCluster
  // might require origin isolation only for certain BrowsingInstances), so all
  // BrowsingInstances hosting in the process must be consulted.
  //
  // This function returns true only if both Jail and Citadel checks pass. On
  // failure, it also populates `out_failure_reason` with debugging information
  // about the cause of the failure, as well as `out_expected_process_lock` with
  // what the process lock was expected to be (e.g., to be used in crash keys).
  //
  // This function must be called while already holding `lock_`.
  bool PerformJailAndCitadelChecks(int child_id,
                                   SecurityState* security_state,
                                   const GURL& url,
                                   bool url_is_precursor_of_opaque_origin,
                                   AccessType access_type,
                                   ProcessLock& out_expected_process_lock,
                                   std::string& out_failure_reason)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper for public CanAccessOrigin overloads.
  bool CanAccessMaybeOpaqueOrigin(int child_id,
                                  const GURL& url,
                                  bool url_is_precursor_of_opaque_origin,
                                  AccessType access_type);

  // Helper used by CanAccessOrigin to impose additional restrictions on a
  // sandboxed process locked to `process_lock`.
  bool IsAccessAllowedForSandboxedProcess(const ProcessLock& process_lock,
                                          const GURL& url,
                                          bool url_is_for_opaque_origin,
                                          AccessType access_type);

  // Helper used by CanAccessOrigin to impose additional restrictions on a
  // process that only hosts PDF documents.
  bool IsAccessAllowedForPdfProcess(AccessType access_type);

  // Utility function to simplify lookups for OriginAgentClusterOptInEntry
  // values by origin.
  OriginAgentClusterIsolationState* LookupOriginIsolationState(
      const BrowsingInstanceId& browsing_instance_id,
      const url::Origin& origin)
      EXCLUSIVE_LOCKS_REQUIRED(origins_isolation_opt_in_lock_);

  // You must acquire this lock before reading or writing any members of this
  // class, except for isolated_origins_, schemes_okay_to_*, and
  // pseudo_schemes_, which use their own locks.  You must not block while
  // holding this lock.
  base::Lock lock_;

  // These schemes are allow-listed for all child processes in various contexts.
  // These sets are protected by |schemes_lock_| rather than |lock_|.
  base::Lock schemes_lock_;
  SchemeSet schemes_okay_to_commit_in_any_process_ GUARDED_BY(schemes_lock_);
  SchemeSet schemes_okay_to_request_in_any_process_ GUARDED_BY(schemes_lock_);
  SchemeSet schemes_okay_to_appear_as_origin_headers_ GUARDED_BY(schemes_lock_);

  // These schemes do not actually represent retrievable URLs.  For example,
  // the the URLs in the "about" scheme are aliases to other URLs.  This set is
  // protected by |schemes_lock_|.
  SchemeSet pseudo_schemes_ GUARDED_BY(schemes_lock_);

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
  //   1. Which BrowsingInstances it applies to.  This is a combination of a
  //      BrowsingInstance ID |browsing_instance_id_| and a bool flag
  //      |applies_to_future_browsing_instances_| stored in in each origin's
  //      IsolatedOriginEntry.  When |applies_to_future_browsing_instances_| is
  //      true, the origin will be isolated in all BrowsingInstances with
  //      IDs equal to or greater than |browsing_instance_id_|. When
  //      |applies_to_future_browsing_instances_| is false, the origin will be
  //      isolated only in a single BrowsingInstance with ID
  //      |browsing_instance_id_|.
  //   2. Optionally, which BrowserContext (profile) it applies to.  When the
  //      |browser_context| field in the IsolatedOriginEntry is non-null, a
  //      particular isolated origin entry only applies to that BrowserContext.
  //      A ResourceContext, BrowserContext's representation on the IO thread,
  //      is also stored in the entry to facilitate checks on the IO thread.
  //      Note that the same origin may be isolated in different profiles,
  //      possibly with different BrowsingInstance ID cut-offs.  For example:
  //        https://foo.com -> { [https://test.foo.com profile1 4],
  //                             [https://test.foo.com profile2 7] }
  //      represents https://test.foo.com being isolated in profile1
  //      with BrowsingInstance ID 4, and also in profile2 with
  //      BrowsingInstance ID 7.
  base::flat_map<GURL, std::vector<IsolatedOriginEntry>> isolated_origins_
      GUARDED_BY(isolated_origins_lock_);

  // TODO(wjmaclean): Move these lists into a per-BrowserContext container, to
  // prevent any record of sites visible in one profile from being visible to
  // another profile.
  base::Lock origins_isolation_opt_in_lock_;
  // The set of all origins that have ever requested opt-in isolation or
  // requested to opt-out, organized by BrowserContext. This is tracked so we
  // know which origins need to be tracked when using default isolation in any
  // given BrowsingInstance. Origins requesting isolation opt-in or out, if
  // successful, are marked as isolated or not via
  // DetermineOriginAgentClusterIsolation's checking
  // |requested_isolation_state|. Each BrowserContext's state is tracked
  // separately so that timing attacks do not reveal whether an origin has been
  // visited in another (e.g., incognito) BrowserContext. In general, the state
  // of other BrowsingInstances is not observable outside such timing side
  // channels.
  base::flat_map<BrowserContext*, base::flat_set<url::Origin>>
      origin_isolation_opt_ins_and_outs_
          GUARDED_BY(origins_isolation_opt_in_lock_);

  // A map to track origins that have been isolated within a given
  // BrowsingInstance, or that have been loaded in a BrowsingInstance
  // without isolation, but that have requested isolation in at least one other
  // BrowsingInstance. Origins loaded without isolation are tracked to make sure
  // we don't try to isolate the origin in the associated BrowsingInstance at a
  // later time, in order to keep the isolation consistent over the lifetime of
  // the BrowsingInstance.
  base::flat_map<BrowsingInstanceId, std::vector<OriginAgentClusterOptInEntry>>
      origin_isolation_by_browsing_instance_
          GUARDED_BY(origins_isolation_opt_in_lock_);

  // When we are notified a BrowsingInstance has destructed, delay cleanup by
  // this amount to allow outstanding IO thread requests to complete. May be set
  // to different values in tests. Note: the value is chosen to be slightly
  // longer than the KeepAliveHandleFactory delay of 30 seconds, with the aim of
  // covering the maximum time needed by any IncrementKeepAliveRefCount callers.
  // TODO(wjmaclean): we know the IncrementKeepAliveRefCount API needs
  // improvement, and with it the BrowsingInstance cleanup here can also be
  // improved.
  base::TimeDelta browsing_instance_cleanup_delay_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
