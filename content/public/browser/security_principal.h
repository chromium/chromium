// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_

#include <string_view>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class StoragePartitionConfig;

// SecurityPrincipal is an abstraction used by Chromium's process model to
// divide documents, workers, and other web content into processes.
//
// All documents and workers within a SiteInstance are considered part of
// the same SecurityPrincipal and will share a renderer process.
// Any two documents within the same browsing context group
// (i.e., BrowsingInstance) that are allowed to script each other *must*
// have the same SecurityPrincipal, so that they end up in the same renderer
// process.
//
// SecurityPrincipal can also be used with non-renderer processes that host
// untrustworthy code, specifically to share utility processes by services in
// a way that respects site isolation.
//
// SecurityPrincipal is the interface that's exposed to features that need
// to utilize security principals outside of //content, providing access to a
// subset of SiteInfo properties. SiteInfo is the sole implementation of that
// interface for use inside //content.
//
// For more information about security principals and site isolation see
// docs/process_model_and_site_isolation.md
class CONTENT_EXPORT SecurityPrincipal {
 public:
  virtual ~SecurityPrincipal() = default;

  // Returns true if this SecurityPrincipal is for process-isolated sandboxed
  // documents only.
  virtual bool IsSandboxed() const = 0;

  // Returns true if this SecurityPrincipal is used for a <webview> guest.
  virtual bool IsGuest() const = 0;

  // Returns true if the scheme of this SecurityPrincipal is for a WebUI page
  // (chrome://, chrome-untrusted://, devtools://, chrome-search://). For the
  // full list of recognized WebUI schemes, see
  // URLDataManagerBackend::GetWebUISchemes().
  virtual bool IsWebUI() const = 0;

  // Get the StoragePartitionConfig, which describes the StoragePartition this
  // SecurityPrincipal is associated with.  For example, this will correspond to
  // a non-default StoragePartition for <webview> guests.
  virtual const StoragePartitionConfig& GetStoragePartitionConfig() const = 0;

  // Returns true if the scheme of this SecurityPrincipal's associated URL
  // matches the given |scheme|. For example, SchemeIs("https") returns true
  // if the principal represents content loaded from an HTTPS URL.
  // Note that when a principal uses an effective URL, the scheme will
  // correspond to that effective URL rather than the original. For example,
  // a hosted app at https://example.com has an effective URL of
  // chrome-extension://<extension-id>/, so SchemeIs("chrome-extension")
  // returns true and SchemeIs("https") returns false.
  virtual bool SchemeIs(std::string_view scheme) const = 0;

  // Returns the site URL associated with all of the documents and workers in
  // this principal, as described above.
  //
  // Compared to the content-internal AgentClusterKey, this URL might have been
  // overridden from the actual URL of the content in cases that involve
  // effective URLs such as hosted apps. The AgentClusterKey is always computed
  // with the real URL, as it is a web spec concept and effective URLs are not
  // part of the spec.
  //
  // NOTE: In most cases, code should be performing checks against the origin
  // returned by |RenderFrameHost::GetLastCommittedOrigin()|. In contrast, the
  // GURL returned by |GetDeprecatedSiteURL()| should not be considered
  // authoritative because:
  // - A SiteInstance can host pages from multiple sites if site isolation
  //   is not enabled (e.g., on Android) and the SiteInstance isn't hosting
  //   pages that require process isolation (e.g. WebUI or extensions).
  // - With site isolation but not origin isolation, the site URL is not an
  //   origin: while often derived from the origin, it only contains the scheme
  //   and the eTLD + 1, i.e. an origin with the host
  //   "deeply.nested.subdomain.example.com" corresponds to a site URL with the
  //   host "example.com".
  // - When origin isolation is in use, there may be multiple SiteInstances
  //   with the same GetDeprecatedSiteURL() but that differ in other
  //   SecurityPrincipal properties, such as whether the principal is for PDF
  //   or sandboxed content.
  //
  // DEPRECATED: Prefer RenderFrameHost::GetLastCommittedOrigin() or more
  // specific SecurityPrincipal methods (e.g., SchemeIs()) over inspecting the
  // site URL directly. This method is provided for callers that have not yet
  // been migrated away from the site URL.
  // Cases like hosted apps should keep using the site URL for now for
  // resolving effective URLs. We expect no new use cases of effective URLs to
  // arise, and for most use cases of effective URLs to go away over time
  // (for example, hosted apps are deprecated and will be removed).
  // More SecurityPrincipal properties will be added over time, which should
  // cover more Site URL use cases. If there's a property that's missing on
  // SecurityPrincipal and that's needed for a new feature, reach out to
  // //content/OWNERS to discuss how to add it.
  virtual const GURL& GetDeprecatedSiteURL() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_
