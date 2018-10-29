// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

#if defined(OS_MACOSX)
#include "mojo/public/cpp/system/buffer.h"
#endif

struct FrameHostMsg_CreateChildFrame_Params;

namespace IPC {
class MessageFilter;
class MessageReplyDeserializer;
}

namespace blink {
enum class WebSandboxFlags;
enum class WebTreeScopeType;
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
  std::unique_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) override;
  void RegisterExtension(v8::Extension* extension) override;
  int PostTaskToAllWebWorkers(const base::Closure& closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  bool IsOnline() override;
  void SetRendererProcessType(
      blink::scheduler::RendererProcessType type) override;
  blink::WebString GetUserAgent() const override;
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#elif defined(OS_MACOSX)
  bool LoadFont(const base::string16& font_name,
                float font_point_size,
                mojo::ScopedSharedBufferHandle* out_font_data,
                uint32_t* out_font_id) override;
#endif
  ServiceManagerConnection* GetServiceManagerConnection() override;
  service_manager::Connector* GetConnector() override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

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

  // The Widget expects to be returned a valid route_id.
  void OnCreateWidget(int opener_id, int* route_id);

  // Returns the request end of the InterfaceProvider interface whose client end
  // was passed in to construct RenderFrame with |routing_id|; if any. The
  // client end will be used by the RenderFrame to service interface requests
  // originating from the original the initial empty document.
  service_manager::mojom::InterfaceProviderRequest
  TakeInitialInterfaceProviderRequestForFrame(int32_t routing_id);

  // Called from the RenderViewTest harness to supply the request end of the
  // InterfaceProvider interface connection that the harness used to service the
  // initial empty document in the RenderFrame with |routing_id|.
  void PassInitialInterfaceProviderRequestForFrame(
      int32_t routing_id,
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request);

 protected:
  // This function operates as a regular IPC listener. Subclasses
  // overriding this should first delegate to this implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // The Frame expects to be returned a valid route_id different from its own.
  void OnCreateChildFrame(const FrameHostMsg_CreateChildFrame_Params& params,
                          int* new_render_frame_id,
                          mojo::MessagePipeHandle* new_interface_provider,
                          base::UnguessableToken* devtools_frame_token);

#if defined(OS_WIN)
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

  IPC::TestSink sink_;

  // Routing ID what will be assigned to the next view, widget, or frame.
  int32_t next_routing_id_;

  std::map<int32_t, service_manager::mojom::InterfaceProviderRequest>
      frame_routing_id_to_initial_interface_provider_requests_;

  // The last known good deserializer for sync messages.
  std::unique_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A list of message filters added to this thread.
  std::vector<scoped_refptr<IPC::MessageFilter> > filters_;

  // Observers to notify.
  base::ObserverList<RenderThreadObserver>::Unchecked observers_;

  std::unique_ptr<service_manager::Connector> connector_;
  service_manager::mojom::ConnectorRequest pending_connector_request_;

  std::unique_ptr<mojom::RenderMessageFilter> mock_render_message_filter_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
