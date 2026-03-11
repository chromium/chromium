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
      read_fd_watcher_(FROM_HERE),
      net_log_(NetLogWithSource::Make(
          NetLog::Get(),
          NetLogSourceType::DNS_TRANSACTION_PLATFORM_ATTEMPT)) {
  parent_net_log.AddEventReferencingSource(
      NetLogEventType::DNS_TRANSACTION_PLATFORM_ATTEMPT, net_log_.source());
  CHECK(delegate_);
}

DnsPlatformAndroidAttempt::~DnsPlatformAndroidAttempt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void DnsPlatformAndroidAttempt::OnFileCanReadWithoutBlocking(int fd) {
  // TODO(https://crbug.com/450545129): Investigate why this happens.
  // This line is important to keep to avoid internal `MessagePumpEpoll` crash.
  read_fd_watcher_.StopWatchingFileDescriptor();

  ReadResponse(fd);
}

void DnsPlatformAndroidAttempt::ReadResponse(int fd) {
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
    std::move(callback_).Run(ERR_DNS_SERVER_REQUIRES_TCP);
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
