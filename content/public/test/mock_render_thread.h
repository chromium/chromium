// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

struct FrameHostMsg_CreateChildFrame_Params;
struct FrameHostMsg_CreateChildFrame_Params_Reply;

namespace IPC {
class MessageFilter;
class MessageReplyDeserializer;
}

namespace blink {
namespace mojom {
enum class TreeScopeType;
}
}

namespace content {

namespace mojom {
class CreateNewWindowParams;
class CreateNewWindowReply;
class RenderMessageFilter;
}

// This class is a very simple mock of RenderThread. It simulates an IPC channel
// which supports only three messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// WidgetMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThread {
 public:
  MockRenderThread();
  ~MockRenderThread() override;

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // RenderThread implementation:
  bool Send(IPC::Message* msg) override;
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  int GenerateRoutingID() override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  void RegisterExtension(std::unique_ptr<v8::Extension> extension) override;
  int PostTaskToAllWebWorkers(base::RepeatingClosure closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  bool IsOnline() override;
  void SetRendererProcessType(
      blink::scheduler::WebRendererProcessType type) override;
  blink::WebString GetUserAgent() override;
  const blink::UserAgentMetadata& GetUserAgentMetadata() override;
  bool IsUseZoomForDSF() override;
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#endif
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;
  void SetUseZoomForDSFEnabled(bool zoom_for_dsf);

  // Returns a new, unique routing ID that can be assigned to the next view,
  // widget, or frame.
  int32_t GetNextRoutingID();

  // Dispatches control messages to observers.
  bool OnControlMessageReceived(const IPC::Message& msg);

  base::ObserverList<RenderThreadObserver>::Unchecked& observers() {
    return observers_;
  }

  // The View expects to be returned a valid |reply.route_id| different from its
  // own. We do not keep track of the newly created widget in MockRenderThread,
  // so it must be cleaned up on its own.
  void OnCreateWindow(const mojom::CreateNewWindowParams& params,
                      mojom::CreateNewWindowReply* reply);

  // Returns the receiver end of the InterfaceProvider interface whose client
  // end was passed in to construct RenderFrame with |routing_id|; if any. The
  // client end will be used by the RenderFrame to service interface receivers
  // originating from the initial empty document.
  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
  TakeInitialInterfaceProviderRequestForFrame(int32_t routing_id);

  // Returns the receiver end of the BrowserInterfaceBroker interface whose
  // client end was passed in to construct RenderFrame with |routing_id|; if
  // any. The client end will be used by the RenderFrame to service interface
  // requests originating from the initial empty document.
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeInitialBrowserInterfaceBrokerReceiverForFrame(int32_t routing_id);

  // Called from the RenderViewTest harness to supply the receiver end of the
  // InterfaceProvider interface connection that the harness used to service the
  // initial empty document in the RenderFrame with |routing_id|.
  void PassInitialInterfaceProviderReceiverForFrame(
      int32_t routing_id,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver);

 protected:
  // This function operates as a regular IPC listener. Subclasses
  // overriding this should first delegate to this implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // The Frame expects to be returned a valid route_id different from its own.
  void OnCreateChildFrame(
      const FrameHostMsg_CreateChildFrame_Params& params,
      FrameHostMsg_CreateChildFrame_Params_Reply* params_reply);

  IPC::TestSink sink_;

  // Routing ID what will be assigned to the next view, widget, or frame.
  int32_t next_routing_id_;

  std::map<int32_t,
           mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>>
      frame_routing_id_to_initial_interface_provider_receivers_;

  std::map<int32_t, mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>>
      frame_routing_id_to_initial_browser_broker_receivers_;

  // The last known good deserializer for sync messages.
  std::unique_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A list of message filters added to this thread.
  std::vector<scoped_refptr<IPC::MessageFilter> > filters_;

  // Observers to notify.
  base::ObserverList<RenderThreadObserver>::Unchecked observers_;

  std::unique_ptr<mojom::RenderMessageFilter> mock_render_message_filter_;
  bool zoom_for_dsf_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
