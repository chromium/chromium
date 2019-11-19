// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config_upgrader.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/logging.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/host_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

namespace {

// The name of the command-line switch used to specify the host configuration
// file to use.
const char kHostConfigSwitchName[] = "host-config";

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay.  The interpretation of this value depends on
    // always_use_initial_delay.  It's either how long we wait between
    // requests before backoff starts, or how much we delay the first request
    // after backoff starts.
    5 * 1000,

    // Factor by which the waiting time will be multiplied.
    2.0,

    // Fuzzing factor. ex: 0.1 will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time (ms) we are willing to delay our request, -1
    // for no maximum.
    60 * 1000,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // If true, we always use a delay of initial_delay_ms, even before
    // we've seen num_errors_to_ignore errors.  Otherwise, initial_delay_ms
    // is the first delay once we start exponential backoff.
    false,
};

constexpr int kMaximumFailures = 10;

}  // namespace

// static
int HostConfigUpgrader::UpgradeConfigFile() {
  return HostConfigUpgrader().DoUpgrade();
}

HostConfigUpgrader::HostConfigUpgrader()
    : backoff_entry_(&kRetryBackoffPolicy) {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "HostConfigUpgrader");
  mojo::core::Init();
}

HostConfigUpgrader::~HostConfigUpgrader() = default;

int HostConfigUpgrader::DoUpgrade() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kHostConfigSwitchName)) {
    LOG(ERROR) << "No host config provided.";
    return 1;
  }
  config_path_ = command_line->GetSwitchValuePath(kHostConfigSwitchName);
  config_ = HostConfigFromJsonFile(config_path_);
  if (!config_) {
    LOG(ERROR) << "Failed to read host config.";
    return 1;
  }
  const std::string* refresh_token =
      config_->FindStringKey(kOAuthRefreshTokenConfigPath);
  if (!refresh_token) {
    LOG(ERROR) << "Refresh token not found in host config.";
    return 1;
  }

  refresh_token_ = *refresh_token;

  if (config_->FindKey(kIsFtlTokenConfigPath)) {
    HOST_LOG << "Host config is already upgraded.";
    return 0;
  }

  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get());
  auto url_loader_factory_owner =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter);

  token_exchanger_ = std::make_unique<OfflineTokenExchanger>(
      url_loader_factory_owner->GetURLLoaderFactory());

  RequestExchangeToken();

  run_loop_.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return exit_code_;
}

void HostConfigUpgrader::RequestExchangeToken() {
  HOST_LOG << "Requesting token exchange.";

  // Unretained() is safe, because |this| is destroyed after the RunLoop
  // quits and the ThreadPool is shut down.
  token_exchanger_->ExchangeRefreshToken(
      refresh_token_, base::BindOnce(&HostConfigUpgrader::OnTokenExchanged,
                                     base::Unretained(this)));
}

void HostConfigUpgrader::OnTokenExchanged(OfflineTokenExchanger::Status status,
                                          const std::string& refresh_token) {
  if (status == OfflineTokenExchanger::FAILURE) {
    backoff_entry_.InformOfRequest(false);
    MaybeWaitAndRetry();
    return;
  }

  // Mark config as upgraded and write to disk (whether or not the token was
  // actually exchanged). The value is not important, only the presence of the
  // key is used.
  config_->SetIntKey(kIsFtlTokenConfigPath, 1);

  if (status == OfflineTokenExchanger::NO_EXCHANGE) {
    HOST_LOG << "No exchange needed, writing new config to mark as upgraded.";
  } else {
    DCHECK(status == OfflineTokenExchanger::SUCCESS);
    HOST_LOG << "Obtained new refresh token for host config.";
    config_->SetStringKey(kOAuthRefreshTokenConfigPath, refresh_token);
  }

  WriteConfig();

  run_loop_.Quit();
}

void HostConfigUpgrader::MaybeWaitAndRetry() {
  if (backoff_entry_.failure_count() >= kMaximumFailures) {
    LOG(ERROR) << "Too many failures.";
    exit_code_ = 1;
    run_loop_.Quit();
    return;
  }

  base::TimeDelta wait_time = backoff_entry_.GetTimeUntilRelease();
  HOST_LOG << "Retrying after " << wait_time;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HostConfigUpgrader::RequestExchangeToken,
                     base::Unretained(this)),
      wait_time);
}

void HostConfigUpgrader::WriteConfig() {
  // Don't use HostConfigToJsonFile(), because that might not preserve the
  // file permissions and ACLs which are required on MacOS. Instead, open the
  // config file for writing, and overwrite it in place.
  std::string serialized = HostConfigToJson(*config_);
  base::File config_file(
      config_path_, base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_WRITE);
  int data_length = static_cast<int>(serialized.length());
  int bytes_written = config_file.Write(0, serialized.data(), data_length);
  config_file.Flush();
  if (bytes_written < data_length) {
    LOG(ERROR) << "Failed to write new config.";
    exit_code_ = 1;
  } else {
    HOST_LOG << "Successfully written host config.";
  }
}

}  // namespace remoting
