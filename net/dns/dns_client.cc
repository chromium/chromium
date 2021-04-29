// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_allocator.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_factory.h"

namespace net {

namespace {

// Creates NetLog parameters for the DNS_CONFIG_CHANGED event.
base::Value NetLogDnsConfigParams(const DnsConfig* config) {
  if (!config)
    return base::Value(base::Value::Type::DICTIONARY);

  return config->ToValue();
}

bool IsEqual(const base::Optional<DnsConfig>& c1, const DnsConfig* c2) {
  if (!c1.has_value() && c2 == nullptr)
    return true;

  if (!c1.has_value() || c2 == nullptr)
    return false;

  return c1.value() == *c2;
}

void UpdateConfigForDohUpgrade(DnsConfig* config) {
  bool has_doh_servers = !config->dns_over_https_servers.empty();
  // Do not attempt upgrade when there are already DoH servers specified or
  // when there are aspects of the system DNS config that are unhandled.
  if (!config->unhandled_options && config->allow_dns_over_https_upgrade &&
      !has_doh_servers &&
      config->secure_dns_mode == SecureDnsMode::kAutomatic) {
    // If we're in strict mode on Android, only attempt to upgrade the
    // specified DoT hostname.
    if (!config->dns_over_tls_hostname.empty()) {
      config->dns_over_https_servers = GetDohUpgradeServersFromDotHostname(
          config->dns_over_tls_hostname, config->disabled_upgrade_providers);
      has_doh_servers = !config->dns_over_https_servers.empty();
      UMA_HISTOGRAM_BOOLEAN("Net.DNS.UpgradeConfig.DotUpgradeSucceeded",
                            has_doh_servers);
    } else {
      bool all_local = true;
      for (const auto& server : config->nameservers) {
        if (server.address().IsPubliclyRoutable()) {
          all_local = false;
          break;
        }
      }
      UMA_HISTOGRAM_BOOLEAN("Net.DNS.UpgradeConfig.HasPublicInsecureNameserver",
                            !all_local);

      config->dns_over_https_servers = GetDohUpgradeServersFromNameservers(
          config->nameservers, config->disabled_upgrade_providers);
      has_doh_servers = !config->dns_over_https_servers.empty();
      UMA_HISTOGRAM_BOOLEAN("Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded",
                            has_doh_servers);
    }
  } else {
    UMA_HISTOGRAM_BOOLEAN("Net.DNS.UpgradeConfig.Ineligible.DohSpecified",
                          has_doh_servers);
    UMA_HISTOGRAM_BOOLEAN("Net.DNS.UpgradeConfig.Ineligible.UnhandledOptions",
                          config->unhandled_options);
  }
}

class DnsClientImpl : public DnsClient {
 public:
  DnsClientImpl(NetLog* net_log,
                ClientSocketFactory* socket_factory,
                const RandIntCallback& rand_int_callback)
      : net_log_(net_log),
        socket_factory_(socket_factory),
        rand_int_callback_(rand_int_callback) {}

  ~DnsClientImpl() override = default;

  bool CanUseSecureDnsTransactions() const override {
    const DnsConfig* config = GetEffectiveConfig();
    return config && config->dns_over_https_servers.size() > 0;
  }

  bool CanUseInsecureDnsTransactions() const override {
    const DnsConfig* config = GetEffectiveConfig();
    return config && config->nameservers.size() > 0 && insecure_enabled_ &&
           !config->unhandled_options && !config->dns_over_tls_active;
  }

  void SetInsecureEnabled(bool enabled) override {
    insecure_enabled_ = enabled;
  }

  bool FallbackFromSecureTransactionPreferred(
      ResolveContext* context) const override {
    if (!CanUseSecureDnsTransactions())
      return true;

    DCHECK(session_);  // Should be true if CanUseSecureDnsTransactions() true.
    return context->NumAvailableDohServers(session_.get()) == 0;
  }

  bool FallbackFromInsecureTransactionPreferred() const override {
    return !CanUseInsecureDnsTransactions() ||
           insecure_fallback_failures_ >= kMaxInsecureFallbackFailures;
  }

  bool SetSystemConfig(base::Optional<DnsConfig> system_config) override {
    if (system_config == system_config_)
      return false;

    system_config_ = std::move(system_config);

    return UpdateDnsConfig();
  }

