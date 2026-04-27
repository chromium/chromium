// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_platform_android_attempt.h"

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
#include "base/posix/eintr_wrapper.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/log/net_log_values.h"

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

constexpr int kInvalidFd = -1;

}  // namespace

DnsPlatformAndroidAttempt::DelegateImpl::DelegateImpl() = default;
DnsPlatformAndroidAttempt::DelegateImpl::~DelegateImpl() = default;

int DnsPlatformAndroidAttempt::DelegateImpl::Query(
    net_handle_t network,
    base::cstring_view hostname,
    uint16_t dns_query_type) {
  return android_res_nquery(network, hostname.c_str(), dns_query_type,
                            dns_protocol::kClassIN,
                            /*flags=*/0);
}

int DnsPlatformAndroidAttempt::DelegateImpl::Result(
    int fd,
    int* rcode,
    base::span<uint8_t> answer) {
  return android_res_nresult(fd, rcode, answer.data(), answer.size());
}

void DnsPlatformAndroidAttempt::DelegateImpl::Close(int fd) {
  if (IGNORE_EINTR(close(fd)) < 0) {
    DPLOG(ERROR) << "close() failed";
  }
}

DnsPlatformAndroidAttempt::DnsPlatformAndroidAttempt(
    size_t server_index,
    base::span<const uint8_t> hostname,
    uint16_t dns_query_type,
    handles::NetworkHandle target_network,
    Delegate* delegate,
    const NetLogWithSource& parent_net_log)
    : DnsAttempt(server_index),
      hostname_(dns_names_util::NetworkToDottedName(hostname).value()),
      dns_query_type_(dns_query_type),
      target_network_(target_network),
      delegate_(delegate),
      net_log_(NetLogWithSource::Make(
          NetLog::Get(),
          NetLogSourceType::DNS_TRANSACTION_PLATFORM_ATTEMPT)),
      fd_(kInvalidFd),
      read_fd_watcher_(FROM_HERE) {
  parent_net_log.AddEventReferencingSource(
      NetLogEventType::DNS_TRANSACTION_PLATFORM_ATTEMPT, net_log_.source());
  CHECK(delegate_);
}

DnsPlatformAndroidAttempt::~DnsPlatformAndroidAttempt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fd_ >= 0) {
    // Destructing `read_fd_watcher_` stops it from watching the file descriptor
    // it tracks (see MessagePumpEpoll::FdWatchController::~FdWatchController).
    // This is done by stopping all epoll events, in the underlying
    // MessagePumpEpoll, associated with the fd being watched.
    // This means that, unless we stop watching the fd now, when it's still
    // valid, the epoll_ctl call within MessagePumpEpoll::StopEpollEvent will
    // crash due to EBADF (since we are about to close it).
    read_fd_watcher_.StopWatchingFileDescriptor();
    delegate_->Close(fd_);
  }
}

int DnsPlatformAndroidAttempt::Start(CompletionOnceCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  // StartInternal can currently call the onLookupComplete callback
  // synchronously. This does not play well with the expectation of the caller.
  // TODO(crbug.com/491090786): Until this is fixed, work around this by posting
  // onto ourselves and always returning ERR_IO_PENDING.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DnsPlatformAndroidAttempt::StartInternal,
                                weak_factory_.GetWeakPtr()));
  return ERR_IO_PENDING;
}

void DnsPlatformAndroidAttempt::StartInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fd_ = delegate_->Query(MapNetworkHandle(target_network_), hostname_,
                         dns_query_type_);
  if (fd_ < 0) {
    OnLookupComplete(base::unexpected(MapSystemError(-fd_)));
    return;
  }

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          fd_, /*persistent=*/false, base::MessagePumpForIO::WATCH_READ,
          &read_fd_watcher_, this)) {
    OnLookupComplete(base::unexpected(ERR_FAILED));
    return;
  }
}

