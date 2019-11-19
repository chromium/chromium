// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/notify_watcher_mac.h"

#include <notify.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/posix/eintr_wrapper.h"

namespace net {

namespace {

// Registers a dummy file descriptor to workaround a bug in libnotify
// in macOS 10.12
// See https://bugs.chromium.org/p/chromium/issues/detail?id=783148.
class NotifyFileDescriptorsGlobalsHolder {
 public:
  NotifyFileDescriptorsGlobalsHolder() {
    int notify_fd = -1;
    int notify_token = -1;
    notify_register_file_descriptor("notify_file_descriptor_holder", &notify_fd,
                                    0, &notify_token);
  }
};

void HoldNotifyFileDescriptorsGlobals() {
  if (base::mac::IsAtMostOS10_12()) {
    static NotifyFileDescriptorsGlobalsHolder holder;
  }
}
}  // namespace

NotifyWatcherMac::NotifyWatcherMac() : notify_fd_(-1), notify_token_(-1) {
  HoldNotifyFileDescriptorsGlobals();
}

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
    notify_cancel(notify_token_);  // Also closes |notify_fd_|.
    notify_fd_ = -1;
    callback_.Reset();
    watcher_.reset();
  }
}

void NotifyWatcherMac::OnFileCanReadWithoutBlocking() {
  int token;
  int status = HANDLE_EINTR(read(notify_fd_, &token, sizeof(token)));
  if (status != sizeof(token)) {
    Cancel();
    callback_.Run(false);
    return;
  }
  // Ignoring |token| value to avoid possible endianness mismatch:
  // http://openradar.appspot.com/8821081
  callback_.Run(true);
}

}  // namespace net