  bool SetConfigOverrides(DnsConfigOverrides config_overrides) override {
    if (config_overrides == config_overrides_)
      return false;

    config_overrides_ = std::move(config_overrides);

    return UpdateDnsConfig();
  }

  void ReplaceCurrentSession() override {
    if (!session_)
      return;

    UpdateSession(session_->config());
  }

  DnsSession* GetCurrentSession() override { return session_.get(); }

  const DnsConfig* GetEffectiveConfig() const override {
    if (!session_)
      return nullptr;

    DCHECK(session_->config().IsValid());
    return &session_->config();
  }

  const DnsHosts* GetHosts() const override {
    const DnsConfig* config = GetEffectiveConfig();
    if (!config)
      return nullptr;

    return &config->hosts;
  }

  DnsTransactionFactory* GetTransactionFactory() override {
    return session_.get() ? factory_.get() : nullptr;
  }

  AddressSorter* GetAddressSorter() override { return address_sorter_.get(); }

  void IncrementInsecureFallbackFailures() override {
    ++insecure_fallback_failures_;
  }

  void ClearInsecureFallbackFailures() override {
    insecure_fallback_failures_ = 0;
  }

  base::Optional<DnsConfig> GetSystemConfigForTesting() const override {
    return system_config_;
  }

  DnsConfigOverrides GetConfigOverridesForTesting() const override {
    return config_overrides_;
  }

  void SetTransactionFactoryForTesting(
      std::unique_ptr<DnsTransactionFactory> factory) override {
    factory_ = std::move(factory);
  }

 private:
  base::Optional<DnsConfig> BuildEffectiveConfig() const {
    DnsConfig config;
    if (config_overrides_.OverridesEverything()) {
      config = config_overrides_.ApplyOverrides(DnsConfig());
    } else {
      if (!system_config_)
        return base::nullopt;

      config = config_overrides_.ApplyOverrides(system_config_.value());
    }

    UpdateConfigForDohUpgrade(&config);

    // TODO(ericorth): Consider keeping a separate DnsConfig for pure Chrome-
    // produced configs to allow respecting all fields like |unhandled_options|
    // while still being able to fallback to system config for DoH.
    // For now, clear the nameservers for extra security if parts of the system
    // config are unhandled.
    if (config.unhandled_options)
      config.nameservers.clear();

    if (!config.IsValid())
      return base::nullopt;

    return config;
  }

  bool UpdateDnsConfig() {
    base::Optional<DnsConfig> new_effective_config = BuildEffectiveConfig();

    if (IsEqual(new_effective_config, GetEffectiveConfig()))
      return false;

    insecure_fallback_failures_ = 0;
    UpdateSession(std::move(new_effective_config));

    if (net_log_) {
      net_log_->AddGlobalEntry(NetLogEventType::DNS_CONFIG_CHANGED, [&] {
        return NetLogDnsConfigParams(GetEffectiveConfig());
      });
    }

    return true;
  }

  void UpdateSession(base::Optional<DnsConfig> new_effective_config) {
    factory_.reset();
    session_ = nullptr;

    if (new_effective_config) {
      DCHECK(new_effective_config.value().IsValid());

      auto socket_allocator = std::make_unique<DnsSocketAllocator>(
          socket_factory_, new_effective_config.value().nameservers, net_log_);
      session_ = new DnsSession(std::move(new_effective_config).value(),
                                std::move(socket_allocator), rand_int_callback_,
                                net_log_);
      factory_ = DnsTransactionFactory::CreateFactory(session_.get());
    }
  }

  bool insecure_enabled_ = false;
  int insecure_fallback_failures_ = 0;

  base::Optional<DnsConfig> system_config_;
  DnsConfigOverrides config_overrides_;

  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_ =
      AddressSorter::CreateAddressSorter();

  NetLog* net_log_;

  ClientSocketFactory* socket_factory_;
  const RandIntCallback rand_int_callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsClientImpl);
};

}  // namespace

// static
std::unique_ptr<DnsClient> DnsClient::CreateClient(NetLog* net_log) {
  return std::make_unique<DnsClientImpl>(
      net_log, ClientSocketFactory::GetDefaultFactory(),
      base::BindRepeating(&base::RandInt));
}

// static
std::unique_ptr<DnsClient> DnsClient::CreateClientForTesting(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    const RandIntCallback& rand_int_callback) {
  return std::make_unique<DnsClientImpl>(net_log, socket_factory,
                                         rand_int_callback);
}

}  // namespace net
