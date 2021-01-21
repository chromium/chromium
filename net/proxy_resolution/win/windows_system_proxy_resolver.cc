// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"

#include <cwchar>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/winhttp_api_wrapper.h"
#include "url/url_canon.h"

namespace net {

namespace {

bool GetProxyServerFromWinHttpResultEntry(
    const WINHTTP_PROXY_RESULT_ENTRY& result_entry,
    ProxyServer* out_proxy_server) {
  // TODO(https://crbug.com/1032820): Include net logs for proxy bypass
  if (!result_entry.fProxy) {
    *out_proxy_server = ProxyServer::Direct();
    return true;
  }

  ProxyServer::Scheme scheme = ProxyServer::Scheme::SCHEME_INVALID;
  switch (result_entry.ProxyScheme) {
    case (INTERNET_SCHEME_HTTP):
      scheme = ProxyServer::Scheme::SCHEME_HTTP;
      break;
    case (INTERNET_SCHEME_HTTPS):
      scheme = ProxyServer::Scheme::SCHEME_HTTPS;
      break;
    case (INTERNET_SCHEME_SOCKS):
      scheme = ProxyServer::Scheme::SCHEME_SOCKS4;
      break;
    default:
      LOG(WARNING) << "Of the possible proxy schemes returned by WinHttp, "
                      "Chrome supports HTTP(S) and SOCKS4. The ProxyScheme "
                      "that triggered this message is: "
                   << result_entry.ProxyScheme;
      break;
  }

  if (scheme == ProxyServer::Scheme::SCHEME_INVALID)
    return false;

  // Chrome expects a specific port from WinHttp. The WinHttp documentation on
  // MSDN makes it unclear whether or not a specific port is guaranteed.
  if (result_entry.ProxyPort == INTERNET_DEFAULT_PORT) {
    LOG(WARNING) << "WinHttpGetProxyForUrlEx() returned a proxy with "
                    "INTERNET_PORT_DEFAULT!";
    return false;
  }

  // Since there is a proxy in the result (i.e. |fProxy| is TRUE), the
  // |pwszProxy| is guaranteed to be non-null and non-empty.
  DCHECK(!!result_entry.pwszProxy);
  DCHECK(wcslen(result_entry.pwszProxy) > 0);

  std::wstring host_wide(result_entry.pwszProxy,
                         wcslen(result_entry.pwszProxy));
  if (!base::IsStringASCII(host_wide)) {
    const int kInitialBufferSize = 256;
    url::RawCanonOutputT<base::char16, kInitialBufferSize> punycode_output;
    if (!url::IDNToASCII(base::as_u16cstr(host_wide), host_wide.length(),
                         &punycode_output))
      return false;

    host_wide.assign(base::as_wcstr(punycode_output.data()),
                     punycode_output.length());
  }

  // At this point the string in |host_wide| is ASCII.
  std::string host;
  if (!base::WideToUTF8(host_wide.data(), host_wide.length(), &host))
    return false;

  HostPortPair host_and_port(host, result_entry.ProxyPort);
  *out_proxy_server = ProxyServer(scheme, host_and_port);
  return true;
}

}  // namespace

// static
scoped_refptr<WindowsSystemProxyResolver>
WindowsSystemProxyResolver::CreateWindowsSystemProxyResolver() {
  scoped_refptr<WindowsSystemProxyResolver> resolver = base::WrapRefCounted(
      new WindowsSystemProxyResolver(std::make_unique<WinHttpAPIWrapper>()));
  if (resolver->Initialize()) {
    return resolver;
  }
  return nullptr;
}

WindowsSystemProxyResolver::WindowsSystemProxyResolver(
    std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper)
    : winhttp_api_wrapper_(std::move(winhttp_api_wrapper)),
      sequenced_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
WindowsSystemProxyResolver::~WindowsSystemProxyResolver() = default;

bool WindowsSystemProxyResolver::Initialize() {
  if (!winhttp_api_wrapper_->CallWinHttpOpen())
    return false;

  // Since this session handle will never be used for WinHTTP connections,
  // these timeouts don't really mean much individually.  However, WinHTTP's
  // out of process PAC resolution will use a combined (sum of all timeouts)
  // value to wait for an RPC reply.
  if (!winhttp_api_wrapper_->CallWinHttpSetTimeouts(10000, 10000, 5000, 5000))
    return false;

  // This sets the entry point for every callback in the WinHttp session created
  // above.
  if (!winhttp_api_wrapper_->CallWinHttpSetStatusCallback(
          &WindowsSystemProxyResolver::WinHttpStatusCallback))
    return false;

  return true;
}

bool WindowsSystemProxyResolver::GetProxyForUrl(
    WindowsSystemProxyResolutionRequest* callback_target,
    const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Fetch the current system proxy settings. These are per current network
  // interface and per current user.
  ScopedIEConfig scoped_ie_config;
  if (!winhttp_api_wrapper_->CallWinHttpGetIEProxyConfigForCurrentUser(
          scoped_ie_config.config()))
    return false;

  // This will create a handle specifically for WinHttpGetProxyForUrlEx().
  HINTERNET resolver_handle = nullptr;
  if (!winhttp_api_wrapper_->CallWinHttpCreateProxyResolver(&resolver_handle))
    return false;

  // WinHttp will do all necessary proxy resolution fallback for this request.
  // If automatic settings aren't configured or fail, it'll use any manually
  // configured proxies on the machine. The WINHTTP_AUTOPROXY_ALLOW_STATIC flag
  // tells the APIs to pick up manually configured proxies.
  //
  // Separately, Windows allows different proxy settings for different network
  // interfaces. The WINHTTP_AUTOPROXY_OPTIONS flag tells WinHttp to
  // differentiate between these settings and to get the proxy that's most
  // specific to the current interface.
  WINHTTP_AUTOPROXY_OPTIONS autoproxy_options = {0};
  autoproxy_options.dwFlags =
      WINHTTP_AUTOPROXY_ALLOW_STATIC | WINHTTP_AUTOPROXY_ALLOW_CM;

  // The fAutoLogonIfChallenged option has been deprecated and should always be
  // set to FALSE throughout Windows 10. Even in earlier versions of the OS,
  // this feature did not work particularly well.
  // https://support.microsoft.com/en-us/help/3161949/ms16-077-description-of-the-security-update-for-wpad-june-14-2016
  autoproxy_options.fAutoLogonIfChallenged = FALSE;

  // Sets a specific PAC URL if there was one in the IE configs.
  if (scoped_ie_config.config()->lpszAutoConfigUrl) {
    autoproxy_options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
    autoproxy_options.lpszAutoConfigUrl =
        scoped_ie_config.config()->lpszAutoConfigUrl;
  }

  // Similarly, allow WPAD if it was enabled in the IE configs.
  if (!!scoped_ie_config.config()->fAutoDetect) {
    autoproxy_options.dwFlags |= WINHTTP_AUTOPROXY_AUTO_DETECT;

    // Enable WPAD using both DNS and DHCP, since that is what idiomatic Windows
    // applications do.
    autoproxy_options.dwAutoDetectFlags |= WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    autoproxy_options.dwAutoDetectFlags |= WINHTTP_AUTO_DETECT_TYPE_DHCP;
  }

  // Now that everything is set-up, ask WinHTTP to get the actual proxy list.
  const DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
  if (!winhttp_api_wrapper_->CallWinHttpGetProxyForUrlEx(
          resolver_handle, url, &autoproxy_options, context)) {
    winhttp_api_wrapper_->CallWinHttpCloseHandle(resolver_handle);
    return false;
  }

  // Saves of the object which will receive the callback once the operation
  // completes.
  AddPendingCallbackTarget(callback_target, resolver_handle);

  // On a successful call to WinHttpGetProxyForUrlEx(), the callback set by
  // CallWinHttpSetStatusCallback() is guaranteed to be called exactly once.
  // That may happen at any time on any thread. In order to make sure this
  // object does not destruct before that callback occurs, it must AddRef()
  // itself. This reference will be Release()'d in the callback.
  base::RefCountedThreadSafe<WindowsSystemProxyResolver>::AddRef();

  return true;
}

void WindowsSystemProxyResolver::AddPendingCallbackTarget(
    WindowsSystemProxyResolutionRequest* callback_target,
    HINTERNET resolver_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_callback_target_map_[callback_target] = resolver_handle;
}

void WindowsSystemProxyResolver::RemovePendingCallbackTarget(
    WindowsSystemProxyResolutionRequest* callback_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_callback_target_map_.erase(callback_target);
}

bool WindowsSystemProxyResolver::HasPendingCallbackTarget(
    WindowsSystemProxyResolutionRequest* callback_target) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (pending_callback_target_map_.find(callback_target) !=
          pending_callback_target_map_.end());
}

