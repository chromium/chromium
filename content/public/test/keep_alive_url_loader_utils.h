// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_
#define CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

class BrowserContext;
class KeepAliveURLLoadersTestObserverImpl;

// Observes behaviors of all `KeepAliveURLLoader` instances in synchronous way.
//
// KeepAliveURLLoader itself is running in browser UI thread, but there can be
// multiple instances created and triggered by different renderers.
// For example:
//   - Renderer A triggers `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     and then `KeepAliveURLLoader::OnReceiveResponseProcessed()
//   - Renderer B triggers `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     twice.
//   - Users can call `WaitForTotalOnReceiveRedirectForwarded()` to wait until
//     all 3 triggerings of `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     completed.
//
// The methods provided by this class can also be used to assert that
// KeepAliveURLLoader has entered certain state, e.g.
// `WaitForTotalOnReceiveRedirectProcessed()` indicates the loader handles the
// redirect itself rather than handing over to renderer as renderer is gone.
class KeepAliveURLLoadersTestObserver {
 public:
  // Begins observing the internal states of all instances of KeepAliveURLLoader
  // created under the given `browser_context` immediately.
  explicit KeepAliveURLLoadersTestObserver(BrowserContext* browser_context);
  ~KeepAliveURLLoadersTestObserver();

  // Not Copyable.
  KeepAliveURLLoadersTestObserver(const KeepAliveURLLoadersTestObserver&) =
      delete;
  KeepAliveURLLoadersTestObserver& operator=(
      const KeepAliveURLLoadersTestObserver&) = delete;

  // Waits for `KeepAliveURLLoader::OnReceiveRedirectForwarded()` to be called
  // `total` times.
  void WaitForTotalOnReceiveRedirectForwarded(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveRedirectProcessed()` to be called
  // `total` times.
  void WaitForTotalOnReceiveRedirectProcessed(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponse()` to be called `total`
  // times.
  void WaitForTotalOnReceiveResponse(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponseForwarded()` to be called
  // `total` times.
  void WaitForTotalOnReceiveResponseForwarded(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponseProcessed()` to be called
  // `total` times.
  void WaitForTotalOnReceiveResponseProcessed(size_t total);
  // Waits for `KeepAliveURLLoader::OnComplete()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnComplete(const std::vector<int>& error_codes);
  // Waits for `KeepAliveURLLoader::OnCompleteForwarded()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnCompleteForwarded(const std::vector<int>& error_codes);
  // Waits for `KeepAliveURLLoader::OnCompleteProcessed()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnCompleteProcessed(const std::vector<int>& error_codes);
  // Waits for `KeepAliveURLLoader::PauseReadingBodyFromNetProcessed()` to be
  // called `total` times.
  void WaitForTotalPauseReadingBodyFromNetProcessed(size_t total);
  // Waits for `KeepAliveURLLoader::ResumeReadingBodyFromNetProcessed()` to be
  // called `total` times.
  void WaitForTotalResumeReadingBodyFromNetProcessed(size_t total);

 private:
  std::unique_ptr<KeepAliveURLLoadersTestObserverImpl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_
