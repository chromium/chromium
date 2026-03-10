// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_

#include <optional>

#include "base/hash/hash.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_multicast_membership.h"
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
#include "third_party/blink/renderer/platform/network/ip_address.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

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

  ScriptPromise<IDLUndefined> joinGroup(ScriptState*,
                                        const String& ip_address,
                                        const MulticastGroupOptions*,
                                        ExceptionState&);

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
    // Helper flag for HashTraits to mark deleted keys without invalidating IP.
    bool is_deleted = false;

    String ToString() const {
      String group_string = String::FromUTF8(group.ToString());
      if (source.has_value()) {
        return group_string + "@" + String::FromUTF8(source->ToString());
      }
      return group_string;
    }

    bool operator==(const MembershipKey& other) const = default;
  };

  struct MembershipKeyHash : public GenericHashTraits<MembershipKey> {
    static unsigned GetHash(const MembershipKey& key) {
      size_t hash = base::FastHash(key.group.bytes());
      if (key.source.has_value()) {
        size_t source_hash = base::FastHash(key.source->bytes());
        if constexpr (sizeof(size_t) == sizeof(uint32_t)) {
          hash = base::HashInts32(hash, source_hash);
        } else {
          hash = base::HashInts64(hash, source_hash);
        }
      }
      return static_cast<unsigned>(hash);
    }
    static bool Equal(const MembershipKey& a, const MembershipKey& b) {
      return a == b;
    }
    static bool IsEmptyValue(const MembershipKey& key) {
      // An empty MembershipKey has a default-constructed (invalid) group
      // address. The !is_deleted guard is required to distinguish an empty
      // slot from a deleted slot: ConstructDeletedValue() sets is_deleted=true
      // but leaves group empty, so both conditions are needed.
      return key.group.empty() && !key.is_deleted;
    }
    static bool IsDeletedValue(const MembershipKey& key) {
      return key.is_deleted;
    }
    static void ConstructDeletedValue(MembershipKey& slot) {
      slot.is_deleted = true;
    }
    static MembershipKey EmptyValue() { return MembershipKey(); }
    // Equal() is called by the hash table on slots that may be empty or
    // deleted. This is safe because operator== includes is_deleted, so a
    // deleted key (is_deleted=true) never compares equal to a live key
    // (is_deleted=false), even if both have an empty group address.
    static constexpr bool kSafeToCompareToEmptyOrDeleted = true;
  };

  void OnJoinedGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                     MembershipKey membership_key,
                     int32_t net_error);

  void OnLeftGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                   MembershipKey membership_key,
                   int32_t net_error);

  const Member<UDPSocketMojoRemote> udp_socket_;

  HeapHashMap<MembershipKey,
              Member<ScriptPromiseResolver<IDLUndefined>>,
              MembershipKeyHash>
      join_group_promises_;
  HeapHashMap<MembershipKey,
              Member<ScriptPromiseResolver<IDLUndefined>>,
              MembershipKeyHash>
      leave_group_promises_;

  HashSet<MembershipKey, MembershipKeyHash> joined_groups_;

  // Track which groups have ASM vs SSM memberships for O(1) mixing check.
  // For SSM, we map group address -> set of source addresses to allow O(1)
  // lookup when leaving a group (to check if other sources remain).
  HashSet<net::IPAddress> asm_groups_;
  HashMap<net::IPAddress, HashSet<net::IPAddress>> ssm_groups_;

  State state_ = State::kOpen;

  // Unique id for devtools inspector_network_agent.
  const uint64_t inspector_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
