// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/legacy_tls_config_distributor.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "crypto/sha2.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace network {

namespace {

// Helper to guarantee |notify_callback| is run, even if |process_callback|
// no-ops due to the worker pool doing the parsing outliving the
// LegacyTLSConfigDistributor.
void ProcessParsedLegacyTLSConfig(
    base::OnceCallback<void(scoped_refptr<LegacyTLSExperimentConfig>)>
        process_callback,
    base::OnceClosure notify_callback,
    scoped_refptr<LegacyTLSExperimentConfig> config) {
  std::move(process_callback).Run(std::move(config));
  std::move(notify_callback).Run();
}

}  // namespace

LegacyTLSExperimentConfig::LegacyTLSExperimentConfig() = default;
LegacyTLSExperimentConfig::~LegacyTLSExperimentConfig() = default;

// static
scoped_refptr<LegacyTLSExperimentConfig> LegacyTLSExperimentConfig::Parse(
    const std::string& data) {
  auto config = base::MakeRefCounted<LegacyTLSExperimentConfig>();
  if (data.empty() || !config->proto_.ParseFromString(data))
    return nullptr;
  return config;
}

bool LegacyTLSExperimentConfig::ShouldSuppressLegacyTLSWarning(
    const std::string& hostname) const {
  // Match on eTLD+1 rather than full hostname (to account for subdomains and
  // redirects). If no registrable domain is found, default to using the
  // hostname as-is.
  auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty())
    domain = hostname;

  // Convert bytes from crypto::SHA256 so we can compare to the proto contents.
  std::string host_hash_bytes = crypto::SHA256HashString(domain);
  std::string host_hash = base::ToLowerASCII(
      base::HexEncode(host_hash_bytes.data(), host_hash_bytes.size()));
  const auto& control_site_hashes = proto_.control_site_hashes();

  // Perform binary search on the sorted list of control site hashes to check
  // if the input URL's hostname is included.
  auto lower = std::lower_bound(control_site_hashes.begin(),
                                control_site_hashes.end(), host_hash);

  return lower != control_site_hashes.end() && *lower == host_hash;
}

LegacyTLSConfigDistributor::LegacyTLSConfigDistributor() = default;
LegacyTLSConfigDistributor::~LegacyTLSConfigDistributor() = default;

void LegacyTLSConfigDistributor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LegacyTLSConfigDistributor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LegacyTLSConfigDistributor::OnNewLegacyTLSConfig(
    base::span<const uint8_t> config,
    base::OnceClosure callback) {
  // Make a copy for the background task, since the underlying storage for the
  // span will go away.
  std::string config_string(reinterpret_cast<const char*>(config.data()),
                            config.size());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&LegacyTLSExperimentConfig::Parse,
                     std::move(config_string)),
      base::BindOnce(
          &ProcessParsedLegacyTLSConfig,
          base::BindOnce(&LegacyTLSConfigDistributor::OnLegacyTLSConfigParsed,
                         weak_factory_.GetWeakPtr()),
          std::move(callback)));
}

void LegacyTLSConfigDistributor::OnLegacyTLSConfigParsed(
    scoped_refptr<LegacyTLSExperimentConfig> config) {
  if (!config)
    return;  // Error parsing the config.

  config_ = std::move(config);

  for (auto& observer : observers_) {
    observer.OnNewLegacyTLSConfig(config_);
  }
}

}  // namespace network
