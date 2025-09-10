// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/multicast_controller.h"

#include <optional>

#include "net/base/ip_address.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

MulticastController::MulticastController(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context) {}

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
    exception_state.ThrowTypeError("Cannot join the same group again");
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  // TODO(crbug.com/398934282): join the group. For now promise never resolves.
  resolver->SuppressDetachCheck();

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

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  // TODO(crbug.com/398934282): join the group. For now promise never resolves.
  resolver->SuppressDetachCheck();

  return resolver->Promise();
}

void MulticastController::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool MulticastController::HasPendingActivity() const {
  return false;
}

void MulticastController::OnCloseOrAbort() {
  state_ = State::kClosed;
  joined_groups_.clear();
}

}  // namespace blink
