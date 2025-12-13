// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/multicast_controller.h"

#include <optional>

#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {
std::optional<net::IPAddress> CreateAndCheckIpAddress(
    const String& ip_address,
    ExceptionState& exception_state) {
  if (!ip_address.ContainsOnlyASCIIOrEmpty()) {
    exception_state.ThrowTypeError(
        "ipAddress must contain only ascii characters");
    return std::nullopt;
  }

  std::optional<net::IPAddress> parsed_ip_opt =
      net::IPAddress::FromIPLiteral(ip_address.Ascii());
  if (!parsed_ip_opt.has_value()) {
    exception_state.ThrowTypeError("ipAddress is not valid ipv4 or ipv6");
    return std::nullopt;
  }
  return parsed_ip_opt;
}
}  // namespace

MulticastController::MulticastController(ExecutionContext* execution_context,
                                         UDPSocketMojoRemote* udp_socket,
                                         uint64_t inspector_id)
    : ExecutionContextClient(execution_context),
      udp_socket_(udp_socket),
      inspector_id_(inspector_id) {}

MulticastController::~MulticastController() = default;

ScriptPromise<IDLUndefined> MulticastController::joinGroup(
    ScriptState* script_state,
    const String& ip_address,
    ExceptionState& exception_state) {
  if (state_ != State::kOpen) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot join group if the socket is not opened");
    return {};
  }

  std::optional<net::IPAddress> parsed_ip_opt =
      CreateAndCheckIpAddress(ip_address, exception_state);
  if (!parsed_ip_opt.has_value()) {
    return {};
  }
  auto normalized_ip = String::FromUTF8(parsed_ip_opt->ToString());
  if (joined_groups_.Contains(normalized_ip)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot join the same group again");
    return {};
  }

  if (join_group_promises_.Contains(normalized_ip)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Already joining this group");
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  join_group_promises_.insert(normalized_ip, resolver);

  udp_socket_->get()->JoinGroup(
      *parsed_ip_opt,
      BindOnce(&MulticastController::OnJoinedGroup, WrapPersistent(this),
               WrapPersistent(resolver), normalized_ip));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MulticastController::leaveGroup(
    ScriptState* script_state,
    const String& ip_address,
    ExceptionState& exception_state) {
  if (state_ != State::kOpen) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot leave group if the socket is not opened");
    return {};
  }

  std::optional<net::IPAddress> parsed_ip_opt =
      CreateAndCheckIpAddress(ip_address, exception_state);
  if (!parsed_ip_opt.has_value()) {
    return {};
  }

  auto normalized_ip = String::FromUTF8(parsed_ip_opt->ToString());
  if (!joined_groups_.Contains(normalized_ip)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot leave group which is not joined");
    return {};
  }

  if (leave_group_promises_.Contains(normalized_ip)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Already leaving the group");
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  leave_group_promises_.insert(normalized_ip, resolver);

  udp_socket_->get()->LeaveGroup(
      *parsed_ip_opt,
      BindOnce(&MulticastController::OnLeftGroup, WrapPersistent(this),
               WrapPersistent(resolver), normalized_ip));

  return resolver->Promise();
}

void MulticastController::Trace(Visitor* visitor) const {
  visitor->Trace(join_group_promises_);
  visitor->Trace(leave_group_promises_);
  visitor->Trace(udp_socket_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool MulticastController::HasPendingActivity() const {
  return !join_group_promises_.empty() || !leave_group_promises_.empty();
}

void MulticastController::OnCloseOrAbort() {
  state_ = State::kClosed;
  joined_groups_.clear();
  join_group_promises_.clear();
  leave_group_promises_.clear();
}

void MulticastController::OnJoinedGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    String normalized_ip_address,
    int32_t net_error) {
  auto iter = join_group_promises_.find(normalized_ip_address);
  CHECK(iter != join_group_promises_.end());
  join_group_promises_.erase(iter);

  if (net_error == net::OK) {
    // Prevent duplicated entries in corner cases.
    if (!joined_groups_.Contains(normalized_ip_address)) {
      joined_groups_.push_back(normalized_ip_address);
      probe::DirectUDPSocketJoinedMulticastGroup(
          GetExecutionContext(), inspector_id_, normalized_ip_address);
    }
    resolver->Resolve();
  } else {
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

void MulticastController::OnLeftGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    String normalized_ip_address,
    int32_t net_error) {
  auto iter = leave_group_promises_.find(normalized_ip_address);
  CHECK(iter != leave_group_promises_.end());
  leave_group_promises_.erase(iter);

  if (net_error == net::OK) {
    Erase(joined_groups_, normalized_ip_address);
    probe::DirectUDPSocketLeftMulticastGroup(
        GetExecutionContext(), inspector_id_, normalized_ip_address);
    resolver->Resolve();
  } else {
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

}  // namespace blink
