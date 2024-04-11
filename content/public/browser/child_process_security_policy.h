// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class FilePath;
}

namespace content {

class BrowserContext;

// The ChildProcessSecurityPolicy class is used to grant and revoke security
// capabilities for child processes.  For example, it restricts whether a child
// process is permitted to load file:// URLs based on whether the process
// has ever been commanded to load file:// URLs by the browser.
//
// ChildProcessSecurityPolicy is a singleton that may be used on any thread.
//
class ChildProcessSecurityPolicy {
 public:
  virtual ~ChildProcessSecurityPolicy() {}

  // There is one global ChildProcessSecurityPolicy object for the entire
  // browser process.  The object returned by this method may be accessed on
  // any thread.
  static CONTENT_EXPORT ChildProcessSecurityPolicy* GetInstance();

  // Web-safe schemes can be requested by any child process.  Once a web-safe
  // scheme has been registered, any child process can request URLs whose
  // origins use that scheme. There is no mechanism for revoking web-safe
  // schemes.
  //
  // Only call this function if URLs of this scheme are okay to host in
  // any ordinary renderer process.
  //
  // Registering 'your-scheme' as web-safe also causes 'blob:your-scheme://'
  // and 'filesystem:your-scheme://' URLs to be considered web-safe.
  virtual void RegisterWebSafeScheme(const std::string& scheme) = 0;

  // More restrictive variant of RegisterWebSafeScheme; URLs with this scheme
  // may be requested by any child process, but navigations to this scheme may
  // only commit in child processes that have been explicitly granted
  // permission to do so.
  //
  // |always_allow_in_origin_headers| controls whether this scheme is allowed to
  // appear as the Origin HTTP header in outbound requests, even if the
  // originating process does not have permission to commit this scheme. This
  // may be necessary if the scheme is used in conjunction with blink's
  // IsolatedWorldSecurityOrigin mechanism, as for extension content scripts.
  virtual void RegisterWebSafeIsolatedScheme(
      const std::string& scheme,
      bool always_allow_in_origin_headers) = 0;

  // Returns true iff |scheme| has been registered as a web-safe scheme.
  // TODO(nick): https://crbug.com/651534 This function does not have enough
  // information to render an appropriate judgment for blob and filesystem URLs;
  // change it to accept an URL instead.
  virtual bool IsWebSafeScheme(const std::string& scheme) = 0;

  // This permission grants only read access to a file.
  // Whenever the user picks a file from a <input type="file"> element, the
  // browser should call this function to grant the child process the capability
  // to upload the file to the web. Grants FILE_PERMISSION_READ_ONLY.
  virtual void GrantReadFile(int child_id, const base::FilePath& file) = 0;

  // This permission grants creation, read, and full write access to a file,
  // including attributes.
  virtual void GrantCreateReadWriteFile(int child_id,
                                        const base::FilePath& file) = 0;

  // This permission grants copy-into permission for |dir|.
  virtual void GrantCopyInto(int child_id, const base::FilePath& dir) = 0;

  // This permission grants delete permission for |dir|.
  virtual void GrantDeleteFrom(int child_id, const base::FilePath& dir) = 0;

  // Determine whether the process has the capability to request the URL.
  // Before servicing a child process's request for a URL, the content layer
  // calls this method to determine whether it is safe. CanRequestURL() allows
  // for requests that might lead to cross-process navigations or external
  // protocol handlers.
  //
  // Note that this check is different from checking whether a document with
  // the given URL can be committed in a process; that latter check is more
  // restrictive and is performed within //content (see
  // ChildProcessSecurityPolicyImpl::CanCommitURL).
  virtual bool CanRequestURL(int child_id, const GURL& url) = 0;

  // These methods verify whether or not the child process has been granted
  // permissions perform these functions on |file|.

  // Before servicing a child process's request to upload a file to the web, the
  // browser should call this method to determine whether the process has the
  // capability to upload the requested file.
  virtual bool CanReadFile(int child_id, const base::FilePath& file) = 0;
  virtual bool CanCreateReadWriteFile(int child_id,
                                      const base::FilePath& file) = 0;

  // Grants read access permission to the given isolated file system
  // identified by |filesystem_id|. An isolated file system can be
  // created for a set of native files/directories (like dropped files)
  // using storage::IsolatedContext. A child process needs to be granted
  // permission to the file system to access the files in it using
  // file system URL. You do NOT need to give direct permission to
  // individual file paths.
  //
  // Note: files/directories in the same file system share the same
  // permission as far as they are accessed via the file system, i.e.
  // using the file system URL (tip: you can create a new file system
  // to give different permission to part of files).
  virtual void GrantReadFileSystem(int child_id,
                                   const std::string& filesystem_id) = 0;

  // Grants write access permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for GrantReadFileSystem
  // for more details.  You do NOT need to give direct permission to
  // individual file paths.
  //
  // This must be called with a great care as this gives write permission
  // to all files/directories included in the file system.
  virtual void GrantWriteFileSystem(int child_id,
                                    const std::string& filesystem_id) = 0;

