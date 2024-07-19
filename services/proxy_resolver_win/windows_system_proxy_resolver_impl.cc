// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/proxy_resolver_win/windows_system_proxy_resolver_impl.h"

#include <cwchar>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver_win/winhttp_api_wrapper_impl.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace proxy_resolver_win {

namespace {

bool GetProxyChainFromWinHttpResultEntry(
    const WINHTTP_PROXY_RESULT_ENTRY& result_entry,
    net::ProxyChain* out_proxy_chain) {
  // TODO(crbug.com/40111093): Include net logs for proxy bypass
  if (!result_entry.fProxy) {
    *out_proxy_chain = net::ProxyChain::Direct();
    return true;
  }

  net::ProxyServer::Scheme scheme = net::ProxyServer::Scheme::SCHEME_INVALID;
  switch (result_entry.ProxyScheme) {
    case (INTERNET_SCHEME_HTTP):
      scheme = net::ProxyServer::Scheme::SCHEME_HTTP;
      break;
    case (INTERNET_SCHEME_HTTPS):
      scheme = net::ProxyServer::Scheme::SCHEME_HTTPS;
      break;
    case (INTERNET_SCHEME_SOCKS):
      scheme = net::ProxyServer::Scheme::SCHEME_SOCKS4;
      break;
    default:
      LOG(WARNING) << "Of the possible proxy schemes returned by WinHttp, "
                      "Chrome supports HTTP(S) and SOCKS4. The ProxyScheme "
                      "that triggered this message is: "
                   << result_entry.ProxyScheme;
      break;
  }

  if (scheme == net::ProxyServer::Scheme::SCHEME_INVALID)
    return false;

  // Chrome expects a specific port from WinHttp. The WinHttp documentation on
  // MSDN makes it unclear whether or not a specific port is guaranteed.
  if (result_entry.ProxyPort == INTERNET_DEFAULT_PORT) {
    LOG(WARNING) << "WinHttpGetProxyForUrlEx() returned a proxy with "
                    "INTERNET_PORT_DEFAULT!";
    return false;
  }

  // Since there is a proxy in the result (i.e. `fProxy` is TRUE), the
  // `pwszProxy` is guaranteed to be non-null and non-empty.
  DCHECK(!!result_entry.pwszProxy);
  DCHECK(wcslen(result_entry.pwszProxy) > 0);

  std::wstring host_wide(result_entry.pwszProxy,
                         wcslen(result_entry.pwszProxy));
  if (!base::IsStringASCII(host_wide)) {
    const int kInitialBufferSize = 256;
    url::RawCanonOutputT<char16_t, kInitialBufferSize> punycode_output;
    if (!url::IDNToASCII(base::AsStringPiece16(host_wide), &punycode_output)) {
      return false;
    }

    host_wide = base::AsWString(punycode_output.view());
  }

  // At this point the string in `host_wide` is ASCII.
  std::string host;
  if (!base::WideToUTF8(host_wide.data(), host_wide.length(), &host))
    return false;

  net::HostPortPair host_and_port(host, result_entry.ProxyPort);
  *out_proxy_chain = net::ProxyChain(scheme, host_and_port);
  return true;
}

}  // namespace

// Encapsulates an in-flight WinHttp proxy resolution request. This also owns
// the GetProxyForUrlCallback and will attempt to supply that callback with the
// proxy result once WinHttp returns.
class WindowsSystemProxyResolverImpl::Request {
 public:
  Request(WindowsSystemProxyResolverImpl* parent,
          GetProxyForUrlCallback callback);
  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;
  ~Request();

  // Sets up and kicks off proxy resolution using WinHttp.
  bool Start(const GURL& url);

  base::SequencedTaskRunner* sequenced_task_runner() {
    return sequenced_task_runner_.get();
  }

  // Called from WinHttpStatusCallback on the right sequence. This will make
  // decisions about what to do from the results of the proxy resolution call.
  void DoWinHttpStatusCallback(HINTERNET resolver_handle,
                               DWORD status,
                               int windows_error);

 private:
  // On a successful call to WinHttpGetProxyForUrlEx(), this translates WinHttp
  // results into Chromium-friendly structures and reports the result.
  void GetProxyResultForCallback();

  // Notifies `callback_` of the proxy result.
  void ReportResult(const net::ProxyList& proxy_list,
                    net::WinHttpStatus winhttp_status,
                    int windows_error);

  WinHttpAPIWrapper* winhttp_api_wrapper() {
    return parent_->winhttp_api_wrapper_.get();
  }

  // The WindowsSystemProxyResolverImpl manages the lifetime of this object. The
  // Request cannot outlive the WindowsSystemProxyResolverImpl. Thus, it is safe
  // to hold on to a raw pointer.
  const raw_ptr<WindowsSystemProxyResolverImpl> parent_;
  GetProxyForUrlCallback callback_;
  HINTERNET resolver_handle_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

WindowsSystemProxyResolverImpl::Request::Request(
    WindowsSystemProxyResolverImpl* parent,
    GetProxyForUrlCallback callback)
    : parent_(parent),
      callback_(std::move(callback)),
      resolver_handle_(nullptr),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

WindowsSystemProxyResolverImpl::Request::~Request() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the `resolver_handle_` is not null, then there may be an in-flight
  // WinHttp call. This could conceivably happen in some shutdown scenarios. In
  // this case, close the handle to cancel the operation.
  if (resolver_handle_) {
    winhttp_api_wrapper()->CallWinHttpCloseHandle(resolver_handle_);
    resolver_handle_ = nullptr;
  }
}

