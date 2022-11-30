// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/mock_pac_file_fetcher.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"

namespace net {

MockPacFileFetcher::MockPacFileFetcher() = default;

MockPacFileFetcher::~MockPacFileFetcher() = default;

// PacFileFetcher implementation.
int MockPacFileFetcher::Fetch(
    const GURL& url,
    std::u16string* text,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag traffic_annotation) {
  DCHECK(!has_pending_request());

  if (on_fetch_complete_)
    std::move(on_fetch_complete_).Run();

  if (is_shutdown_)
    return ERR_CONTEXT_SHUT_DOWN;

  // Save the caller's information, and have them wait.
  pending_request_url_ = url;
  pending_request_callback_ = std::move(callback);
  pending_request_text_ = text;

  return ERR_IO_PENDING;
}

void MockPacFileFetcher::NotifyFetchCompletion(int result,
                                               const std::string& ascii_text) {
  DCHECK(has_pending_request());
  *pending_request_text_ = base::ASCIIToUTF16(ascii_text);
  std::move(pending_request_callback_).Run(result);
}

void MockPacFileFetcher::Cancel() {
  pending_request_callback_.Reset();
}

void MockPacFileFetcher::OnShutdown() {
  is_shutdown_ = true;
  if (pending_request_callback_) {
    std::move(pending_request_callback_).Run(ERR_CONTEXT_SHUT_DOWN);
  }
}

URLRequestContext* MockPacFileFetcher::GetRequestContext() const {
  return nullptr;
}

const GURL& MockPacFileFetcher::pending_request_url() const {
  return pending_request_url_;
}

bool MockPacFileFetcher::has_pending_request() const {
  return !pending_request_callback_.is_null();
}

void MockPacFileFetcher::WaitUntilFetch() {
  DCHECK(!has_pending_request());
  base::RunLoop run_loop;
  on_fetch_complete_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace net
