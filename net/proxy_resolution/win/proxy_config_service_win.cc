// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/proxy_config_service_win.h"

#include <windows.h>

#include <winhttp.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

const int kPollIntervalSec = 10;

void FreeIEConfig(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_config) {
  if (ie_config->lpszAutoConfigUrl) {
    GlobalFree(ie_config->lpszAutoConfigUrl);
  }
  if (ie_config->lpszProxy) {
    GlobalFree(ie_config->lpszProxy);
  }
  if (ie_config->lpszProxyBypass) {
    GlobalFree(ie_config->lpszProxyBypass);
  }
}

}  // namespace

ProxyConfigServiceWin::ProxyConfigServiceWin(
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : PollingProxyConfigService(
          base::Seconds(kPollIntervalSec),
          base::BindRepeating(&ProxyConfigServiceWin::GetCurrentProxyConfig),
          traffic_annotation) {
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

ProxyConfigServiceWin::~ProxyConfigServiceWin() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  // The registry functions below will end up going to disk.  TODO: Do this on
  // another thread to avoid slowing the current thread.  http://crbug.com/61453
  base::ScopedAllowBlocking scoped_allow_blocking;
  keys_to_watch_.clear();
}

void ProxyConfigServiceWin::AddObserver(Observer* observer) {
  // Lazily-initialize our registry watcher.
  StartWatchingRegistryForChanges();

  // Let the super-class do its work now.
  PollingProxyConfigService::AddObserver(observer);
}

void ProxyConfigServiceWin::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  // Proxy settings on Windows may change when the active connection changes.
  // For instance, after connecting to a VPN, the proxy settings for the active
  // connection will be that for the VPN. (And ProxyConfigService only reports
  // proxy settings for the default connection).

  // This is conditioned on CONNECTION_NONE to avoid duplicating work, as
  // NetworkChangeNotifier additionally sends it preceding completion.
  // See https://crbug.com/1071901.
  if (type == NetworkChangeNotifier::CONNECTION_NONE) {
    CheckForChangesNow();
  }
}

void ProxyConfigServiceWin::StartWatchingRegistryForChanges() {
  if (!keys_to_watch_.empty()) {
    return;  // Already initialized.
  }

  // The registry functions below will end up going to disk.  Do this on another
  // thread to avoid slowing the current thread.  http://crbug.com/61453
  base::ScopedAllowBlocking scoped_allow_blocking;

  // There are a number of different places where proxy settings can live
  // in the registry. In some cases it appears in a binary value, in other
  // cases string values. Furthermore winhttp and wininet appear to have
  // separate stores, and proxy settings can be configured per-machine
  // or per-user.
  //
  // This function is probably not exhaustive in the registry locations it
  // watches for changes, however it should catch the majority of the
  // cases. In case we have missed some less common triggers (likely), we
  // will catch them during the periodic (10 second) polling, so things
  // will recover.

  AddKeyToWatchList(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");

  AddKeyToWatchList(
      HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");

  AddKeyToWatchList(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\"
                    L"Internet Settings");
}

bool ProxyConfigServiceWin::AddKeyToWatchList(HKEY rootkey,
                                              const wchar_t* subkey) {
  std::unique_ptr<base::win::RegKey> key =
      std::make_unique<base::win::RegKey>();
  if (key->Create(rootkey, subkey, KEY_NOTIFY) != ERROR_SUCCESS) {
    return false;
  }

  if (!key->StartWatching(base::BindOnce(
          &ProxyConfigServiceWin::OnObjectSignaled, base::Unretained(this),
          base::Unretained(key.get())))) {
    return false;
  }

  keys_to_watch_.push_back(std::move(key));
  return true;
}

void ProxyConfigServiceWin::OnObjectSignaled(base::win::RegKey* key) {
  // Figure out which registry key signalled this change.
  auto it = base::ranges::find(keys_to_watch_, key,
                               &std::unique_ptr<base::win::RegKey>::get);
  CHECK(it != keys_to_watch_.end(), base::NotFatalUntil::M130);

  // Keep watching the registry key.
  if (!key->StartWatching(
          base::BindOnce(&ProxyConfigServiceWin::OnObjectSignaled,
                         base::Unretained(this), base::Unretained(key)))) {
    keys_to_watch_.erase(it);
  }

  // Have the PollingProxyConfigService test for changes.
  CheckForChangesNow();
}

// static
void ProxyConfigServiceWin::GetCurrentProxyConfig(
    const NetworkTrafficAnnotationTag traffic_annotation,
    ProxyConfigWithAnnotation* config) {
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
  if (!WinHttpGetIEProxyConfigForCurrentUser(&ie_config)) {
    LOG(ERROR) << "WinHttpGetIEProxyConfigForCurrentUser failed: "
               << GetLastError();
    *config = ProxyConfigWithAnnotation::CreateDirect();
    return;
  }
  ProxyConfig proxy_config;
  SetFromIEConfig(&proxy_config, ie_config);
  FreeIEConfig(&ie_config);
  proxy_config.set_from_system(true);
  *config = ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
}

// static
void ProxyConfigServiceWin::SetFromIEConfig(
    ProxyConfig* config,
    const WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& ie_config) {
  if (ie_config.fAutoDetect) {
    config->set_auto_detect(true);
  }
  if (ie_config.lpszProxy) {
    // lpszProxy may be a single proxy, or a proxy per scheme. The format
    // is compatible with ProxyConfig::ProxyRules's string format.
    config->proxy_rules().ParseFromString(
        base::WideToUTF8(ie_config.lpszProxy));
  }
  if (ie_config.lpszProxyBypass) {
    std::string proxy_bypass = base::WideToUTF8(ie_config.lpszProxyBypass);

    base::StringTokenizer proxy_server_bypass_list(proxy_bypass, ";, \t\n\r");
    while (proxy_server_bypass_list.GetNext()) {
      std::string bypass_url_domain = proxy_server_bypass_list.token();
      config->proxy_rules().bypass_rules.AddRuleFromString(bypass_url_domain);
    }
  }
  if (ie_config.lpszAutoConfigUrl) {
    config->set_pac_url(GURL(base::as_u16cstr(ie_config.lpszAutoConfigUrl)));
  }
}

}  // namespace net