  // Grants create file permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for GrantReadFileSystem
  // for more details.  You do NOT need to give direct permission to
  // individual file paths.
  //
  // This must be called with a great care as this gives create permission
  // within all directories included in the file system.
  virtual void GrantCreateFileForFileSystem(
      int child_id,
      const std::string& filesystem_id) = 0;

  // Grants create, read and write access permissions to the given isolated
  // file system identified by |filesystem_id|.  See comments for
  // GrantReadFileSystem for more details.  You do NOT need to give direct
  // permission to individual file paths.
  //
  // This must be called with a great care as this gives create, read and write
  // permissions to all files/directories included in the file system.
  virtual void GrantCreateReadWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) = 0;

  // Grants permission to copy-into filesystem |filesystem_id|. 'copy-into'
  // is used to allow copying files into the destination filesystem without
  // granting more general create and write permissions.
  virtual void GrantCopyIntoFileSystem(int child_id,
                                       const std::string& filesystem_id) = 0;

  // Grants permission to delete from filesystem |filesystem_id|. 'delete-from'
  // is used to allow deleting files into the destination filesystem without
  // granting more general create and write permissions.
  virtual void GrantDeleteFromFileSystem(int child_id,
                                         const std::string& filesystem_id) = 0;

  // Grants the child process the capability to commit URLs with the provided
  // origin. Usage should be extremely rare: the content framework already
  // automatically grants this privilege as needed on successful navigation to a
  // URL.
  // If you think you need this, please reach out to site-isolation-dev@ first.
  virtual void GrantCommitOrigin(int child_id, const url::Origin& origin) = 0;
  //
  // Grants the child process the capability to request URLs with the provided
  // origin.
  virtual void GrantRequestOrigin(int child_id, const url::Origin& origin) = 0;

  // Grants the child process the capability to request URLs of the provided
  // scheme.
  virtual void GrantRequestScheme(int child_id, const std::string& scheme) = 0;

  // Returns true if read access has been granted to |filesystem_id|.
  virtual bool CanReadFileSystem(int child_id,
                                 const std::string& filesystem_id) = 0;

  // Returns true if read and write access has been granted to |filesystem_id|.
  virtual bool CanReadWriteFileSystem(int child_id,
                                      const std::string& filesystem_id) = 0;

  // Returns true if copy-into access has been granted to |filesystem_id|.
  virtual bool CanCopyIntoFileSystem(int child_id,
                                     const std::string& filesystem_id) = 0;

  // Returns true if delete-from access has been granted to |filesystem_id|.
  virtual bool CanDeleteFromFileSystem(int child_id,
                                       const std::string& filesystem_id) = 0;

  // Returns true if the specified child_id has been granted WebUI bindings.
  // The browser should check this property before assuming the child process
  // is allowed to use WebUI bindings.
  virtual bool HasWebUIBindings(int child_id) = 0;

  // Grants permission to send messages to any MIDI devices.
  virtual void GrantSendMidiMessage(int child_id) = 0;

  // Grants permission to send system exclusive (SysEx) messages to any MIDI
  // devices.
  virtual void GrantSendMidiSysExMessage(int child_id) = 0;

  // This function checks whether a process is allowed to read and/or modify
  // `origin`'s data such as passwords, cookies, or localStorage. Generally,
  // this implies that an instance of `origin` must have previously been
  // committed in this process (e.g., via a navigation or worker instantiation).
  //
  // Internally, this function performs two kinds of security checks:
  // - "Jail" check: ensures that a process locked to a particular site can only
  //   access data belonging to that site.
  // - "Citadel" check: ensures that a process that is *not* locked to a
  //   particular site does not access data belonging to a site that requires a
  //   dedicated process. This check is mainly relevant on Android, where only
  //   some sites require site isolation.
  //
  // This is one of two core ways of enforcing Site Isolation (see
  // docs/process_model_and_site_isolation.md), the other being `HostsOrigin()`
  // below. Compared to `HostsOrigin()`, this check is more strict.  In
  // particular, it may block access for renderer processes that should never
  // need to access any origin's data, such as processes containing only
  // documents with opaque origins (e.g., from sandboxed frames), or processes
  // containing more restricted content types like PDF.
  virtual bool CanAccessDataForOrigin(int child_id,
                                      const url::Origin& origin) = 0;

  // This function checks whether a process has an instance of a particular
  // `origin`, either because it has committed a document or has instantiated a
  // worker with that origin. This can be used to validate initiator or source
  // origins that are included in messages or IPCs received from the renderer,
  // such as the source origin for postMessage. Generally, this function
  // performs similar enforcements to `CanAccessDataForOrigin()` above,
  // including both jail and citadel checks. In contrast to
  // `CanAccessDataForOrigin()`, this function permits access from opaque
  // origins, making it more suitable to use for APIs that are allowed in such
  // origins (such as postMessage).
  //
  // Subtle note: due to shutdown races, where a document/worker destruction may
  // race with processing legitimate IPCs on behalf of `origin` from the
  // renderer, this function actually checks for the process either *currently*
  // having an origin, or having hosted it in the *past* (but not necessarily
  // now).
  virtual bool HostsOrigin(int child_id, const url::Origin& origin) = 0;

