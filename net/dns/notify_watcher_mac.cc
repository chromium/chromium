// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/notify_watcher_mac.h"

#include <notify.h>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/posix/eintr_wrapper.h"

namespace net {

NotifyWatcherMac::NotifyWatcherMac() : notify_fd_(-1), notify_token_(-1) {}

NotifyWatcherMac::~NotifyWatcherMac() {
  Cancel();
}

bool NotifyWatcherMac::Watch(const char* key, const CallbackType& callback) {
  DCHECK(key);
  DCHECK(!callback.is_null());
  Cancel();
  uint32_t status = notify_register_file_descriptor(
      key, &notify_fd_, 0, &notify_token_);
  if (status != NOTIFY_STATUS_OK)
    return false;
  DCHECK_GE(notify_fd_, 0);
  watcher_ = base::FileDescriptorWatcher::WatchReadable(
      notify_fd_,
      base::BindRepeating(&NotifyWatcherMac::OnFileCanReadWithoutBlocking,
                          base::Unretained(this)));
  callback_ = callback;
  return true;
}

void NotifyWatcherMac::Cancel() {
  if (notify_fd_ >= 0) {
    CancelInternal();
  }
}

void NotifyWatcherMac::OnFileCanReadWithoutBlocking() {
  int token;
  int status = HANDLE_EINTR(read(notify_fd_, &token, sizeof(token)));
  if (status != sizeof(token)) {
    CancelInternal().Run(false);
    return;
  }
  // Ignoring |token| value to avoid possible endianness mismatch:
  // https://openradar.appspot.com/8821081
  callback_.Run(true);
}

NotifyWatcherMac::CallbackType NotifyWatcherMac::CancelInternal() {
  DCHECK_GE(notify_fd_, 0);

  watcher_.reset();
  notify_cancel(notify_token_);  // Also closes |notify_fd_|.
  notify_fd_ = -1;

  return std::move(callback_);
}

}  // namespace net
