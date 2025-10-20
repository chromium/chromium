// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_
#define NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_

#include <string>

#include "base/functional/callback.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/sequence_checker.h"
#include "net/base/address_list.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"

namespace net {

// Performs DNS resolution using Android specific APIs instead of
// getaddrinfo()
//
// This class be used only on Android 29+
// (https://developer.android.com/ndk/reference/group/networking#android_res_nquery).
//
// This class is not thread-safe.
//
// TODO(https://crbug.com/448975408): This class is not production-ready, and is
// under active development. Once development is complete, this TODO will be
// removed.
class NET_EXPORT PlatformDnsQueryExecutorAndroid final
    : private base::MessagePumpForIO::FdWatcher {
 public:
  // TODO(https://crbug.com/452943289): Change AddressList to
  // std::set<std::unique_ptr<HostResolverInternalResult>> to support HTTPS RR.
  using ResultCallback = base::OnceCallback<
      void(const AddressList& addr_list, int os_error, int net_error)>;

  // `hostname` must be a valid domain name, and it's the caller's
  // responsibility to check it before calling this constructor.
  PlatformDnsQueryExecutorAndroid(std::string hostname,
                                  handles::NetworkHandle target_network)
      __INTRODUCED_IN(29);

  PlatformDnsQueryExecutorAndroid(const PlatformDnsQueryExecutorAndroid&) =
      delete;
  PlatformDnsQueryExecutorAndroid& operator=(
      const PlatformDnsQueryExecutorAndroid&) = delete;

  // Cancels this executor. Any outstanding resolve
  // attempts cannot be cancelled.
  ~PlatformDnsQueryExecutorAndroid() override;

  // Starts the `hostname` resolution. `Start()` can be called only once per
  // each instance of `PlatformDnsQueryExecutorAndroid`. Calling it multiple
  // times will result in crash. `result_callback` will be invoked
  // asynchronously on the thread that called `Start()` with the results of the
  // resolution. `result_callback` can destroy `this`.
  void Start(ResultCallback result_callback) __INTRODUCED_IN(29);

 private:
  // `base::MessagePumpForIO::FdWatcher` methods.
  void OnFileCanReadWithoutBlocking(int fd) __INTRODUCED_IN(29) override;
  void OnFileCanWriteWithoutBlocking(int fd) __INTRODUCED_IN(29) override;

  void ReadResponse(int fd) __INTRODUCED_IN(29);

  // Callback for when resolution completes.
  void OnLookupComplete(const AddressList& addr_list,
                        int os_error,
                        int net_error);

  bool IsActive() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !result_callback_.is_null();
  }

  const std::string hostname_;

  const handles::NetworkHandle target_network_;

  base::MessagePumpForIO::FdWatchController read_fd_watcher_;

  // The listener to the results of this executor.
  ResultCallback result_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_
