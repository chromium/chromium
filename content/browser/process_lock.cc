// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_lock.h"

#include "content/public/browser/browser_thread.h"

namespace content {

// static
ProcessLock ProcessLock::CreateAllowAnySite(
    const StoragePartitionConfig& storage_partition_config,
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  return ProcessLock(
      SiteInfo(GURL(), GURL(), false, storage_partition_config,
               web_exposed_isolation_info, /* is_guest */ false,
               /* does_site_request_dedicated_process_for_coop */ false,
               /* is_jit_disabled */ false, /* is_pdf */ false));
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

ProcessLock::ProcessLock(const SiteInfo& site_info) : site_info_(site_info) {}

ProcessLock::ProcessLock() = default;

ProcessLock::ProcessLock(const ProcessLock&) = default;

ProcessLock& ProcessLock::operator=(const ProcessLock&) = default;

ProcessLock::~ProcessLock() = default;

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
  return site_info_.has_value() && site_info_->web_exposed_isolation_info() ==
                                       site_info.web_exposed_isolation_info();
}

bool ProcessLock::operator==(const ProcessLock& rhs) const {
  // As we add additional features to SiteInfo, we'll expand this comparison.
  // Note that this should *not* compare site_url() values from the SiteInfo,
  // since those include effective URLs which may differ even if the actual
  // document origins match. We use process_lock_url() comparisons to account
  // for this.
  bool is_equal = site_info_.has_value() == rhs.site_info_.has_value();

  if (is_equal && site_info_.has_value()) {
    is_equal =
        site_info_->process_lock_url() == rhs.site_info_->process_lock_url() &&
        site_info_->requires_origin_keyed_process() ==
            rhs.site_info_->requires_origin_keyed_process() &&
        site_info_->is_pdf() == rhs.site_info_->is_pdf() &&
        (site_info_->web_exposed_isolation_info() ==
         rhs.site_info_->web_exposed_isolation_info());
  }

  return is_equal;
}

bool ProcessLock::operator!=(const ProcessLock& rhs) const {
  return !(*this == rhs);
}

bool ProcessLock::operator<(const ProcessLock& rhs) const {
  const auto this_is_origin_keyed_process = is_origin_keyed_process();
  const auto this_is_pdf = is_pdf();
  const auto this_web_exposed_isolation_info = web_exposed_isolation_info();
  const auto rhs_is_origin_keyed_process = is_origin_keyed_process();
  const auto rhs_is_pdf = rhs.is_pdf();
  const auto rhs_web_exposed_isolation_info = web_exposed_isolation_info();
  return std::tie(lock_url(), this_is_origin_keyed_process, this_is_pdf,
                  this_web_exposed_isolation_info) <
         std::tie(rhs.lock_url(), rhs_is_origin_keyed_process, rhs_is_pdf,
                  rhs_web_exposed_isolation_info);
}

std::string ProcessLock::ToString() const {
  std::string ret = "{ ";

  if (site_info_.has_value()) {
    ret += lock_url().possibly_invalid_spec();

    if (is_origin_keyed_process())
      ret += " origin-keyed";

    if (is_pdf())
      ret += " pdf";

    if (web_exposed_isolation_info().is_isolated()) {
      ret += " cross-origin-isolated";
      if (web_exposed_isolation_info().is_isolated_application())
        ret += "-application";
      ret += " coi-origin='" +
             web_exposed_isolation_info().origin().GetDebugString() + "'";
    }
    if (!storage_partition_config().is_default()) {
      ret += ", partition=" + storage_partition_config().partition_domain() +
             "." + storage_partition_config().partition_name();
      if (storage_partition_config().in_memory())
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
