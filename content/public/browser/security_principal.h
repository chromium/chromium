// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_

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

  // Get the StoragePartitionConfig, which describes the StoragePartition this
  // SecurityPrincipal is associated with.  For example, this will correspond to
  // a non-default StoragePartition for <webview> guests.
  virtual const StoragePartitionConfig& GetStoragePartitionConfig() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_PRINCIPAL_H_
