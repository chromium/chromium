// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_

#include <optional>

#include "net/base/ip_address.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_multicastmembership_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MulticastGroupOptions;
// MulticastController interface from multicast_controller.idl
// MulticastController lives as long as UDPSocket, and extends the socket
// lifecycle to be alive while there are pending joinGroup or leaveGroup
// requests, otherwise it would not be possible to resolve returned promises.
// It is not necessary to leave multicast groups. OS will do so automatically on
// socket closure.
class MODULES_EXPORT MulticastController final : public ScriptWrappable,
                                                 public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MulticastController(ExecutionContext*,
                               UDPSocketMojoRemote*,
                               uint64_t inspector_id);

  ~MulticastController() override;

  // multicast_controller.idl:
  ScriptPromise<IDLUndefined> joinGroup(ScriptState*,
                                        const String& ip_address,
                                        const MulticastGroupOptions*,
                                        ExceptionState&);

  // multicast_controller.idl:
  ScriptPromise<IDLUndefined> leaveGroup(ScriptState*,
                                         const String& ip_address,
                                         const MulticastGroupOptions*,
                                         ExceptionState&);

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

  bool HasPendingActivity() const;

  void OnCloseOrAbort();

  // multicast_controller.idl:
  HeapVector<Member<V8UnionMulticastMembershipOrString>> joinedGroups() const;

 private:
  enum class State { kOpen, kClosed };

  struct MembershipKey {
    MembershipKey() = default;
    MembershipKey(const net::IPAddress& group,
                  const std::optional<net::IPAddress>& source)
        : group(group), source(source) {}

    net::IPAddress group;
    std::optional<net::IPAddress> source;

    String ToString() const {
      String group_string = String::FromUtf8(group.ToString());
      if (source.has_value()) {
        return StrCat(
            {group_string, "@", String::FromUtf8(source->ToString())});
      }
      return group_string;
    }

    bool operator==(const MembershipKey& other) const = default;
  };

  // A single multicast membership, committed (kJoined) or in-flight
  // (kJoining). A kJoining entry exists iff join_group_promises_ holds a
  // pending resolver for the same key; such entries participate in duplicate
  // and ASM/SSM-mixing checks but are not reported by joinedGroups().
  struct Membership {
    enum class State { kJoining, kJoined };

    MembershipKey key;
    State state = State::kJoining;
  };

  void OnJoinedGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                     const String& key,
                     int32_t net_error);

  void OnLeftGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                   const String& key,
                   int32_t net_error);

  const Member<UDPSocketMojoRemote> udp_socket_;

  // Maps are keyed by MembershipKey::ToString() ("group" for ASM,
  // "group@source" for SSM), which uses Blink's built-in String hashing.
  HeapHashMap<String, Member<ScriptPromiseResolver<IDLUndefined>>>
      join_group_promises_;
  HeapHashMap<String, Member<ScriptPromiseResolver<IDLUndefined>>>
      leave_group_promises_;

  // All memberships, in-flight and committed. The value carries the
  // structured (group, source) for joinedGroups() and the ASM/SSM mixing
  // check.
  HashMap<String, Membership> memberships_;

  State state_ = State::kOpen;

  // Unique id for devtools inspector_network_agent.
  const uint64_t inspector_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
