// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_CPSP_CHILD_PROCESS_SECURITY_POLICY_SHIM_H_
#define CONTENT_BROWSER_SECURITY_CPSP_CHILD_PROCESS_SECURITY_POLICY_SHIM_H_

#include <memory>
#include <vector>

#include "url/gurl.h"
#include "url/origin.h"

namespace content::rust::child_process_security_policy {

// Shims to allow Rust to perform C++ operations that are currently not well
// supported by CXX.
// TODO(https://crbug.com/40226863): Eventually, these shims should be removed
// and replaced by Crubit-generated bindings.

// Allows Rust to push `url::Origin` to a `std::vector<url::Origin>` since
// CXX does not natively support pushing opaque C++ types.
void PushOriginToVector(const url::Origin& origin,
                        std::vector<url::Origin>& vec) noexcept;

// Allows Rust to call `SiteInfo::GetSiteForOrigin` and get the result as a
// heap-allocated object, as Rust cannot construct `GURL` directly by value.
std::unique_ptr<GURL> GetSiteForOrigin(const url::Origin& origin);

// Helper to remove the trailing dot from the provided URL's host, if present,
// and return the resulting URL as a new heap-allocated object for Rust.
// Used when looking for matching legacy isolated origins.
std::unique_ptr<GURL> RemoveTrailingDotFromUrlIfNecessary(const GURL& site_url);

// Reconstructs the given origin with its default port if it currently has a
// non-default port. Used for resolving matches for wildcards (i.e.,
// where isolate_all_subdomains is true). Returns a heap-allocated object for
// Rust.
std::unique_ptr<url::Origin> CreateOriginWithDefaultPortIfNecessary(
    const url::Origin& origin);

}  // namespace content::rust::child_process_security_policy

#endif  // CONTENT_BROWSER_SECURITY_CPSP_CHILD_PROCESS_SECURITY_POLICY_SHIM_H_
