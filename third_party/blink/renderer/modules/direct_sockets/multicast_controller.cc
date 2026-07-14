// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/multicast_controller.h"

#include <optional>

#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_multicast_group_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_multicast_membership.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_multicastmembership_string.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Returns nullopt if validation failed (an exception has been thrown).
std::optional<net::IPAddress> ParseAndValidateIPAddress(
    const String& ip_string,
    const StringView& param_name,
    ExceptionState& exception_state) {
  std::optional<net::IPAddress> parsed_ip =
      net::IPAddress::FromIPLiteral(ip_string.Ascii());
  if (!parsed_ip.has_value()) {
    exception_state.ThrowTypeError(
        StrCat({param_name, " is not a valid IPv4 or IPv6 address"}));
    return std::nullopt;
  }

  return parsed_ip;
}

std::optional<net::IPAddress> ParseAndValidateMulticastAddress(
    const String& ip_string,
    const StringView& param_name,
    ExceptionState& exception_state) {
  std::optional<net::IPAddress> parsed_ip =
      ParseAndValidateIPAddress(ip_string, param_name, exception_state);
  if (!parsed_ip.has_value()) {
    return std::nullopt;
  }

  if (!parsed_ip->IsMulticast()) {
    exception_state.ThrowTypeError(
        StrCat({param_name, " must be a valid multicast address"}));
    return std::nullopt;
  }

  return parsed_ip;
}

std::optional<net::IPAddress> ParseAndValidateUnicastAddress(
    const String& ip_string,
    const StringView& param_name,
    ExceptionState& exception_state) {
  std::optional<net::IPAddress> parsed_ip =
      ParseAndValidateIPAddress(ip_string, param_name, exception_state);
  if (!parsed_ip.has_value()) {
    return std::nullopt;
  }

  if (parsed_ip->IsMulticast()) {
    exception_state.ThrowTypeError(StrCat(
        {param_name, " must be a unicast address, not a multicast address"}));
    return std::nullopt;
  }

  if (parsed_ip->IsZero()) {
    exception_state.ThrowTypeError(
        StrCat({param_name, " must not be the zero address (0.0.0.0 or ::)"}));
    return std::nullopt;
  }

  return parsed_ip;
}

std::optional<net::IPAddress> ParseSourceAddressOption(
    const MulticastGroupOptions* options,
    ExceptionState& exception_state) {
  if (!options || !options->hasSourceAddress()) {
    return std::nullopt;
  }

  const String& source_address = options->sourceAddress();
  if (source_address.empty()) {
    exception_state.ThrowTypeError("sourceAddress must not be empty");
    return std::nullopt;
  }

  return ParseAndValidateUnicastAddress(source_address, "sourceAddress",
                                        exception_state);
}

// Parsed and validated group/source IPs for join/leave operations.
struct ParsedGroupIPs {
  net::IPAddress group;
  std::optional<net::IPAddress> source;
};

