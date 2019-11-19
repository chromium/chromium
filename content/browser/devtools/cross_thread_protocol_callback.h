// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_CROSS_THREAD_PROTOCOL_CALLBACK_H_
#define CONTENT_BROWSER_DEVTOOLS_CROSS_THREAD_PROTOCOL_CALLBACK_H_

#include <memory>

#include "base/task/post_task.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// A wrapper for a DevTools protocol method callback that assures the
// underlying callback is called on the correct thread. Use this to pass
// the protocol callback to methods handling DevTools commands on threads
// other than UI.
template <typename ProtocolCallback>
class CrossThreadProtocolCallback {
 public:
  explicit CrossThreadProtocolCallback(
      std::unique_ptr<ProtocolCallback> callback)
      : callback_(std::move(callback)) {}
  CrossThreadProtocolCallback(CrossThreadProtocolCallback&& r) = default;

  template <typename... Args>
  void sendSuccess(Args&&... args) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ProtocolCallback::sendSuccess, std::move(callback_),
                       std::forward<Args>(args)...));
  }

  void sendFailure(protocol::DispatchResponse response) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ProtocolCallback::sendFailure,
                                  std::move(callback_), std::move(response)));
  }

  ~CrossThreadProtocolCallback() {
    BrowserThread::DeleteSoon({BrowserThread::UI}, FROM_HERE,
                              std::move(callback_));
  }

 private:
  std::unique_ptr<ProtocolCallback> callback_;
};

template <typename ProtocolCallback>
CrossThreadProtocolCallback<ProtocolCallback> WrapForAnotherThread(
    std::unique_ptr<ProtocolCallback> callback) {
  return CrossThreadProtocolCallback<ProtocolCallback>(std::move(callback));
}

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_CROSS_THREAD_PROTOCOL_CALLBACK_H_