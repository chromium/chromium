// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/platform_dns_query_executor_android.h"

#include <android/multinetwork.h>
#include <android/versioning.h>
#include <stdint.h>

#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/task/current_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

// TODO(https://crbug.com/458035179): Investigate what buffer size to pass to
// android_res_nresult. The maximum possible DNS packet size (including TCP) is
// 64kb. Currently, we're using 8kb for practical reasons, but that might
// theoretically result in a truncated response and a failed lookup. Should we
// retry with 64kb in such cases?
constexpr int kResponseBufferSize = 8 * 1024;

// TODO(https://crbug.com/452586797): Verify this conversion logic is correct.
net_handle_t MapNetworkHandle(handles::NetworkHandle network) {
  if (network == handles::kInvalidNetworkHandle) {
    return NETWORK_UNSPECIFIED;
  }
  return static_cast<net_handle_t>(network);
}

}  // namespace

PlatformDnsQueryExecutorAndroid::DelegateImpl::DelegateImpl() = default;
PlatformDnsQueryExecutorAndroid::DelegateImpl::~DelegateImpl() = default;

int PlatformDnsQueryExecutorAndroid::DelegateImpl::Query(
    net_handle_t network,
    base::cstring_view dname,
    uint16_t dns_query_type) {
  return android_res_nquery(network, dname.c_str(), dns_query_type,
                            dns_protocol::kClassIN,
                            /*flags=*/0);
}

int PlatformDnsQueryExecutorAndroid::DelegateImpl::Result(
    int fd,
    int* rcode,
    base::span<uint8_t> answer) {
  return android_res_nresult(fd, rcode, answer.data(), answer.size());
}

PlatformDnsQueryExecutorAndroid::PlatformDnsQueryExecutorAndroid(
    std::string hostname,
    uint16_t dns_query_type,
    handles::NetworkHandle target_network,
    Delegate* delegate)
    : hostname_(std::move(hostname)),
      dns_query_type_(dns_query_type),
      target_network_(target_network),
      delegate_(delegate),
      read_fd_watcher_(FROM_HERE) {
  // `hostname` must be a valid domain name, and it's the caller's
  // responsibility to check it before calling this constructor.
  DCHECK(dns_names_util::IsValidDnsName(hostname_))
      << "Invalid hostname: " << hostname_;
  CHECK(delegate_);
}

PlatformDnsQueryExecutorAndroid::~PlatformDnsQueryExecutorAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PlatformDnsQueryExecutorAndroid::Start(ResultsCallback results_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(results_callback);
  CHECK(!results_callback_);
  results_callback_ = std::move(results_callback);

  int fd = delegate_->Query(MapNetworkHandle(target_network_), hostname_,
                            dns_query_type_);
  if (fd < 0) {
    // TODO(https://crbug.com/451557941): Consider whether this should surface a
    // lower-level system error.
    OnLookupComplete(base::unexpected(ERR_NAME_NOT_RESOLVED));
    return;
  }

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          fd, /*persistent=*/false, base::MessagePumpForIO::WATCH_READ,
          &read_fd_watcher_, this)) {
    // TODO(https://crbug.com/451557941): Consider whether this should surface a
    // specific error.
    OnLookupComplete(base::unexpected(ERR_NAME_NOT_RESOLVED));
    return;
  }
}

void PlatformDnsQueryExecutorAndroid::OnFileCanReadWithoutBlocking(int fd) {
  // TODO(https://crbug.com/450545129): Investigate why this happens.
  // This line is important to keep to avoid internal `MessagePumpEpoll` crash.
  read_fd_watcher_.StopWatchingFileDescriptor();

  ReadResponse(fd);
}

void PlatformDnsQueryExecutorAndroid::ReadResponse(int fd) {
  int rcode = -1;
  auto answer_buf = base::MakeRefCounted<GrowableIOBuffer>();
  answer_buf->SetCapacity(kResponseBufferSize);
  int rv = delegate_->Result(fd, &rcode, answer_buf->span());

  if (rv < 0) {
    // TODO(https://crbug.com/451557941): Consider whether this should surface a
    // lower-level system error.
    OnLookupComplete(base::unexpected(ERR_NAME_NOT_RESOLVED));
    return;
  }

  if (rcode != dns_protocol::kRcodeNOERROR) {
    // TODO(https://crbug.com/451557941): Consider whether we should do this
    // mapping here, based on `rcode`, or if we can just relying on the caller
    // to retrieve `rcode` from the response.
    OnLookupComplete(base::unexpected(ERR_NAME_NOT_RESOLVED));
    return;
  }

  // TODO(https://crbug.com/458035179): We currently communicate the DNS
  // response size via this IOBuffer's capacity. Investigate whether we care and
  // if we do how this can be made more performant.
  answer_buf->SetCapacity(rv);
  OnLookupComplete(answer_buf);
}

void PlatformDnsQueryExecutorAndroid::OnLookupComplete(
    base::expected<scoped_refptr<net::IOBuffer>, int> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActive());

  std::move(results_callback_).Run(result);
  // Running `results_callback_` can delete `this`.
}

void PlatformDnsQueryExecutorAndroid::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected write on file descriptor.";
}

}  // namespace net
