// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_
#define SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_

#include <windows.h>
#include <winhttp.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/win/windows_types.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_list.h"

namespace proxy_resolver_win {

class WindowsSystemProxyResolutionRequest;
class WinHttpAPIWrapper;

// This provides a layer of abstraction between calling code and
// Windows-specific code. It is shared between the
// WindowsSystemProxyResolutionService and inflight WinHttp callbacks.
// Internally, it takes care of all interaction with WinHttp. The only time this
// object is ever accessed outside of its sequence is during the WinHttp
// callback. For the sake of that callback, this must be RefCountedThreadSafe.
class NET_EXPORT WindowsSystemProxyResolverImpl
    : public base::RefCountedThreadSafe<WindowsSystemProxyResolverImpl> {
 public:
  static scoped_refptr<WindowsSystemProxyResolverImpl>
  CreateWindowsSystemProxyResolver();

  WindowsSystemProxyResolverImpl(const WindowsSystemProxyResolverImpl&) =
      delete;
  WindowsSystemProxyResolverImpl& operator=(
      const WindowsSystemProxyResolverImpl&) = delete;

  // This will first fetch the current system proxy settings by calling into
  // WinHttpGetIEProxyConfigForCurrentUser() and then resolve the proxy using
  // those settings as an input into WinHttpGetProxyForUrlEx().
  bool GetProxyForUrl(WindowsSystemProxyResolutionRequest* callback_target,
                      const std::string& url) WARN_UNUSED_RESULT;

  // After calling GetProxyForUrl(), a |callback_target| is saved internally for
  // when proxy resolution is complete. When a
  // WindowsSystemProxyResolutionRequest wants to avoid receiving a callback,
  // it must remove itself from the list of pending callback targets.
  void RemovePendingCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target);
  bool HasPendingCallbackTarget(WindowsSystemProxyResolutionRequest*
                                    callback_target) const WARN_UNUSED_RESULT;

 private:
  friend class base::RefCountedThreadSafe<WindowsSystemProxyResolverImpl>;
  friend class WindowsSystemProxyResolverTest;

  explicit WindowsSystemProxyResolverImpl(
      std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper);
  ~WindowsSystemProxyResolverImpl();

  // Sets up the WinHttp session that will be used throughout the lifetime of
  // this object.
  bool Initialize();

  // These will interact with |pending_callback_target_map_|.
  void AddPendingCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target,
      HINTERNET handle);
  WindowsSystemProxyResolutionRequest* LookupCallbackTargetFromResolverHandle(
      HINTERNET resolver_handle) const;

  // This is the callback provided to WinHttp. Once a call to resolve a proxy
  // succeeds or errors out, it'll call into here with |context| being a pointer
  // to a WindowsSystemProxyResolverImpl that has been kept alive. This callback
  // can hit in any thread and will immediately post a task to the right
  // sequence.
  static void CALLBACK WinHttpStatusCallback(HINTERNET resolver_handle,
                                             DWORD_PTR context,
                                             DWORD status,
                                             void* info,
                                             DWORD info_len);

  // Called from WinHttpStatusCallback on the right sequence. This will make
  // decisions about what to do from the results of the proxy resolution call.
  // Note that the WindowsSystemProxyResolutionRequest that asked for this proxy
  // may have decided they no longer need an answer (ex: the request has gone
  // away), so this function has to deal with that situation too.
  void DoWinHttpStatusCallback(HINTERNET resolver_handle,
                               DWORD status,
                               int windows_error);

  // On a successful call to WinHttpGetProxyForUrlEx(), this translates WinHttp
  // results into Chromium-friendly structures before notifying the right
  // WindowsSystemProxyResolutionRequest.
  void GetProxyResultForCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target,
      HINTERNET resolver_handle);

  // On a failed call to WinHttpGetProxyForUrlEx(), this will notify the right
  // WindowsSystemProxyResolutionRequest of the error.
  void HandleErrorForCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target,
      int windows_error);

  // This is a thin wrapper over WinHttp APIs that may be overridden for
  // testing.
  std::unique_ptr<WinHttpAPIWrapper> winhttp_api_wrapper_;

  // This is the mapping of WindowsSystemProxyResolutionRequest objects that
  // called GetProxyForUrl() to the handle that's being used for their proxy
  // resolution call. Upon receiving a callback from WinHttp (which includes an
  // HINTERNET handle), a reverse lookup here will get the right
  // WindowsSystemProxyResolutionRequest to use.
  std::unordered_map<WindowsSystemProxyResolutionRequest*, HINTERNET>
      pending_callback_target_map_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace proxy_resolver_win

#endif  // SERVICES_PROXY_RESOLVER_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_IMPL_H_
