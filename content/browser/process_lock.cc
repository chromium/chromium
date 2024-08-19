// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_lock.h"

#include "base/strings/stringprintf.h"
#include "content/browser/agent_cluster_key.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_exposed_isolation_level.h"

namespace content {

// static
ProcessLock ProcessLock::CreateAllowAnySite(
    const StoragePartitionConfig& storage_partition_config,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  WebExposedIsolationLevel web_exposed_isolation_level =
      SiteInfo::ComputeWebExposedIsolationLevelForEmptySite(
          web_exposed_isolation_info);

  return ProcessLock(SiteInfo(
      /*site_url=*/GURL(), /*process_lock_url=*/GURL(),
      /*requires_origin_keyed_process=*/false,
      /*requires_origin_keyed_process_by_default=*/false,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      storage_partition_config, web_exposed_isolation_info,
      web_exposed_isolation_level, /*is_guest=*/false,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false, std::nullopt));
}

// static
ProcessLock ProcessLock::Create(const IsolationContext& isolation_context,
                                const UrlInfo& url_info) {
  DCHECK(url_info.storage_partition_config.has_value());
  if (BrowserThread::CurrentlyOn(BrowserThread::UI))
    return ProcessLock(SiteInfo::Create(isolation_context, url_info));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // On the IO thread we need to use a special SiteInfo creation method because
  // we cannot properly compute some SiteInfo fields on that thread.
  // ProcessLocks must always match no matter which thread they were created on,
  // but the SiteInfo objects used to create them may not always match.
  return ProcessLock(SiteInfo::CreateOnIOThread(isolation_context, url_info));
}

// static
ProcessLock ProcessLock::FromSiteInfo(const SiteInfo& site_info) {
  return ProcessLock(site_info);
}

ProcessLock::ProcessLock(const SiteInfo& site_info) : site_info_(site_info) {}

ProcessLock::ProcessLock() = default;

ProcessLock::ProcessLock(const ProcessLock&) = default;

ProcessLock& ProcessLock::operator=(const ProcessLock&) = default;

ProcessLock::~ProcessLock() = default;

StoragePartitionConfig ProcessLock::GetStoragePartitionConfig() const {
  DCHECK(site_info_.has_value());
  return site_info_->storage_partition_config();
}

WebExposedIsolationInfo ProcessLock::GetWebExposedIsolationInfo() const {
  return site_info_.has_value() ? site_info_->web_exposed_isolation_info()
                                : WebExposedIsolationInfo::CreateNonIsolated();
}

WebExposedIsolationLevel ProcessLock::GetWebExposedIsolationLevel() const {
  return site_info_.has_value() ? site_info_->web_exposed_isolation_level()
                                : WebExposedIsolationLevel::kNotIsolated;
}

bool ProcessLock::IsASiteOrOrigin() const {
  const GURL lock_url = ProcessLock::lock_url();
  return lock_url.has_scheme() && lock_url.has_host() && lock_url.is_valid();
}

bool ProcessLock::HasOpaqueOrigin() const {
  DCHECK(is_locked_to_site());
  return url::Origin::Create(lock_url()).opaque();
}

bool ProcessLock::MatchesOrigin(const url::Origin& origin) const {
  url::Origin process_lock_origin = url::Origin::Create(lock_url());
  return origin == process_lock_origin;
}

bool ProcessLock::IsCompatibleWithWebExposedIsolation(
    const SiteInfo& site_info) const {
  if (!site_info_.has_value()) {
    return true;
  }

  // Check if the WebExposedIsolationInfos are compatible.
  if (site_info_->web_exposed_isolation_info() !=
      site_info.web_exposed_isolation_info()) {
    return false;
  }

  // Check if the CrossOriginIsolationKeys are compatible.
  //
  // TODO(crbug.com/349755777): Currently, this prevents a RenderProcessHost
  // with a ProcessLock created with ProcessLock::CreateAllowAnySite to be
  // reused for navigations to documents with DocumentIsolationPolicy, even if
  // the RenderProcessHost has not been used and it would be safe to reuse it.
  //
  // Unfortunately, ProcessLock::CreateAllowAnySite will result in the
  // associated RenderProcessHost to be marked as crossOriginIsolated or not,
  // depending on the passed WebExposedIsolationInfo. It cannot be set to a
  // different crossOriginIsolated status again (without removing checks that
  // the COI status of the process cannot change).
  //
  // Therefore, we need this check to avoid reusing a process matching a
  // ProcessLock created ProcessLock::CreateAllowAnySite, and triggering the COI
  // state change check in the renderer process.
  //
  // We should refactor how COI status is set in the renderer process, so that
  // unused RenderProcessHosts are not assigned a COI status. This will allow
  // them to be reused regardless of the COI status of the navigation.
  std::optional<AgentClusterKey::CrossOriginIsolationKey> this_coi_key =
      site_info_->agent_cluster_key()
          ? site_info_->agent_cluster_key()->GetCrossOriginIsolationKey()
          : std::nullopt;
  std::optional<AgentClusterKey::CrossOriginIsolationKey> other_coi_key =
      site_info.agent_cluster_key()
          ? site_info.agent_cluster_key()->GetCrossOriginIsolationKey()
          : std::nullopt;

  return this_coi_key == other_coi_key;
}

bool ProcessLock::operator==(const ProcessLock& rhs) const {
  if (site_info_.has_value() != rhs.site_info_.has_value())
    return false;

  if (!site_info_.has_value())  // Neither has a value, so they're equal.
    return true;

  // At this point, both `this` and `rhs` are known to have valid SiteInfos.
  // Here we proceed with a comparison almost identical to
  // SiteInfo::MakeSecurityPrincipalKey(), except that `site_url_` is excluded.
  return site_info_->ProcessLockCompareTo(rhs.site_info_.value()) == 0;
}

bool ProcessLock::operator!=(const ProcessLock& rhs) const {
  return !(*this == rhs);
}

bool ProcessLock::operator<(const ProcessLock& rhs) const {
  if (!site_info_.has_value() && !rhs.site_info_.has_value())
    return false;
  if (!site_info_.has_value())  // Here rhs.site_info_.has_value() is true.
    return true;
  if (!rhs.site_info_.has_value())  // Here site_info_.has_value() is true.
    return false;

  // At this point, both `this` and `rhs` are known to have valid SiteInfos.
  // Here we proceed with a comparison almost identical to
  // SiteInfo::MakeSecurityPrincipalKey(), except that `site_url_` is excluded.
  return site_info_->ProcessLockCompareTo(rhs.site_info_.value()) < 0;
}

std::string ProcessLock::ToString() const {
  std::string ret = "{ ";

  if (site_info_.has_value()) {
    ret += lock_url().possibly_invalid_spec();

    if (is_origin_keyed_process())
      ret += " origin-keyed";

    if (is_sandboxed()) {
      ret += " sandboxed";
      if (site_info_->unique_sandbox_id() != UrlInfo::kInvalidUniqueSandboxId)
        ret += base::StringPrintf(" (id=%d)", site_info_->unique_sandbox_id());
    }

    if (is_pdf())
      ret += " pdf";

    if (is_guest())
      ret += " guest";

    if (is_fenced())
      ret += " fenced";

    if (GetWebExposedIsolationInfo().is_isolated()) {
      ret += " cross-origin-isolated";
      if (GetWebExposedIsolationInfo().is_isolated_application())
        ret += "-application";
      ret += " coi-origin='" +
             GetWebExposedIsolationInfo().origin().GetDebugString() + "'";
    }

    if (!GetStoragePartitionConfig().is_default()) {
      ret += ", partition=" + GetStoragePartitionConfig().partition_domain() +
             "." + GetStoragePartitionConfig().partition_name();
      if (GetStoragePartitionConfig().in_memory())
        ret += ", in-memory";
    }
  } else {
    ret += " no-site-info";
  }
  ret += " }";

  return ret;
}

std::ostream& operator<<(std::ostream& out, const ProcessLock& process_lock) {
  return out << process_lock.ToString();
}

}  // namespace content
