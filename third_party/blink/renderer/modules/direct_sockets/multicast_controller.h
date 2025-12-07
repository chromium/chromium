// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
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
                                        ExceptionState&);

  // multicast_controller.idl:
  ScriptPromise<IDLUndefined> leaveGroup(ScriptState*,
                                         const String& ip_address,
                                         ExceptionState&);

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

  bool HasPendingActivity() const;

  void OnCloseOrAbort();

  // multicast_controller.idl:
  const Vector<String>& joinedGroups() const { return joined_groups_; }

 private:
  enum class State { kOpen, kClosed };

  void OnJoinedGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                     String normalized_ip_address,
                     int32_t net_error);

  void OnLeftGroup(ScriptPromiseResolver<IDLUndefined>* resolver,
                   String normalized_ip_address,
                   int32_t net_error);

  const Member<UDPSocketMojoRemote> udp_socket_;

  HeapHashMap<String, Member<ScriptPromiseResolver<IDLUndefined>>>
      join_group_promises_;
  HeapHashMap<String, Member<ScriptPromiseResolver<IDLUndefined>>>
      leave_group_promises_;

  Vector<String> joined_groups_;

  State state_ = State::kOpen;

  // Unique id for devtools inspector_network_agent.
  const uint64_t inspector_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_MULTICAST_CONTROLLER_H_
