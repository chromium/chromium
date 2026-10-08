// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_broker/resource_broker_host.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

constexpr base::TimeDelta kCrashWindow = base::Minutes(10);
constexpr size_t kMaxCrashCount = 3;
constexpr base::TimeDelta kRelaunchCooldown = base::Seconds(10);

std::unique_ptr<ResourceBrokerHost>& GetInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<ResourceBrokerHost>> instance;
  return *instance;
}

}  // namespace

// static
ResourceBrokerHost& ResourceBrokerHost::GetInstance() {
  auto& instance_storage = GetInstanceStorage();
  if (!instance_storage) {
    instance_storage = base::WrapUnique(new ResourceBrokerHost());
  }
  return *instance_storage;
}

ResourceBrokerHost::ResourceBrokerHost() = default;

ResourceBrokerHost::~ResourceBrokerHost() = default;

resource_broker::mojom::ResourceBrokerService*
ResourceBrokerHost::GetService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (disabled_for_session_ ||
      !base::FeatureList::IsEnabled(features::kResourceBroker)) {
    return nullptr;
  }

  if (!service_remote_) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (!last_disconnect_time_.is_null() &&
        now - last_disconnect_time_ < kRelaunchCooldown) {
      return nullptr;
    }
    LaunchService();
  }

  return service_remote_.is_bound() ? service_remote_.get() : nullptr;
}

base::UnguessableToken ResourceBrokerHost::GetSessionNonce() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_nonce_;
}

// static
void ResourceBrokerHost::ResetForTesting() {
  GetInstanceStorage().reset();
}

void ResourceBrokerHost::MaybeFlushForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (service_remote_) {
    service_remote_.FlushForTesting();  // IN-TEST
  }
}

void ResourceBrokerHost::SetServiceLauncherForTesting(
    ServiceLauncherForTesting launcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_launcher_for_testing_ = std::move(launcher);
}

// static
resource_broker::mojom::BrokerConfigPtr ResourceBrokerHost::CreateConfig(
    const base::UnguessableToken& session_nonce) {
  CHECK(!session_nonce.is_empty());
  auto config = resource_broker::mojom::BrokerConfig::New();
  config->session_nonce = session_nonce;
  return config;
}

void ResourceBrokerHost::LaunchService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initial_launch_recorded_) {
    initial_launch_recorded_ = true;
    base::UmaHistogramEnumeration("ResourceBroker.LifecycleEvent",
                                  ResourceBrokerLifecycleEvent::kLaunched);
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  if (service_launcher_for_testing_) {
    service_launcher_for_testing_.Run(std::move(receiver));
  } else {
    ServiceProcessHost::Launch(std::move(receiver),
                               ServiceProcessHost::Options()
                                   .WithDisplayName(u"Resource Broker Service")
                                   .Pass());
  }

  // base::Unretained is safe because `this` owns `service_remote_`.
  service_remote_.set_disconnect_handler(base::BindOnce(
      &ResourceBrokerHost::OnServiceDisconnected, base::Unretained(this)));

  // Prepare and send config.
  auto config = CreateConfig(session_nonce_);

  service_remote_->Initialize(std::move(config));
}

void ResourceBrokerHost::OnServiceDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_remote_.reset();

  if (GetContentClient() && GetContentClient()->browser() &&
      GetContentClient()->browser()->IsShuttingDown()) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  last_disconnect_time_ = now;

  // TODO(crbug.com/560232768): Currently every disconnection is treated as an
  // abnormal crash. In the future, inspect the process exit status (e.g. via
  // ServiceProcessHost termination tracking or an observer) so that clean exits
  // (such as idle timeouts or graceful browser shutdown) are not conflated with
  // crashes, exhausting the crash-loop quota.

  // Prune crashes older than 10 minutes.
  std::erase_if(crash_timestamps_,
                [now](base::TimeTicks t) { return now - t > kCrashWindow; });

  crash_timestamps_.push_back(now);

  if (!disabled_for_session_ && crash_timestamps_.size() >= kMaxCrashCount) {
    disabled_for_session_ = true;
    base::UmaHistogramEnumeration(
        "ResourceBroker.LifecycleEvent",
        ResourceBrokerLifecycleEvent::kDisabledDueToCrashLoop);
  }
}

}  // namespace content
