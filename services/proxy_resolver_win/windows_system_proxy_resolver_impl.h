// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_
#define SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_

#include <windows.h>

#include <winhttp.h>

#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/sequence_checker.h"
#include "base/win/windows_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/proxy_resolution/win/winhttp_status.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"

class GURL;

namespace proxy_resolver_win {

class WinHttpAPIWrapper;

// This implements a mojo service to resolve proxies on Windows using only
// WinHttp APIs. It enables support for features like the Name Resolution Proxy
// Table (NRPT), DirectAccess, and per-interface proxy configurations. The
// WindowsSystemProxyResolverImpl attempts to create a HINTERNET session handle
// for all proxy resolutions, which should last for the lifetime of the process.
// Once created, incoming proxy resolution requests can be forwarded to WinHttp,
// which is guaranteed to return exactly once per request. The
// WindowsSystemProxyResolverImpl is also in charge of managing the lifetime of
// each of these requests, which will reply via callback once they have
// received a response from WinHttp.
class COMPONENT_EXPORT(PROXY_RESOLVER_WIN) WindowsSystemProxyResolverImpl
    : public mojom::WindowsSystemProxyResolver {
 public:
  explicit WindowsSystemProxyResolverImpl(
      mojo::PendingReceiver<mojom::WindowsSystemProxyResolver> receiver);

  WindowsSystemProxyResolverImpl(const WindowsSystemProxyResolverImpl&) =
      delete;
  WindowsSystemProxyResolverImpl& operator=(
      const WindowsSystemProxyResolverImpl&) = delete;

  ~WindowsSystemProxyResolverImpl() override;

  void SetCreateWinHttpAPIWrapperForTesting(
      std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper_for_testing);

  // mojom::WindowsSystemProxyResolver implementation
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override;

 private:
  friend class WindowsSystemProxyResolverImplTest;
  class Request;

  // Sets up the WinHttp session that will be used throughout the lifetime of
  // this object.
  net::WinHttpStatus EnsureInitialized();

  // This is the callback provided to WinHttp. Once a call to resolve a proxy
  // succeeds or errors out, it'll call into here with `context` being a pointer
  // to a Request that has been kept alive. This callback can hit in any thread
  // and will immediately post a task to the right sequence.
  static void CALLBACK WinHttpStatusCallback(HINTERNET resolver_handle,
                                             DWORD_PTR context,
                                             DWORD status,
                                             void* info,
                                             DWORD info_len);

  // Set to true once a WinHttp session has been successfully created and
  // configured.
  bool initialized_ = false;

  // This is a thin wrapper over WinHttp APIs.
  std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper_;

  // Tests may set their own wrapper that will get filled into
  // `winhttp_api_wrapper_` when needed instead of creating a new
  // WinHttpAPIWrapper.
  std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper_for_testing_;

  // This is the set of in-flight WinHttp proxy resolution calls. Each Request
  // will be kept alive for the duration of that call. After that, the Request
  // will attempt to respond to the callback and then get deleted.
  std::set<std::unique_ptr<Request>, base::UniquePtrComparator> requests_;

  mojo::Receiver<mojom::WindowsSystemProxyResolver> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace proxy_resolver_win

#endif  // SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_
