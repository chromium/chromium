// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_winhttp.h"

#include <windows.h>
#include <winhttp.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "url/gurl.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {
namespace {

static void FreeInfo(WINHTTP_PROXY_INFO* info) {
  if (info->lpszProxy)
    GlobalFree(info->lpszProxy);
  if (info->lpszProxyBypass)
    GlobalFree(info->lpszProxyBypass);
}

static Error WinHttpErrorToNetError(DWORD win_http_error) {
  switch (win_http_error) {
    case ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR:
    case ERROR_WINHTTP_INTERNAL_ERROR:
    case ERROR_WINHTTP_INCORRECT_HANDLE_TYPE:
      return ERR_FAILED;
    case ERROR_WINHTTP_LOGIN_FAILURE:
      return ERR_PROXY_AUTH_UNSUPPORTED;
    case ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT:
      return ERR_PAC_SCRIPT_FAILED;
    case ERROR_WINHTTP_INVALID_URL:
    case ERROR_WINHTTP_OPERATION_CANCELLED:
    case ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT:
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
      return ERR_HTTP_RESPONSE_CODE_FAILURE;
    case ERROR_NOT_ENOUGH_MEMORY:
      return ERR_INSUFFICIENT_RESOURCES;
    default:
      return ERR_FAILED;
  }
}

class ProxyResolverWinHttp : public ProxyResolver {
 public:
  ProxyResolverWinHttp(const scoped_refptr<PacFileData>& script_data);
  ~ProxyResolverWinHttp() override;

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     const NetworkIsolationKey& network_isolation_key,
                     ProxyInfo* results,
                     CompletionOnceCallback /*callback*/,
                     std::unique_ptr<Request>* /*request*/,
                     const NetLogWithSource& /*net_log*/) override;

 private:
  bool OpenWinHttpSession();
  void CloseWinHttpSession();

  // Proxy configuration is cached on the session handle.
  HINTERNET session_handle_;

  const GURL pac_url_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverWinHttp);
};

ProxyResolverWinHttp::ProxyResolverWinHttp(
    const scoped_refptr<PacFileData>& script_data)
    : session_handle_(nullptr),
      pac_url_(script_data->type() == PacFileData::TYPE_AUTO_DETECT
                   ? GURL("http://wpad/wpad.dat")
                   : script_data->url()) {}

ProxyResolverWinHttp::~ProxyResolverWinHttp() {
  CloseWinHttpSession();
}

int ProxyResolverWinHttp::GetProxyForURL(
    const GURL& query_url,
    const NetworkIsolationKey& network_isolation_key,
    ProxyInfo* results,
    CompletionOnceCallback /*callback*/,
    std::unique_ptr<Request>* /*request*/,
    const NetLogWithSource& /*net_log*/) {
  // If we don't have a WinHTTP session, then create a new one.
  if (!session_handle_ && !OpenWinHttpSession())
    return ERR_FAILED;

  // Windows' system resolver does not support WebSocket URLs in proxy.pac. This
  // was tested in version 10.0.16299, and is also implied by the description of
  // the ERROR_WINHTTP_UNRECOGNIZED_SCHEME error code in the Microsoft
  // documentation at
  // https://docs.microsoft.com/en-us/windows/desktop/api/winhttp/nf-winhttp-winhttpgetproxyforurl.
  // See https://crbug.com/862121.
  GURL mutable_query_url = query_url;
  if (query_url.SchemeIsWSOrWSS()) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(query_url.SchemeIsCryptographic() ? "https"
                                                                : "http");
    mutable_query_url = query_url.ReplaceComponents(replacements);
  }

  // If we have been given an empty PAC url, then use auto-detection.
  //
  // NOTE: We just use DNS-based auto-detection here like Firefox.  We do this
  // to avoid WinHTTP's auto-detection code, which while more featureful (it
  // supports DHCP based auto-detection) also appears to have issues.
  //
  WINHTTP_AUTOPROXY_OPTIONS options = {0};
  options.fAutoLogonIfChallenged = FALSE;
  options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
  base::string16 pac_url16 = base::ASCIIToUTF16(pac_url_.spec());
  options.lpszAutoConfigUrl = base::as_wcstr(pac_url16);

  WINHTTP_PROXY_INFO info = {0};
  DCHECK(session_handle_);

  // Per http://msdn.microsoft.com/en-us/library/aa383153(VS.85).aspx, it is
  // necessary to first try resolving with fAutoLogonIfChallenged set to false.
  // Otherwise, we fail over to trying it with a value of true.  This way we
  // get good performance in the case where WinHTTP uses an out-of-process
  // resolver.  This is important for Vista and Win2k3.
  BOOL ok = WinHttpGetProxyForUrl(
      session_handle_,
      base::as_wcstr(base::ASCIIToUTF16(mutable_query_url.spec())), &options,
      &info);
  if (!ok) {
    if (ERROR_WINHTTP_LOGIN_FAILURE == GetLastError()) {
      options.fAutoLogonIfChallenged = TRUE;
      ok = WinHttpGetProxyForUrl(
          session_handle_,
          base::as_wcstr(base::ASCIIToUTF16(mutable_query_url.spec())),
          &options, &info);
    }
    if (!ok) {
      DWORD error = GetLastError();
      // If we got here because of RPC timeout during out of process PAC
      // resolution, no further requests on this session are going to work.
      if (ERROR_WINHTTP_TIMEOUT == error ||
          ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR == error) {
        CloseWinHttpSession();
      }
      return WinHttpErrorToNetError(error);
    }
  }

  int rv = OK;

  switch (info.dwAccessType) {
    case WINHTTP_ACCESS_TYPE_NO_PROXY:
      results->UseDirect();
      break;
    case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
      // According to MSDN:
      //
      // The proxy server list contains one or more of the following strings
      // separated by semicolons or whitespace.
      //
      // ([<scheme>=][<scheme>"://"]<server>[":"<port>])
      //
      // Based on this description, ProxyInfo::UseNamedProxy() isn't
      // going to handle all the variations (in particular <scheme>=).
      //
      // However in practice, it seems that WinHTTP is simply returning
      // things like "foopy1:80;foopy2:80". It strips out the non-HTTP
      // proxy types, and stops the list when PAC encounters a "DIRECT".
      // So UseNamedProxy() should work OK.
      results->UseNamedProxy(base::WideToUTF8(info.lpszProxy));
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }

  FreeInfo(&info);
  return rv;
}

bool ProxyResolverWinHttp::OpenWinHttpSession() {
  DCHECK(!session_handle_);
  session_handle_ =
      WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session_handle_)
    return false;

  // Since this session handle will never be used for WinHTTP connections,
  // these timeouts don't really mean much individually.  However, WinHTTP's
  // out of process PAC resolution will use a combined (sum of all timeouts)
  // value to wait for an RPC reply.
  BOOL rv = WinHttpSetTimeouts(session_handle_, 10000, 10000, 5000, 5000);
  DCHECK(rv);

  return true;
}

void ProxyResolverWinHttp::CloseWinHttpSession() {
  if (session_handle_) {
    WinHttpCloseHandle(session_handle_);
    session_handle_ = nullptr;
  }
}

}  // namespace

ProxyResolverFactoryWinHttp::ProxyResolverFactoryWinHttp()
    : ProxyResolverFactory(false /*expects_pac_bytes*/) {
}

int ProxyResolverFactoryWinHttp::CreateProxyResolver(
    const scoped_refptr<PacFileData>& pac_script,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request) {
  resolver->reset(new ProxyResolverWinHttp(pac_script));
  return OK;
}

}  // namespace net