bool WindowsSystemProxyResolverImpl::Request::Start(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40111093): Use better/distinct net errors.
  net::WinHttpStatus winhttp_status = parent_->EnsureInitialized();
  if (winhttp_status != net::WinHttpStatus::kOk) {
    const int error = GetLastError();
    ReportResult(net::ProxyList(), winhttp_status, error);
    return false;
  }

  // Fetch the current system proxy settings. These are per current network
  // interface and per current user.
  ScopedIEConfig scoped_ie_config;
  if (!winhttp_api_wrapper()->CallWinHttpGetIEProxyConfigForCurrentUser(
          scoped_ie_config.config())) {
    const int error = GetLastError();
    ReportResult(
        net::ProxyList(),
        net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed,
        error);
    return false;
  }

  // This will create a handle specifically for WinHttpGetProxyForUrlEx().
  if (!winhttp_api_wrapper()->CallWinHttpCreateProxyResolver(
          &resolver_handle_)) {
    const int error = GetLastError();
    ReportResult(net::ProxyList(),
                 net::WinHttpStatus::kWinHttpCreateProxyResolverFailed, error);
    return false;
  }

  // WinHttp will do all necessary proxy resolution fallback for this request.
  // If automatic settings aren't configured or fail, it'll use any manually
  // configured proxies on the machine. The WINHTTP_AUTOPROXY_ALLOW_STATIC flag
  // tells the APIs to pick up manually configured proxies.
  //
  // Separately, Windows allows different proxy settings for different network
  // interfaces. The WINHTTP_AUTOPROXY_ALLOW_CM flag tells WinHttp to
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
  if (!winhttp_api_wrapper()->CallWinHttpGetProxyForUrlEx(
          resolver_handle_, url.spec(), &autoproxy_options, context)) {
    const int error = GetLastError();
    winhttp_api_wrapper()->CallWinHttpCloseHandle(resolver_handle_);
    resolver_handle_ = nullptr;
    ReportResult(net::ProxyList(),
                 net::WinHttpStatus::kWinHttpGetProxyForURLExFailed, error);
    return false;
  }

  return true;
}

void WindowsSystemProxyResolverImpl::Request::DoWinHttpStatusCallback(
    HINTERNET resolver_handle,
    DWORD status,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The `resolver_handle` should correspond to `resolver_handle_`.
  DCHECK_EQ(resolver_handle_, resolver_handle);

  // There is no work to do if this request has been cancelled.
  if (callback_) {
    switch (status) {
      case WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE:
        GetProxyResultForCallback();
        break;
      case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        // TODO(crbug.com/40111093): Use a better/distinct net error.
        ReportResult(net::ProxyList(),
                     net::WinHttpStatus::kStatusCallbackFailed, windows_error);
        break;
      default:
        LOG(WARNING) << "DoWinHttpStatusCallback() expects only callbacks for "
                        "WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE and "
                        "WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, not: "
                     << status;
        ReportResult(net::ProxyList(),
                     net::WinHttpStatus::kStatusCallbackFailed, status);
        break;
    }
  }

  // The HINTERNET `resolver_handle_` for this attempt at getting a proxy is no
  // longer needed.
  winhttp_api_wrapper()->CallWinHttpCloseHandle(resolver_handle_);
  resolver_handle_ = nullptr;

  // At this point, the mojo callback has definitely been called.
  DCHECK(callback_.is_null());

  // Now, it's finally safe to delete this object.
  auto it = parent_->requests_.find(this);
  CHECK(it != parent_->requests_.end(), base::NotFatalUntil::M130);
  parent_->requests_.erase(it);

  // DO NOT ADD ANYTHING BELOW THIS LINE, THE OBJECT HAS NOW BEEN DESTROYED.
}

void WindowsSystemProxyResolverImpl::Request::GetProxyResultForCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40111093): Use better/distinct net errors.
  WINHTTP_PROXY_RESULT proxy_result = {0};
  if (!winhttp_api_wrapper()->CallWinHttpGetProxyResult(resolver_handle_,
                                                        &proxy_result)) {
    const int error = GetLastError();
    ReportResult(net::ProxyList(),
                 net::WinHttpStatus::kWinHttpGetProxyResultFailed, error);
    return;
  }

  // Translate the results for ProxyInfo.
  net::ProxyList proxy_list;
  for (DWORD i = 0u; i < proxy_result.cEntries; ++i) {
    net::ProxyChain proxy_chain;
    if (GetProxyChainFromWinHttpResultEntry(proxy_result.pEntries[i],
                                            &proxy_chain)) {
      proxy_list.AddProxyChain(proxy_chain);
    }
  }

  // The `proxy_result` must be freed.
  winhttp_api_wrapper()->CallWinHttpFreeProxyResult(&proxy_result);

  // The consumer of this proxy resolution may not understand an empty proxy
  // list. Thus, this case is considered an error.
  net::WinHttpStatus winhttp_status = proxy_list.IsEmpty()
                                          ? net::WinHttpStatus::kEmptyProxyList
                                          : net::WinHttpStatus::kOk;
  ReportResult(proxy_list, winhttp_status, 0);
}

