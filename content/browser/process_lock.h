// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_LOCK_H_
#define CONTENT_BROWSER_PROCESS_LOCK_H_

#include <optional>

#include "content/browser/site_info.h"
#include "content/browser/url_info.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "url/origin.h"

namespace content {

class IsolationContext;

// ProcessLock is a core part of Site Isolation, which is used to determine
// which documents are allowed to load in a process and which site data the
// process is allowed to access, based on the SiteInfo principal.
//
// If a process has a ProcessLock in the "invalid" state, then no SiteInstances
// have been associated with the process and access should not be granted to
// anything.
//
// Once a process is associated with its first SiteInstance, it transitions to
// the "locked_to_site" or "allow_any_site" state depending on whether the
// SiteInstance requires the process to be locked to a specific site or not.
// If the SiteInstance does not require the process to be locked to a site, the
// process will transition to the "allow_any_site" state and will allow any
// site to commit in the process. Such a process can later be upgraded to the
// "locked_to_site" state if something later determines that the process should
// only allow access to a single site, but only if it hasn't otherwise been used
// to render content. Once the process is in the "locked_to_site" state, it will
// not be able to access site data from other sites.
//
// ProcessLock is currently defined in terms of a single SiteInfo with a process
// lock URL, but it could be possible to define it in terms of multiple
// SiteInfos that are compatible with each other.
class CONTENT_EXPORT ProcessLock {
 public:
  // Create a lock that that represents a process that is associated with at
  // least one SiteInstance, but is not locked to a specific site. Any request
  // that wants to commit in this process must have a StoragePartitionConfig
  // and web-exposed isolation information (COOP/COEP, for example) that
  // match the values used to create this lock.
  static ProcessLock CreateAllowAnySite(
      const StoragePartitionConfig& storage_partition_config,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  // Create a lock for a specific UrlInfo. This method can be called from both
  // the UI and IO threads. Locks created with the same parameters must always
  // be considered equal independent of what thread they are called on. Special
  // care must be taken since SiteInfos created on different threads don't
  // always have the same contents for all their fields (e.g. site_url field is
  // thread dependent).
  static ProcessLock Create(const IsolationContext& isolation_context,
                            const UrlInfo& url_info);

  // Returns a ProcessLock representing what the given |site_info| requires.
  // Note that this may be different from the actual ProcessLock of the
  // resulting process, in cases where a locked process is not required (e.g.,
  // SiteInfos for http://unisolated.invalid).
  static ProcessLock FromSiteInfo(const SiteInfo& site_info);

  ProcessLock();
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

  // Returns the site URL of the SiteInfo with which the lock was constructed.
  // Prefer comparing ProcessLocks directly or using lock_url(), unless you
  // care about effective URLs.
  const GURL site_url() const {
    return site_info_.has_value() ? site_info_->site_url() : GURL();
  }

  // Returns the AgentClusterKey shared by agents allowed in this ProcessLock.
  std::optional<AgentClusterKey> agent_cluster_key() const {
    return site_info_.has_value() ? site_info_->agent_cluster_key()
                                  : std::nullopt;
  }

  // Returns whether this ProcessLock is specific to an origin rather than
  // including subdomains, such as due to opt-in origin isolation. This resolves
  // an ambiguity of whether a process with a lock_url() like
  // "https://foo.example" is allowed to include "https://sub.foo.example" or
  // not.
  bool is_origin_keyed_process() const {
    return site_info_.has_value() &&
           site_info_->requires_origin_keyed_process();
  }

  // True if this ProcessLock is for a sandboxed iframe without
  // allow-same-origin.
  // TODO(wjmaclean): This function's return type could mutate to an enum in
  // future if required for sandboxed iframes that are restricted with different
  // sandbox flags.
  bool is_sandboxed() const {
    return site_info_.has_value() && site_info_->is_sandboxed();
  }

  // If this ProcessLock is for a sandboxed iframe without allow-same-origin,
  // and per-document grouping has been enabled for kIsolateSandboxedIframes,
  // then each SiteInfo will have a unique sandbox id encoded as part of the
  // lock. If per-document grouping is not enabled, this returns
  // UrlInfo::kInvalidUniqueSandboxId.
  int unique_sandbox_id() const {
    return (site_info_.has_value() ? site_info_->unique_sandbox_id()
                                   : UrlInfo::kInvalidUniqueSandboxId);
  }

  // Returns whether this ProcessLock is specific to PDF contents.
  bool is_pdf() const { return site_info_.has_value() && site_info_->is_pdf(); }

  // Returns whether this ProcessLock can only be used for error pages.
  bool is_error_page() const {
    return site_info_.has_value() && site_info_->is_error_page();
  }

  // Returns whether this ProcessLock is used for a <webview> guest process.
  // This may be false for other types of GuestView.
  bool is_guest() const {
    return site_info_.has_value() && site_info_->is_guest();
  }

  // Returns whether this ProcessLock is used for a process that exclusively
  // hosts content inside a <fencedframe>.
  bool is_fenced() const {
    return site_info_.has_value() && site_info_->is_fenced();
  }

  // Returns the StoragePartitionConfig that corresponds to the SiteInfo the
  // lock is used with.
  StoragePartitionConfig GetStoragePartitionConfig() const;

  // Returns the cross-origin isolation mode of the BrowsingInstance that all
  // agents allowed in this ProcessLock belong to. See
  // https://html.spec.whatwg.org/multipage/document-sequences.html#cross-origin-isolation-mode
  // This is tracked on ProcessLock because a RenderProcessHost can host only
  // cross-origin isolated agents or only non-cross-origin isolated agents, not
  // both.
  WebExposedIsolationInfo GetWebExposedIsolationInfo() const;

  // Returns the cross-origin isolated capability of all agents allowed in this
  // ProcessLock, without taking into account the 'cross-origin-isolated'
  // permissions policy. This ignores permissions policy because it's currently
  // possible for agents with the same ProcessLock to have different
  // 'cross-origin-isolated' permission policies. This can return a lower
  // isolation level than `GetWebExposedIsolationInfo()` if this ProcessLock
  // hosts agents that are cross-origin to a top-level document with the
  // 'isolated application' isolation level. See
  // https://html.spec.whatwg.org/multipage/webappapis.html#dom-crossoriginisolated
  WebExposedIsolationLevel GetWebExposedIsolationLevel() const;

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
  // Returns true if the web-exposed isolation level in this lock is set and
  // matches (or exceeds) the level set in |site_info|.|.
  bool IsCompatibleWithWebExposedIsolation(const SiteInfo& site_info) const;

  bool operator==(const ProcessLock& rhs) const;
  bool operator!=(const ProcessLock& rhs) const;
  // Defined to allow this object to act as a key for std::map.
  bool operator<(const ProcessLock& rhs) const;

  std::string ToString() const;

 private:
  explicit ProcessLock(const SiteInfo& site_info);

  // TODO(creis): Consider tracking multiple compatible SiteInfos in ProcessLock
  // (e.g., multiple sites when Site Isolation is disabled). This can better
  // restrict what the process has access to in cases that we currently use an
  // allows-any-site ProcessLock.
  std::optional<SiteInfo> site_info_;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const ProcessLock& process_lock);

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_LOCK_H_