WindowsSystemProxyResolutionRequest*
WindowsSystemProxyResolver::LookupCallbackTargetFromResolverHandle(
    HINTERNET resolver_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WindowsSystemProxyResolutionRequest* pending_callback_target = nullptr;
  for (auto target : pending_callback_target_map_) {
    if (target.second == resolver_handle) {
      pending_callback_target = target.first;
      break;
    }
  }
  return pending_callback_target;
}

// static
void __stdcall WindowsSystemProxyResolver::WinHttpStatusCallback(
    HINTERNET resolver_handle,
    DWORD_PTR context,
    DWORD status,
    void* info,
    DWORD info_len) {
  DCHECK(resolver_handle);
  DCHECK(context);
  WindowsSystemProxyResolver* windows_system_proxy_resolver =
      reinterpret_cast<WindowsSystemProxyResolver*>(context);

  // Make a copy of any error information in |info| so it can be accessed from
  // the subsequently posted task. The |info| pointer's lifetime is managed by
  // WinHTTP and hence is not valid once this frame returns.
  int windows_error = S_OK;
  if (info && status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
    WINHTTP_ASYNC_RESULT* result = static_cast<WINHTTP_ASYNC_RESULT*>(info);
    windows_error = result->dwError;
  }

  // It is possible for PostTask() to fail (ex: during shutdown). In that case,
  // the WindowsSystemProxyResolver in |context| will leak. This is expected to
  // be either unusual or to occur during shutdown, where a leak doesn't matter.
  // Since calling the |context| on the wrong thread may be problematic, it will
  // be allowed to leak here if PostTask() fails.
  windows_system_proxy_resolver->sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSystemProxyResolver::DoWinHttpStatusCallback,
                     windows_system_proxy_resolver, resolver_handle, status,
                     windows_error));
}

