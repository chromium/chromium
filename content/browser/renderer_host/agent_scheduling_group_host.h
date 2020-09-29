// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include <stdint.h>

#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"

namespace IPC {
class ChannelProxy;
class Listener;
class Message;
}  // namespace IPC

namespace content {

class RenderProcessHost;
class SiteInstance;

// Browser-side host of an AgentSchedulingGroup, used for
// AgentSchedulingGroup-bound messaging. AgentSchedulingGroup is Blink's unit of
// scheduling and performance isolation, which is the only way to obtain
// ordering guarantees between different Mojo (associated) interfaces and legacy
// IPC messages.
//
// An AgentSchedulingGroupHost is stored as (and owned by) UserData on the
// RenderProcessHost.
class CONTENT_EXPORT AgentSchedulingGroupHost
    : public RenderProcessHostObserver,
      public mojom::AgentSchedulingGroupHost,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider {
 public:
  // Get the appropriate AgentSchedulingGroupHost for the given |instance| and
  // |process|. For now, each RenderProcessHost has a single
  // AgentSchedulingGroupHost, though future policies will allow multiple groups
  // in a process.
  static AgentSchedulingGroupHost* Get(const SiteInstance& instance,
                                       RenderProcessHost& process);

  // Utility ctor, forwarding to the main ctor below.
  // Should not be called explicitly. Use `Get()` instead.
  explicit AgentSchedulingGroupHost(RenderProcessHost& process);
  ~AgentSchedulingGroupHost() override;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  RenderProcessHost* GetProcess();
  // Ensure that the process this AgentSchedulingGroupHost belongs to is alive,
  // that the renderer-side AgentSchedulingGroup exists, and that the
  // mojom::AgentSchedulingGroup/Host interfaces are bound and connected.
  // Returns |false| if any part of the initialization failed.
  bool InitProcessAndMojos();

  // IPC and mojo messages to be forwarded to the RenderProcessHost, for now. In
  // the future they will be handled directly by the AgentSchedulingGroupHost.
  // IPC:
  IPC::ChannelProxy* GetChannel();
  bool Send(IPC::Message* message);
  void AddRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveRoute(int32_t routing_id);

  // Mojo:
  mojom::RouteProvider* GetRemoteRouteProvider();
  void CreateFrame(mojom::CreateFrameParamsPtr params);
  void CreateView(mojom::CreateViewParamsPtr params);
  void DestroyView(int32_t routing_id);
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token);

 private:
  // `MaybeAssociatedReceiver` and `MaybeAssociatedRemote` are temporary helper
  // classes that allow us to switch between using associated and non-associated
  // mojo interfaces. This behavior is controlled by the
  // `kMbiDetachAgentSchedulingGroupFromChannel` feature flag.
  // Associated interfaces are associated with the IPC channel (transitively,
  // via the `Renderer` interface), thus preserving cross-agent scheduling group
  // message order. Non-associated interfaces are independent from each other
  // and do not preserve message order between agent scheduling groups.
  // TODO(crbug.com/1111231): Remove these once we can remove the flag.
  class MaybeAssociatedReceiver {
   public:
    MaybeAssociatedReceiver(AgentSchedulingGroupHost& host,
                            bool should_associate);
    ~MaybeAssociatedReceiver();

    mojo::PendingRemote<mojom::AgentSchedulingGroupHost>
    BindNewPipeAndPassRemote() WARN_UNUSED_RESULT;
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
    BindNewEndpointAndPassRemote() WARN_UNUSED_RESULT;

    void reset();
    bool is_bound();

   private:
    // This will hold the actual receiver pointed to by |receiver_|.
    absl::variant<
        // This is required to make the variant default constructible. After the
        // ctor body finishes, the variant will never hold this alternative.
        absl::monostate,
        mojo::Receiver<mojom::AgentSchedulingGroupHost>,
        mojo::AssociatedReceiver<mojom::AgentSchedulingGroupHost>>
        receiver_or_monostate_;

    // View of |receiver_or_monostate_| that "strips out" the `monostate`,
    // allowing for easier handling of the underlying remote. Should be declared
    // after |receiver_or_monostate_| so that it is destroyed first.
    absl::variant<mojo::Receiver<mojom::AgentSchedulingGroupHost>*,
                  mojo::AssociatedReceiver<mojom::AgentSchedulingGroupHost>*>
        receiver_;
  };

  class MaybeAssociatedRemote {
   public:
    explicit MaybeAssociatedRemote(bool should_associate);
    ~MaybeAssociatedRemote();

    mojo::PendingReceiver<mojom::AgentSchedulingGroup>
    BindNewPipeAndPassReceiver() WARN_UNUSED_RESULT;
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
    BindNewEndpointAndPassReceiver() WARN_UNUSED_RESULT;

    void reset();
    bool is_bound();
    mojom::AgentSchedulingGroup* get();

   private:
    absl::variant<mojo::Remote<mojom::AgentSchedulingGroup>,
                  mojo::AssociatedRemote<mojom::AgentSchedulingGroup>>
        remote_;
  };

  // Main constructor.
  // |should_associate| determines whether the `AgentSchedulingGroupHost` and
  // `AgentSchedulingGroup` mojos should be associated with the `Renderer` or
  // not. If they are, message order will be preserved across the entire
  // process. If not, ordering will only be preserved inside an
  // `AgentSchedulingGroup`.
  AgentSchedulingGroupHost(RenderProcessHost& process, bool should_associate);

  // mojom::RouteProvider
  void GetRoute(
      int32_t routing_id,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          receiver) override;

  // blink::mojom::AssociatedInterfaceProvider
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

  void ResetMojo();
  void SetUpMojoIfNeeded();

  // The RenderProcessHost this AgentSchedulingGroup is assigned to.
  RenderProcessHost& process_;

  const bool should_associate_;

  // Implementation of `mojom::AgentSchedulingGroupHost`, used for responding to
  // calls from the (renderer-side) `AgentSchedulingGroup`.
  MaybeAssociatedReceiver receiver_;

  // Remote stub of `mojom::AgentSchedulingGroup`, used for sending calls to the
  // (renderer-side) `AgentSchedulingGroup`.
  MaybeAssociatedRemote mojo_remote_;
};

}  // namespace content

#endif
