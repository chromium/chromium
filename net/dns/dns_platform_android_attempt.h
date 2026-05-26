// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_PLATFORM_ANDROID_ATTEMPT_H_
#define NET_DNS_DNS_PLATFORM_ANDROID_ATTEMPT_H_

#include <android/multinetwork.h>
#include <android/versioning.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_attempt.h"

namespace net {

// Implementation of DnsAttempt that relies on Android specific APIs instead of
// writing (and reading) DNS packets to (and from) a socket.
//
// This class be used only on Android 29+
// (https://developer.android.com/ndk/reference/group/networking#android_res_nquery).
//
// This class is not thread-safe.
class NET_EXPORT DnsPlatformAndroidAttempt final
    : public DnsAttempt,
      private base::MessagePumpForIO::FdWatcher {
 public:

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // An abstraction over the `android_res_nquery` DNS resolution API to allow
    // for mocking in tests.
    virtual int Query(net_handle_t network,
                      base::cstring_view hostname,
                      uint16_t dns_query_type) = 0;

    // An abstraction over the `android_res_nresult` DNS resolution API to
    // allow for mocking in tests.
    virtual int Result(int fd, int* rcode, base::span<uint8_t> answer) = 0;

    // An abstraction over the POSIX `close()` API to allow for mocking in
    // tests.
    virtual void Close(int fd) = 0;
  };

  class DelegateImpl final : public DnsPlatformAndroidAttempt::Delegate {
   public:
    DelegateImpl() __INTRODUCED_IN(29);
    ~DelegateImpl() override;

    int Query(net_handle_t network,
              base::cstring_view hostname,
              uint16_t dns_query_type) __INTRODUCED_IN(29) override;

    int Result(int fd, int* rcode, base::span<uint8_t> answer)
        __INTRODUCED_IN(29) override;

    void Close(int fd) override;
  };

  // `hostname` must be a valid domain name, and it's the caller's
  // responsibility to check it before calling this constructor.
  DnsPlatformAndroidAttempt(size_t server_index,
                                  base::span<const uint8_t> hostname,
                                  uint16_t dns_query_type,
                                  handles::NetworkHandle target_network,
                                  Delegate* delegate,
                                  const NetLogWithSource& net_log)
      __INTRODUCED_IN(29);

  DnsPlatformAndroidAttempt(const DnsPlatformAndroidAttempt&) =
      delete;
  DnsPlatformAndroidAttempt& operator=(
      const DnsPlatformAndroidAttempt&) = delete;

  // Cancels this executor. Any outstanding resolve
  // attempts cannot be cancelled.
  ~DnsPlatformAndroidAttempt() override;

  // DnsAttempt methods.
  int Start(CompletionOnceCallback callback) __INTRODUCED_IN(29) override;
  const DnsQuery* GetQuery() const override;
  const DnsResponse* GetResponse() const override;
  base::Value GetRawResponseBufferForLog() const override;
  const NetLogWithSource& GetSocketNetLog() const override;
  bool IsPending() const override;

 private:
  enum class State {
    kNone,
    kQuery,
    kQueryComplete,
    kReadResponse,
    kReadResponseComplete,
  };

  int DoLoop(int result) __INTRODUCED_IN(29);
  int DoQuery() __INTRODUCED_IN(29);
  int DoQueryComplete(int result) __INTRODUCED_IN(29);
  int DoReadResponse() __INTRODUCED_IN(29);
  int DoReadResponseComplete(int result);

  // `base::MessagePumpForIO::FdWatcher` methods.
  void OnFileCanReadWithoutBlocking(int fd) __INTRODUCED_IN(29) override;
  void OnFileCanWriteWithoutBlocking(int fd) __INTRODUCED_IN(29) override;

  const std::string hostname_;
  const uint16_t dns_query_type_;
  const handles::NetworkHandle target_network_;
  const raw_ptr<Delegate> delegate_;
  const NetLogWithSource net_log_;

  int fd_;
  CompletionOnceCallback callback_;
  std::unique_ptr<DnsResponse> response_;
  base::MessagePumpForIO::FdWatchController read_fd_watcher_;
  State next_state_ = State::kNone;
  scoped_refptr<GrowableIOBuffer> read_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_DNS_PLATFORM_DNS_QUERY_EXECUTOR_ANDROID_H_