void WindowsSystemProxyResolverImpl::Request::ReportResult(
    const net::ProxyList& proxy_list,
    net::WinHttpStatus winhttp_status,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_)
    return;

  std::move(callback_).Run(proxy_list, winhttp_status, windows_error);
  callback_.Reset();

  // Even though there are no more mojo calls to make, it is not safe to delete
  // this object yet. To be absolutely sure that WinHttpStatusCallback() does
  // not attempt to dereference a freed object, the only times we delete this
  // object are when we fail to call into WinHttp APIs completely (i.e. we do
  // not expect a callback) and when the WinHttp callback completes in
  // DoWinHttpStatusCallback().
}

WindowsSystemProxyResolverImpl::WindowsSystemProxyResolverImpl(
    mojo::PendingReceiver<mojom::WindowsSystemProxyResolver> receiver)
    : receiver_(this, std::move(receiver)) {}
WindowsSystemProxyResolverImpl::~WindowsSystemProxyResolverImpl() {
  // The WindowsSystemProxyResolverImpl must outlive every Request it owns.
  CHECK(requests_.empty());
}

void WindowsSystemProxyResolverImpl::SetCreateWinHttpAPIWrapperForTesting(
    std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper_for_testing) {
  winhttp_api_wrapper_ = std::move(winhttp_api_wrapper_for_testing);
}

void WindowsSystemProxyResolverImpl::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<Request> request =
      std::make_unique<Request>(this, std::move(callback));

  // If the request fails to start, it will internally report that to
  // `callback`. After that, it's safe to delete this `request`.
  if (request->Start(url))
    requests_.insert(std::move(request));
}

net::WinHttpStatus WindowsSystemProxyResolverImpl::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_)
    return net::WinHttpStatus::kOk;

  // TODO(crbug.com/40111093): Limit the number of times this can
  // fail to initialize.

  // The `winhttp_api_wrapper_` is intended to only get set when initialization
  // is successful. However, it may have been pre-populated via
  // SetCreateWinHttpAPIWrapperForTesting(). In those cases, use that object
  // instead of creating a new one.
  std::unique_ptr<WinHttpAPIWrapper> uninitialized_winhttp_api_wrapper;
  if (winhttp_api_wrapper_) {
    uninitialized_winhttp_api_wrapper = std::move(winhttp_api_wrapper_);
    winhttp_api_wrapper_.reset();
  } else {
    uninitialized_winhttp_api_wrapper =
        std::make_unique<WinHttpAPIWrapperImpl>();
  }

  if (!uninitialized_winhttp_api_wrapper->CallWinHttpOpen())
    return net::WinHttpStatus::kWinHttpOpenFailed;

  // Since this session handle will never be used for WinHTTP connections,
  // these timeouts don't really mean much individually.  However, WinHTTP's
  // out of process PAC resolution will use a combined (sum of all timeouts)
  // value to wait for an RPC reply.
  if (!uninitialized_winhttp_api_wrapper->CallWinHttpSetTimeouts(10000, 10000,
                                                                 5000, 5000)) {
    return net::WinHttpStatus::kWinHttpSetTimeoutsFailed;
  }

  // This sets the entry point for every callback in the WinHttp session created
  // above.
  if (!uninitialized_winhttp_api_wrapper->CallWinHttpSetStatusCallback(
          &WindowsSystemProxyResolverImpl::WinHttpStatusCallback)) {
    return net::WinHttpStatus::kWinHttpSetStatusCallbackFailed;
  }

  initialized_ = true;
  winhttp_api_wrapper_ = std::move(uninitialized_winhttp_api_wrapper);
  return net::WinHttpStatus::kOk;
}

// static
void CALLBACK
WindowsSystemProxyResolverImpl::WinHttpStatusCallback(HINTERNET resolver_handle,
                                                      DWORD_PTR context,
                                                      DWORD status,
                                                      void* info,
                                                      DWORD info_len) {
  DCHECK(resolver_handle);
  DCHECK(context);
  Request* request = reinterpret_cast<Request*>(context);

  // Make a copy of any error information in `info` so it can be accessed from
  // the subsequently posted task. The `info` pointer's lifetime is managed by
  // WinHTTP and hence is not valid once this frame returns.
  int windows_error = S_OK;
  if (info && status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
    WINHTTP_ASYNC_RESULT* result = static_cast<WINHTTP_ASYNC_RESULT*>(info);
    windows_error = result->dwError;
  }

  request->sequenced_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Request::DoWinHttpStatusCallback,
                                base::Unretained(request), resolver_handle,
                                status, windows_error));
}

}  // namespace proxy_resolver_win
