// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/proxy/proxy_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <WebKit/WebKit.h>

#import <optional>
#import <string>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

namespace {

// A key used to associate a `ProxyConfigurationProvider` with a `BrowserState`.
constexpr char kProxyConfigurationProviderKeyName[] =
    "proxy_configuration_provider";

// Maps `rules` into Apple's native `Network.framework` configuration
// objects on a background thread. Returns `std::nullopt` early if `cancel_flag`
// is set.
std::optional<NSArray<nw_proxy_config_t>*> MapProxyRulesToNative(
    std::vector<ProxyRule> rules,
    scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag)
    API_AVAILABLE(ios(17.0)) {
  CHECK(!web::WebThread::CurrentlyOn(web::WebThread::UI));

  NSMutableArray<nw_proxy_config_t>* native_configs = [NSMutableArray array];
  std::vector<std::string> accumulated_bypass_domains;

  for (const ProxyRule& rule : rules) {
    if (cancel_flag && cancel_flag->data.IsSet()) {
      return std::nullopt;
    }

    if (!rule.proxy_server.has_value()) {
      // Direct connection rule: collect `match_domains` as bypass domains.
      for (const std::string& domain : rule.match_domains) {
        accumulated_bypass_domains.push_back(domain);
      }
      continue;
    }

    if (!rule.proxy_server->is_http() && !rule.proxy_server->is_https()) {
      // TODO(crbug.com/476405339): Support additional proxy schemes once
      // resolved. Currently, only HTTP and HTTPS proxies (HTTP/1.1 CONNECT)
      // are supported by Apple's `Network.framework` HTTP CONNECT proxy API.
      LOG(WARNING) << "Unsupported proxy scheme: "
                   << static_cast<int>(rule.proxy_server->scheme())
                   << ". Only HTTP and HTTPS proxies are currently supported "
                      "(see crbug.com/476405339).";
      continue;
    }

    std::string port_str =
        base::NumberToString(rule.proxy_server->host_port_pair().port());
    nw_endpoint_t endpoint = nw_endpoint_create_host(
        rule.proxy_server->host_port_pair().host().c_str(), port_str.c_str());
    nw_protocol_options_t tls_options =
        rule.proxy_server->is_https() ? nw_tls_create_options() : nullptr;
    nw_proxy_config_t native_config =
        nw_proxy_config_create_http_connect(endpoint, tls_options);

    for (const std::string& domain : rule.match_domains) {
      nw_proxy_config_add_match_domain(native_config, domain.c_str());
    }

    // Attach accumulated bypass domains as exclusions to enforce precedence
    for (const std::string& excluded_domain : accumulated_bypass_domains) {
      nw_proxy_config_add_excluded_domain(native_config,
                                          excluded_domain.c_str());
    }

    [native_configs addObject:native_config];
  }

  return [native_configs copy];
}

}  // namespace

// static
ProxyConfigurationProvider& ProxyConfigurationProvider::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kProxyConfigurationProviderKeyName)) {
    browser_state->SetUserData(
        kProxyConfigurationProviderKeyName,
        base::WrapUnique(new ProxyConfigurationProvider(browser_state)));
  }
  return *(static_cast<ProxyConfigurationProvider*>(
      browser_state->GetUserData(kProxyConfigurationProviderKeyName)));
}

ProxyConfigurationProvider::ProxyConfigurationProvider(
    BrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

ProxyConfigurationProvider::~ProxyConfigurationProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProxyConfigurationProvider::UpdateProxyConfiguration(
    std::vector<ProxyRule> rules) API_AVAILABLE(ios(17.0)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancel_flag_) {
    cancel_flag_->data.Set();
  }
  cancel_flag_ = base::MakeRefCounted<base::RefCountedData<base::AtomicFlag>>();

  // Mapping proxy rules to native Apple proxy configurations is performed on a
  // background thread to avoid blocking the main UI sequence when processing
  // large sets of rules.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&MapProxyRulesToNative, std::move(rules), cancel_flag_),
      base::BindOnce(
          &ProxyConfigurationProvider::OnNativeProxyConfigurationsMapped,
          weak_ptr_factory_.GetWeakPtr(), cancel_flag_));
}

void ProxyConfigurationProvider::OnNativeProxyConfigurationsMapped(
    scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag,
    std::optional<NSArray<nw_proxy_config_t>*> native_configs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancel_flag->data.IsSet() || !native_configs.has_value()) {
    return;
  }

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  WKWebsiteDataStore* data_store = config_provider.GetWebsiteDataStore();
  if (data_store) {
    data_store.proxyConfigurations = *native_configs;
  }
}

}  // namespace web