// Returns nullopt if validation failed (exception is thrown).
std::optional<ParsedGroupIPs> ParseAndValidateGroupIPs(
    const String& ip_address,
    const MulticastGroupOptions* options,
    ExceptionState& exception_state) {
  if (ip_address.empty()) {
    exception_state.ThrowTypeError("ipAddress must not be empty");
    return std::nullopt;
  }

  std::optional<net::IPAddress> group_ip = ParseAndValidateMulticastAddress(
      ip_address, "ipAddress", exception_state);
  if (!group_ip.has_value()) {
    return std::nullopt;
  }

  std::optional<net::IPAddress> source_ip =
      ParseSourceAddressOption(options, exception_state);
  if (exception_state.HadException()) {
    return std::nullopt;
  }

  if (source_ip.has_value()) {
    // IDL already gates the sourceAddress option on the feature flag, so this
    // should be unreachable if the feature is disabled.
    CHECK(RuntimeEnabledFeatures::
              SourceSpecificMulticastInDirectSocketsEnabled());

    if (group_ip->IsIPv4() != source_ip->IsIPv4()) {
      exception_state.ThrowTypeError(
          "sourceAddress and ipAddress must be the same IP version");
      return std::nullopt;
    }
  }

  return ParsedGroupIPs{*group_ip, source_ip};
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
    const MulticastGroupOptions* options,
    ExceptionState& exception_state) {
  if (state_ != State::kOpen) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot join group if the socket is not opened");
    return {};
  }

  std::optional<ParsedGroupIPs> parsed =
      ParseAndValidateGroupIPs(ip_address, options, exception_state);
  if (!parsed.has_value()) {
    return {};
  }

  MembershipKey key(parsed->group, parsed->source);
  const String key_str = key.ToString();

  if (auto it = memberships_.find(key_str); it != memberships_.end()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        it->value.state == Membership::State::kJoined
            ? "Cannot join the same group/source combination again"
            : "Already joining this group/source combination");
    return {};
  }

  // A join is incompatible with a membership for the same group that has the
  // opposite source-ness (ASM has no source, SSM has one). memberships_
  // includes in-flight joins, which closes the TOCTOU window between this
  // check and the OnJoinedGroup callback that commits the membership.
  // Membership counts per socket are small, so the linear scan is negligible.
  const bool joining_ssm = parsed->source.has_value();
  for (const Membership& existing : memberships_.Values()) {
    if (existing.key.group != parsed->group ||
        existing.key.source.has_value() == joining_ssm) {
      continue;
    }
    const bool existing_is_pending =
        existing.state == Membership::State::kJoining;
    String message;
    if (joining_ssm) {
      message = existing_is_pending
                    ? "Cannot join SSM group while an ASM join for the same "
                      "group is pending"
                    : "Cannot join SSM group for which ASM membership already "
                      "exists";
    } else {
      message = existing_is_pending
                    ? "Cannot join ASM group while an SSM join for the same "
                      "group is pending"
                    : "Cannot join ASM group for which SSM membership already "
                      "exists";
    }
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      message);
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  join_group_promises_.insert(key_str, resolver);
  memberships_.insert(key_str, Membership{key, Membership::State::kJoining});

  udp_socket_->get()->JoinGroup(
      parsed->group, parsed->source,
      BindOnce(&MulticastController::OnJoinedGroup, WrapPersistent(this),
               WrapPersistent(resolver), key_str));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MulticastController::leaveGroup(
    ScriptState* script_state,
    const String& ip_address,
    const MulticastGroupOptions* options,
    ExceptionState& exception_state) {
  if (state_ != State::kOpen) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot leave group if the socket is not opened");
    return {};
  }

  std::optional<ParsedGroupIPs> parsed =
      ParseAndValidateGroupIPs(ip_address, options, exception_state);
  if (!parsed.has_value()) {
    return {};
  }

  MembershipKey key(parsed->group, parsed->source);
  const String key_str = key.ToString();

  auto membership_iter = memberships_.find(key_str);
  if (membership_iter == memberships_.end() ||
      membership_iter->value.state != Membership::State::kJoined) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot leave group which is not joined");
    return {};
  }

  if (leave_group_promises_.Contains(key_str)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Already leaving the group");
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  leave_group_promises_.insert(key_str, resolver);

  udp_socket_->get()->LeaveGroup(
      parsed->group, parsed->source,
      BindOnce(&MulticastController::OnLeftGroup, WrapPersistent(this),
               WrapPersistent(resolver), key_str));

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
  memberships_.clear();

  // Reject all pending promises before clearing the maps. Unresolved
  // ScriptPromiseResolvers must be settled before garbage collection,
  // otherwise a DCHECK fires in ScriptPromiseResolverBase::Dispose().
  auto reject_and_clear = [](auto& promises) {
    for (auto& entry : promises) {
      entry.value->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Socket closed."));
    }
    promises.clear();
  };
  reject_and_clear(join_group_promises_);
  reject_and_clear(leave_group_promises_);
}

void MulticastController::OnJoinedGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const String& key,
    int32_t net_error) {
  auto iter = join_group_promises_.find(key);
  if (iter == join_group_promises_.end()) {
    // OnCloseOrAbort() already rejected this promise and cleared the
    // membership.
    return;
  }
  join_group_promises_.erase(iter);

  auto membership_iter = memberships_.find(key);
  CHECK(membership_iter != memberships_.end());
  DCHECK(membership_iter->value.state == Membership::State::kJoining);

  if (net_error == net::OK) {
    membership_iter->value.state = Membership::State::kJoined;
    probe::DirectUDPSocketJoinedMulticastGroup(GetExecutionContext(),
                                               inspector_id_, key);
    resolver->Resolve();
  } else {
    memberships_.erase(membership_iter);
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

void MulticastController::OnLeftGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const String& key,
    int32_t net_error) {
  auto iter = leave_group_promises_.find(key);
  if (iter == leave_group_promises_.end()) {
    // OnCloseOrAbort() already rejected and cleared this promise.
    return;
  }
  leave_group_promises_.erase(iter);

  if (net_error == net::OK) {
    memberships_.erase(key);
    probe::DirectUDPSocketLeftMulticastGroup(GetExecutionContext(),
                                             inspector_id_, key);
    resolver->Resolve();
  } else {
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

HeapVector<Member<V8UnionMulticastMembershipOrString>>
MulticastController::joinedGroups() const {
  HeapVector<Member<V8UnionMulticastMembershipOrString>> result;
  result.ReserveInitialCapacity(memberships_.size());

  for (const Membership& membership : memberships_.Values()) {
    if (membership.state != Membership::State::kJoined) {
      continue;
    }
    const String group_string =
        String::FromUtf8(membership.key.group.ToString());
    if (!membership.key.source.has_value()) {
      result.push_back(MakeGarbageCollected<V8UnionMulticastMembershipOrString>(
          group_string));
    } else {
      auto* v8_membership = MulticastMembership::Create();
      v8_membership->setGroupAddress(group_string);
      v8_membership->setSourceAddress(
          String::FromUtf8(membership.key.source->ToString()));

      result.push_back(MakeGarbageCollected<V8UnionMulticastMembershipOrString>(
          v8_membership));
    }
  }

  return result;
}

}  // namespace blink
