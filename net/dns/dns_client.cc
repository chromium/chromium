// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_factory.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

bool IsEqual(const std::optional<DnsConfig>& c1, const DnsConfig* c2) {
  if (!c1.has_value() && c2 == nullptr)
    return true;

  if (!c1.has_value() || c2 == nullptr)
    return false;

  return c1.value() == *c2;
}

void UpdateConfigForDohUpgrade(DnsConfig* config) {
  bool has_doh_servers = !config->doh_config.servers().empty();
  // Do not attempt upgrade when there are already DoH servers specified or
  // when there are aspects of the system DNS config that are unhandled.
  if (!config->unhandled_options && config->allow_dns_over_https_upgrade &&
      !has_doh_servers &&
      config->secure_dns_mode == SecureDnsMode::kAutomatic) {
    // If we're in strict mode on Android, only attempt to upgrade the
    // specified DoT hostname.
    if (!config->dns_over_tls_hostname.empty()) {
      config->doh_config = DnsOverHttpsConfig(
          GetDohUpgradeServersFromDotHostname(config->dns_over_tls_hostname));
      has_doh_servers = !config->doh_config.servers().empty();
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

      config->doh_config = DnsOverHttpsConfig(
          GetDohUpgradeServersFromNameservers(config->nameservers));
      has_doh_servers = !config->doh_config.servers().empty();
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
  DnsClientImpl(NetLog* net_log, const RandIntCallback& rand_int_callback)
      : net_log_(net_log), rand_int_callback_(rand_int_callback) {}

  DnsClientImpl(const DnsClientImpl&) = delete;
  DnsClientImpl& operator=(const DnsClientImpl&) = delete;

  ~DnsClientImpl() override = default;

  bool CanUseSecureDnsTransactions() const override {
    const DnsConfig* config = GetEffectiveConfig();
    return config && !config->doh_config.servers().empty();
  }

  bool CanUseInsecureDnsTransactions() const override {
    const DnsConfig* config = GetEffectiveConfig();
    return config && config->nameservers.size() > 0 && insecure_enabled_ &&
           !config->unhandled_options && !config->dns_over_tls_active;
  }

  bool CanQueryAdditionalTypesViaInsecureDns() const override {
    // Only useful information if insecure DNS is usable, so expect this to
    // never be called if that is not the case.
    DCHECK(CanUseInsecureDnsTransactions());

    return can_query_additional_types_via_insecure_;
  }

  void SetInsecureEnabled(bool enabled,
                          bool additional_types_enabled) override {
    insecure_enabled_ = enabled;
    can_query_additional_types_via_insecure_ = additional_types_enabled;
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

  bool SetSystemConfig(std::optional<DnsConfig> system_config) override {
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

  std::optional<std::vector<IPEndPoint>> GetPresetAddrs(
      const url::SchemeHostPort& endpoint) const override {
    DCHECK(endpoint.IsValid());
    if (!session_)
      return std::nullopt;
    const auto& servers = session_->config().doh_config.servers();
    auto it = base::ranges::find_if(servers, [&](const auto& server) {
      std::string uri;
      bool valid = uri_template::Expand(server.server_template(), {}, &uri);
      // Server templates are validated before being allowed into the config.
      DCHECK(valid);
      GURL gurl(uri);
      return url::SchemeHostPort(gurl) == endpoint;
    });
    if (it == servers.end())
      return std::nullopt;
    std::vector<IPEndPoint> combined;
    for (const IPAddressList& ips : it->endpoints()) {
      for (const IPAddress& ip : ips) {
        combined.emplace_back(ip, endpoint.port());
      }
    }
    return combined;
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

  base::Value::Dict GetDnsConfigAsValueForNetLog() const override {
    const DnsConfig* config = GetEffectiveConfig();
    if (config == nullptr)
      return base::Value::Dict();
    base::Value::Dict dict = config->ToDict();
    dict.Set("can_use_secure_dns_transactions", CanUseSecureDnsTransactions());
    dict.Set("can_use_insecure_dns_transactions",
             CanUseInsecureDnsTransactions());
    return dict;
  }

  std::optional<DnsConfig> GetSystemConfigForTesting() const override {
    return system_config_;
  }

  DnsConfigOverrides GetConfigOverridesForTesting() const override {
    return config_overrides_;
  }

  void SetTransactionFactoryForTesting(
      std::unique_ptr<DnsTransactionFactory> factory) override {
    factory_ = std::move(factory);
  }

  void SetAddressSorterForTesting(
      std::unique_ptr<AddressSorter> address_sorter) override {
    NOTIMPLEMENTED();
  }

 private:
  std::optional<DnsConfig> BuildEffectiveConfig() const {
    DnsConfig config;
    if (config_overrides_.OverridesEverything()) {
      config = config_overrides_.ApplyOverrides(DnsConfig());
    } else {
      if (!system_config_)
        return std::nullopt;

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
      return std::nullopt;

    return config;
  }

  bool UpdateDnsConfig() {
    std::optional<DnsConfig> new_effective_config = BuildEffectiveConfig();

    if (IsEqual(new_effective_config, GetEffectiveConfig()))
      return false;

    insecure_fallback_failures_ = 0;
    UpdateSession(std::move(new_effective_config));

    if (net_log_) {
      net_log_->AddGlobalEntry(NetLogEventType::DNS_CONFIG_CHANGED, [this] {
        return GetDnsConfigAsValueForNetLog();
      });
    }

    return true;
  }

  void UpdateSession(std::optional<DnsConfig> new_effective_config) {
    factory_.reset();
    session_ = nullptr;

    if (new_effective_config) {
      DCHECK(new_effective_config.value().IsValid());

      session_ = base::MakeRefCounted<DnsSession>(
          std::move(new_effective_config).value(), rand_int_callback_,
          net_log_);
      factory_ = DnsTransactionFactory::CreateFactory(session_.get());
    }
  }

  bool insecure_enabled_ = false;
  bool can_query_additional_types_via_insecure_ = false;
  int insecure_fallback_failures_ = 0;

  std::optional<DnsConfig> system_config_;
  DnsConfigOverrides config_overrides_;

  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_ =
      AddressSorter::CreateAddressSorter();

  raw_ptr<NetLog> net_log_;

  const RandIntCallback rand_int_callback_;
};

}  // namespace

// static
std::unique_ptr<DnsClient> DnsClient::CreateClient(NetLog* net_log) {
  return std::make_unique<DnsClientImpl>(net_log,
                                         base::BindRepeating(&base::RandInt));
}

// static
std::unique_ptr<DnsClient> DnsClient::CreateClientForTesting(
    NetLog* net_log,
    const RandIntCallback& rand_int_callback) {
  return std::make_unique<DnsClientImpl>(net_log, rand_int_callback);
}

}  // namespace net
