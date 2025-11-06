// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_
#define NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_

#include <android/multinetwork.h>
#include <android/versioning.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/host_resolver_internal_result.h"

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
  using Results = std::set<std::unique_ptr<HostResolverInternalResult>>;
  using ResultsCallback =
      base::OnceCallback<void(Results results, int os_error, int net_error)>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // An abstraction over the `android_res_nquery` DNS resolution API to allow
    // for mocking in tests.
    virtual int Query(net_handle_t network,
                      base::cstring_view dname,
                      int ns_class,
                      int ns_type,
                      uint32_t flags) = 0;

    // An abstraction over the `android_res_nresult` DNS resolution API to
    // allow for mocking in tests.
    virtual int Result(int fd, int* rcode, base::span<uint8_t> answer) = 0;
  };

  class DelegateImpl final : public PlatformDnsQueryExecutorAndroid::Delegate {
   public:
    DelegateImpl() __INTRODUCED_IN(29);
    ~DelegateImpl() override;

    int Query(net_handle_t network,
              base::cstring_view dname,
              int ns_class,
              int ns_type,
              uint32_t flags) __INTRODUCED_IN(29) override;

    int Result(int fd, int* rcode, base::span<uint8_t> answer)
        __INTRODUCED_IN(29) override;
  };

  // `hostname` must be a valid domain name, and it's the caller's
  // responsibility to check it before calling this constructor.
  PlatformDnsQueryExecutorAndroid(std::string hostname,
                                  handles::NetworkHandle target_network,
                                  Delegate* delegate) __INTRODUCED_IN(29);

  PlatformDnsQueryExecutorAndroid(const PlatformDnsQueryExecutorAndroid&) =
      delete;
  PlatformDnsQueryExecutorAndroid& operator=(
      const PlatformDnsQueryExecutorAndroid&) = delete;

  // Cancels this executor. Any outstanding resolve
  // attempts cannot be cancelled.
  ~PlatformDnsQueryExecutorAndroid() override;

  // Starts the `hostname` resolution. `Start()` can be called only once per
  // each instance of `PlatformDnsQueryExecutorAndroid`. Calling it multiple
  // times will result in crash. `results_callback` will be invoked
  // asynchronously on the thread that called `Start()` with the results of the
  // resolution. `results_callback` can destroy `this`.
  void Start(ResultsCallback results_callback) __INTRODUCED_IN(29);

 private:
  // `base::MessagePumpForIO::FdWatcher` methods.
  void OnFileCanReadWithoutBlocking(int fd) __INTRODUCED_IN(29) override;
  void OnFileCanWriteWithoutBlocking(int fd) __INTRODUCED_IN(29) override;

  void ReadResponse(int fd) __INTRODUCED_IN(29);

  // Callback for when resolution completes.
  void OnLookupComplete(Results results, int os_error, int net_error);

  bool IsActive() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !results_callback_.is_null();
  }

  const std::string hostname_;

  const handles::NetworkHandle target_network_;

  const raw_ptr<Delegate> delegate_;

  base::MessagePumpForIO::FdWatchController read_fd_watcher_;

  // The listener to the results of this executor.
  ResultsCallback results_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_