void DnsPlatformAndroidAttempt::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd, fd_);
  // Destructing `read_fd_watcher_` stops it from watching the file descriptor
  // it tracks (see MessagePumpEpoll::FdWatchController::~FdWatchController).
  // This is done by stopping all epoll events, in the underlying
  // MessagePumpEpoll, associated with the fd being watched. This is usually
  // fine. However, android_res_nresult always closes the fd it receives before
  // returning
  // (https://developer.android.com/ndk/reference/group/networking#android_res_nresult).
  // This means that, unless we stop watching the fd now, when it's still valid,
  // the epoll_ctl call within MessagePumpEpoll::StopEpollEvent will crash due
  // to EBADF (since it has already been closed by android_res_nresult).
  read_fd_watcher_.StopWatchingFileDescriptor();
  ReadResponse();
}

void DnsPlatformAndroidAttempt::ReadResponse() {
  // Note: we consciously do not use the rcode value from android_res_nresult.
  // Instead, we rely on rcode within the DNS response itself. These two always
  // match, except when android_res_nresult fails to populate the response
  // buffer, in which case we simply do not have access to the DNS response. In
  // this scenario, android_res_nresult will return a negative value, indicating
  // an error, we should fail the lookup with that instead. See
  // https://cs.android.com/android/platform/superproject/main/+/main:system/netd/client/NetdClient.cpp;l=563-579;drc=ed285b9c6b449b68321fd163d1444e62322fd9de)
  int rcode = dns_protocol::kRcodeNOERROR;
  auto answer_buf = base::MakeRefCounted<GrowableIOBuffer>();
  answer_buf->SetCapacity(kResponseBufferSize);
  int rv = delegate_->Result(fd_, &rcode, answer_buf->span());
  // android_res_nresult takes care of closing `fd_`. Invalidate `fd_` to
  // prevent closing it twice.
  fd_ = kInvalidFd;

  if (rv < 0) {
    OnLookupComplete(base::unexpected(MapSystemError(-rv)));
    return;
  }

  // TODO(https://crbug.com/458035179): We currently communicate the DNS
  // response size via this IOBuffer's capacity. Investigate whether we care and
  // if we do how this can be made more performant.
  answer_buf->SetCapacity(rv);
  OnLookupComplete(answer_buf);
}

void DnsPlatformAndroidAttempt::OnLookupComplete(
    base::expected<scoped_refptr<net::IOBuffer>, int> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActive());

  if (!result.has_value()) {
    std::move(callback_).Run(result.error());
    return;
  }

  scoped_refptr<IOBuffer> buffer = result.value();
  response_ = std::make_unique<DnsResponse>(buffer, buffer->size());
  if (!response_->InitParseWithoutQuery(buffer->size())) {
    std::move(callback_).Run(ERR_DNS_MALFORMED_RESPONSE);
    return;
  }

  if (response_->flags() & dns_protocol::kFlagTC) {
    // dns_protocol::kFlagTC is reported by a server when the response would
    // have been larger than what the underlying transmission channel allows.
    // This usually happens for DNS over UDP, where usually we fallback onto DNS
    // over TCP. Having said that, the platform API we're using should be the
    // one falling back onto TCP, it should never return a truncated response.
    // Note: this is consistent with how dns_protocol::kFlagTC is handled by
    // DnsTcpAttempt.
    std::move(callback_).Run(ERR_UNEXPECTED);
    return;
  }
  if (response_->rcode() != dns_protocol::kRcodeNOERROR) {
    std::move(callback_).Run(FailureRcodeToNetError(response_->rcode()));
    return;
  }

  std::move(callback_).Run(OK);
}

void DnsPlatformAndroidAttempt::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected write on file descriptor.";
}

const DnsQuery* DnsPlatformAndroidAttempt::GetQuery() const {
  NOTREACHED() << "The internal Android API being called takes care of "
                  "constructing the query and does not provide access to it.";
}

const DnsResponse* DnsPlatformAndroidAttempt::GetResponse() const {
  const DnsResponse* resp = response_.get();
  return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
}

base::Value DnsPlatformAndroidAttempt::GetRawResponseBufferForLog()
    const {
  if (!response_) {
    return base::Value();
  }
  return NetLogBinaryValue(response_->io_buffer()->data(),
                           response_->io_buffer_size());
}

const NetLogWithSource& DnsPlatformAndroidAttempt::GetSocketNetLog()
    const {
  return net_log_;
}

bool DnsPlatformAndroidAttempt::IsPending() const {
  return !callback_.is_null();
}

}  // namespace net
