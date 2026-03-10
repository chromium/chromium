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
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

std::optional<net::IPAddress> ParseAndValidateIPAddress(
    const String& ip_string,
    const char* param_name,
    ExceptionState& exception_state) {
  if (ip_string.IsNull() || ip_string.empty()) {
    return std::nullopt;  // Null or empty is valid for optional parameters
  }

  std::optional<net::IPAddress> parsed_ip =
      net::IPAddress::FromIPLiteral(ip_string.Ascii());
  if (!parsed_ip.has_value()) {
    exception_state.ThrowTypeError(String(param_name) +
                                   " is not a valid IPv4 or IPv6 address");
    return std::nullopt;
  }

  return parsed_ip;
}

std::optional<net::IPAddress> ParseAndValidateMulticastAddress(
    const String& ip_string,
    const char* param_name,
    ExceptionState& exception_state) {
  std::optional<net::IPAddress> parsed_ip =
      ParseAndValidateIPAddress(ip_string, param_name, exception_state);
  if (!parsed_ip.has_value()) {
    return std::nullopt;
  }

  if (!parsed_ip->IsMulticast()) {
    exception_state.ThrowTypeError(String(param_name) +
                                   " must be a valid multicast address");
    return std::nullopt;
  }

  return parsed_ip;
}

std::optional<net::IPAddress> ParseAndValidateUnicastAddress(
    const String& ip_string,
    const char* param_name,
    ExceptionState& exception_state) {
  std::optional<net::IPAddress> parsed_ip =
      ParseAndValidateIPAddress(ip_string, param_name, exception_state);
  if (!parsed_ip.has_value()) {
    return std::nullopt;
  }

  if (parsed_ip->IsMulticast()) {
    exception_state.ThrowTypeError(
        String(param_name) +
        " must be a unicast address, not a multicast address");
    return std::nullopt;
  }

  if (parsed_ip->IsZero()) {
    exception_state.ThrowTypeError(
        String(param_name) + " must not be the zero address (0.0.0.0 or ::)");
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
    CHECK(exception_state.HadException());
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

  if (joined_groups_.Contains(key)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot join the same group/source combination again");
    return {};
  }

  if (join_group_promises_.Contains(key)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Already joining this group/source combination");
    return {};
  }

  // Check for ASM/SSM mixing using O(1) HashSet lookups on committed state,
  // plus a scan of in-flight joins to close the TOCTOU window between the
  // check and the OnJoinedGroup callback that populates
  // asm_groups_/ssm_groups_.
  if (parsed->source.has_value()) {
    if (asm_groups_.Contains(parsed->group)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot join SSM group for which ASM membership already exists");
      return {};
    }
    // Check pending ASM joins for same group.
    for (const auto& entry : join_group_promises_) {
      if (entry.key.group == parsed->group && !entry.key.source.has_value()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "Cannot join SSM group while an ASM join for the same group is "
            "pending");
        return {};
      }
    }
  } else {
    if (ssm_groups_.Contains(parsed->group)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot join ASM group for which SSM membership already exists");
      return {};
    }
    // Check pending SSM joins for same group.
    for (const auto& entry : join_group_promises_) {
      if (entry.key.group == parsed->group && entry.key.source.has_value()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "Cannot join ASM group while an SSM join for the same group is "
            "pending");
        return {};
      }
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  join_group_promises_.insert(key, resolver);

  udp_socket_->get()->JoinGroup(
      parsed->group, parsed->source,
      BindOnce(&MulticastController::OnJoinedGroup, WrapPersistent(this),
               WrapPersistent(resolver), key));

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

  if (!joined_groups_.Contains(key)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot leave group which is not joined");
    return {};
  }

  if (leave_group_promises_.Contains(key)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Already leaving the group");
    return {};
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  leave_group_promises_.insert(key, resolver);

  udp_socket_->get()->LeaveGroup(
      parsed->group, parsed->source,
      BindOnce(&MulticastController::OnLeftGroup, WrapPersistent(this),
               WrapPersistent(resolver), key));

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
  asm_groups_.clear();
  ssm_groups_.clear();

  // Reject all pending promises before clearing the maps. Unresolved
  // ScriptPromiseResolvers must be settled before garbage collection,
  // otherwise a DCHECK fires in ScriptPromiseResolverBase::Dispose().
  for (auto& entry : join_group_promises_) {
    entry.value->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Socket closed."));
  }
  join_group_promises_.clear();
  for (auto& entry : leave_group_promises_) {
    entry.value->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Socket closed."));
  }
  leave_group_promises_.clear();
}

void MulticastController::OnJoinedGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    MembershipKey membership_key,
    int32_t net_error) {
  auto iter = join_group_promises_.find(membership_key);
  if (iter == join_group_promises_.end()) {
    // OnCloseOrAbort() already rejected and cleared this promise.
    return;
  }
  join_group_promises_.erase(iter);

  if (net_error == net::OK) {
    // The joinGroup() call already prevents duplicate joins, so a key that is
    // already present here indicates an unexpected double-callback from the
    // network service.
    DCHECK(!joined_groups_.Contains(membership_key))
        << "OnJoinedGroup: unexpected duplicate join for "
        << membership_key.ToString();
    joined_groups_.insert(membership_key);
    if (membership_key.source.has_value()) {
      // Insert empty set if key doesn't exist, then add source to the set.
      auto result =
          ssm_groups_.insert(membership_key.group, HashSet<net::IPAddress>());
      result.stored_value->value.insert(*membership_key.source);
    } else {
      asm_groups_.insert(membership_key.group);
    }
    probe::DirectUDPSocketJoinedMulticastGroup(
        GetExecutionContext(), inspector_id_, membership_key.ToString());
    resolver->Resolve();
  } else {
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

void MulticastController::OnLeftGroup(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    MembershipKey membership_key,
    int32_t net_error) {
  auto iter = leave_group_promises_.find(membership_key);
  if (iter == leave_group_promises_.end()) {
    // OnCloseOrAbort() already rejected and cleared this promise.
    return;
  }
  leave_group_promises_.erase(iter);

  if (net_error == net::OK) {
    joined_groups_.erase(membership_key);
    if (membership_key.source.has_value()) {
      // Remove source from this group's set. If no sources remain, remove the
      // group entry entirely.
      auto it = ssm_groups_.find(membership_key.group);
      DCHECK(it != ssm_groups_.end());
      it->value.erase(*membership_key.source);
      if (it->value.empty()) {
        ssm_groups_.erase(it);
      }
    } else {
      asm_groups_.erase(membership_key.group);
    }
    probe::DirectUDPSocketLeftMulticastGroup(
        GetExecutionContext(), inspector_id_, membership_key.ToString());
    resolver->Resolve();
  } else {
    resolver->Reject(Socket::CreateDOMExceptionFromNetErrorCode(net_error));
  }
}

HeapVector<Member<V8UnionMulticastMembershipOrString>>
MulticastController::joinedGroups() const {
  HeapVector<Member<V8UnionMulticastMembershipOrString>> result;
  result.ReserveInitialCapacity(joined_groups_.size());

  for (const MembershipKey& key : joined_groups_) {
    const String group_string = String::FromUTF8(key.group.ToString());
    if (!key.source.has_value()) {
      result.push_back(MakeGarbageCollected<V8UnionMulticastMembershipOrString>(
          group_string));
    } else {
      auto* membership = MulticastMembership::Create();
      membership->setGroupAddress(group_string);
      membership->setSourceAddress(String::FromUTF8(key.source->ToString()));

      result.push_back(
          MakeGarbageCollected<V8UnionMulticastMembershipOrString>(membership));
    }
  }

  return result;
}

}  // namespace blink