  // Defines available sources of isolated origins.  This should be specified
  // when adding isolated origins with the AddFutureIsolatedOrigins() call
  // below.
  enum class IsolatedOriginSource {
    // Used for origins that are hardcoded into the browser.
    BUILT_IN,
    // Used for origins that are specified from the command line, i.e.
    // --isolate-origins.
    COMMAND_LINE,
    // Used for origins that are configured through field trials.
    FIELD_TRIAL,
    // Used for origins defined by an administrator (e.g., via enterprise
    // policy).
    POLICY,
    // Used for origins that are isolated based on user-triggered runtime
    // heuristics.
    USER_TRIGGERED,
    // Used for origins that are isolated based on runtime heuristics triggered
    // directly by web pages, such as headers.
    WEB_TRIGGERED,
    // Used for testing purposes.
    TEST
  };

  // Add |origins| to the list of origins that require process isolation.  When
  // making process model decisions for such origins, the scheme+host tuple
  // rather than scheme and eTLD+1 will be used.  SiteInstances for these
  // origins will also use the full host of the isolated origin as site URL.
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
  //
  // The new isolated origins will apply only to BrowsingInstances and renderer
  // processes created *after* this call.  This is necessary to not break
  // scripting relationships between same-origin iframes in existing
  // BrowsingInstances.  To do this, this function internally determines a
  // threshold BrowsingInstance ID that is higher than all existing
  // BrowsingInstance IDs but lower than future BrowsingInstance IDs, and
  // associates it with each of the |origins|. If an origin had already been
  // isolated prior to calling this, it is ignored, and its threshold is not
  // updated.
  //
  // |source| describes the context/reason for adding the new isolated origins;
  // see comments on IsolatedOriginSource.
  //
  // If |browser_context| is non-null, the new isolated origins added via this
  // function will apply only within that BrowserContext.  If |browser_context|
  // is null, the new isolated origins will apply globally in *all*
  // BrowserContexts (but still subject to the BrowsingInstance ID cutoff in
  // the previous paragraph).
  //
  // This function may be called again for the same origin but different
  // |browser_context|. In that case, the origin will be isolated in all
  // BrowserContexts for which this function has been called.  However,
  // attempts to re-add an origin for the same |browser_context| will be
  // ignored.
  virtual void AddFutureIsolatedOrigins(
      const std::vector<url::Origin>& origins,
      IsolatedOriginSource source,
      BrowserContext* browser_context = nullptr) = 0;

  // Semantically identical to the above, but accepts a string of comma
  // separated origins. |origins_to_add| can contain both wildcard and
  // non-wildcard origins, e.g. "https://[*.]foo.com,https://bar.com".
  //
  // Wildcard origins provide a way to treat all subdomains under the specified
  // host and scheme as distinct isolated origins. For example,
  // https://[*.]foo.com would isolate https://foo.com, https://bar.foo.com and
  // https://qux.baz.foo.com all in separate processes. Adding a wildcard origin
  // implies breaking document.domain for all of its subdomains.
  //
  // Note that wildcards can only be added using this version of
  // AddFutureIsolatedOrigins(); they cannot be specified in a url::Origin().
  virtual void AddFutureIsolatedOrigins(
      std::string_view origins_to_add,
      IsolatedOriginSource source,
      BrowserContext* browser_context = nullptr) = 0;

  // Returns true if |origin| is a globally (not per-profile) isolated origin.
  virtual bool IsGloballyIsolatedOriginForTesting(
      const url::Origin& origin) = 0;

  // Returns the set of currently active isolated origins, optionally filtered
  // by the source of how they were added and/or by BrowserContext.
  //
  // If |source| is provided, only origins that were added with the same source
  // will be returned; if |source| is std::nullopt, origins from all sources
  // will be returned.
  //
  // If |browser_context| is null, only globally applicable origins will be
  // returned.  If |browser_context| is non-null, only origins that apply
  // within that particular BrowserContext will be returned (note that this
  // includes both matching per-profile isolated origins as well as globally
  // applicable origins which apply to |browser_context| by definition).
  //
  // Origins returned by this function only include origins that would apply to
  // any future BrowsingInstance (browsing context group).  Origins that were
  // isolated only in specific BrowsingInstances are not included.  (In
  // particular, this excludes BrowsingInstance-specific isolated origins for
  // Origin-Agent-Cluster as well as COOP documents loaded without user
  // activation.)
  virtual std::vector<url::Origin> GetIsolatedOrigins(
      std::optional<IsolatedOriginSource> source = std::nullopt,
      BrowserContext* browser_context = nullptr) = 0;

  // Returns whether the site of |origin| is isolated and was added by the
  // |source| to be isolated.
  virtual bool IsIsolatedSiteFromSource(const url::Origin& origin,
                                        IsolatedOriginSource source) = 0;

  // Clears all isolated origins.  This is unsafe to use outside of testing.
  virtual void ClearIsolatedOriginsForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