void WindowsSystemProxyResolver::DoWinHttpStatusCallback(
    HINTERNET resolver_handle,
    DWORD status,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The |resolver_handle| should correspond to a HINTERNET resolver_handle in
  // |pending_callback_target_map_| unless the associated attempt to get a proxy
  // for an URL has been cancelled.
  WindowsSystemProxyResolutionRequest* pending_callback_target =
      LookupCallbackTargetFromResolverHandle(resolver_handle);

  // There is no work to do if this request has been cancelled.
  if (pending_callback_target) {
    switch (status) {
      case WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE:
        GetProxyResultForCallbackTarget(pending_callback_target,
                                        resolver_handle);
        break;
      case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        HandleErrorForCallbackTarget(pending_callback_target, windows_error);
        break;
      default:
        LOG(WARNING) << "DoWinHttpStatusCallback() expects only callbacks for "
                        "WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE and "
                        "WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, not: "
                     << status;
        HandleErrorForCallbackTarget(pending_callback_target, E_UNEXPECTED);
        break;
    }

    // No matter what happened above, the |pending_callback_target| should no
    // longer be in the |pending_callback_target_map_|. Either the callback was
    // handled or it was cancelled. This pointer will be explicitly cleared to
    // make it obvious that it can no longer be used safely.
    DCHECK(!HasPendingCallbackTarget(pending_callback_target));
    pending_callback_target = nullptr;
  }

  // The HINTERNET |resolver_handle| for this attempt at getting a proxy is no
  // longer needed.
  winhttp_api_wrapper_->CallWinHttpCloseHandle(resolver_handle);

  // The current WindowsSystemProxyResolver object may now be Release()'d on the
  // correct sequence after all work is done, thus balancing out the AddRef()
  // from WinHttpGetProxyForUrlEx().
  base::RefCountedThreadSafe<WindowsSystemProxyResolver>::Release();
}

void WindowsSystemProxyResolver::GetProxyResultForCallbackTarget(
    WindowsSystemProxyResolutionRequest* callback_target,
    HINTERNET resolver_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasPendingCallbackTarget(callback_target));

  WINHTTP_PROXY_RESULT proxy_result = {0};
  if (!winhttp_api_wrapper_->CallWinHttpGetProxyResult(resolver_handle,
                                                       &proxy_result)) {
    // TODO(https://crbug.com/1032820): Use a more detailed net error.
    callback_target->AsynchronousProxyResolutionComplete(ProxyList(),
                                                         ERR_FAILED, 0);
    return;
  }

  // Translate the results for ProxyInfo.
  ProxyList proxy_list;
  for (DWORD i = 0u; i < proxy_result.cEntries; ++i) {
    ProxyServer proxy_server;
    if (GetProxyServerFromWinHttpResultEntry(proxy_result.pEntries[i],
                                             &proxy_server))
      proxy_list.AddProxyServer(proxy_server);
  }

  // The |proxy_result| must be freed.
  winhttp_api_wrapper_->CallWinHttpFreeProxyResult(&proxy_result);

  // The consumer of this proxy resolution may not understand an empty proxy
  // list. Thus, this case is considered an error.
  int net_error = proxy_list.IsEmpty() ? ERR_FAILED : OK;
  callback_target->AsynchronousProxyResolutionComplete(proxy_list, net_error,
                                                       0);
}

void WindowsSystemProxyResolver::HandleErrorForCallbackTarget(
    WindowsSystemProxyResolutionRequest* callback_target,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasPendingCallbackTarget(callback_target));

  // TODO(https://crbug.com/1032820): Use a more detailed net error.
  callback_target->AsynchronousProxyResolutionComplete(ProxyList(), ERR_FAILED,
                                                       windows_error);
}

}  // namespace net
