// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/common/widget_type.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {

namespace mojom {
class CreateNewWindowParams;
class CreateNewWindowReply;
class Frame;
}

// This class is a very simple mock of RenderThread. It simulates an IPC channel
// which supports the following message:
// FrameHostMsg_CreateChildFrame : sync message sent by the renderer.
class MockRenderThread : public RenderThread {
 public:
  MockRenderThread();
  ~MockRenderThread() override;

  void SetIOTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    io_task_runner_ = std::move(task_runner);
  }

  // RenderThread implementation:
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;
  bool GenerateFrameRoutingID(int32_t& routing_id,
                              blink::LocalFrameToken& frame_token,
                              base::UnguessableToken& devtools_frame_token,
                              blink::DocumentToken& document_token) override;

  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  int PostTaskToAllWebWorkers(base::RepeatingClosure closure) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  blink::WebString GetUserAgent() override;
  const blink::UserAgentMetadata& GetUserAgentMetadata() override;
#if BUILDFLAG(IS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#endif
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::RenderProcessHost> proto)
      override;

  // Returns a new, unique routing ID that can be assigned to the next view,
  // widget, or frame.
  int32_t GetNextRoutingID();

  base::ObserverList<RenderThreadObserver>::Unchecked& observers() {
    return observers_;
  }

  // The View expects to be returned a valid |reply.route_id| different from its
  // own. We do not keep track of the newly created widget in MockRenderThread,
  // so it must be cleaned up on its own.
  void OnCreateWindow(mojom::CreateNewWindowParams& params,
                      mojom::CreateNewWindowReply* reply);

  // Releases any `blink::WebView`s that are being held onto by PageBroadcast
  // associated remotes.
  void ReleaseAllWebViews();

  void OnCreateChildFrame(
      const blink::LocalFrameToken& frame_token,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker);

  // Returns the receiver end of the BrowserInterfaceBroker interface whose
  // client end was passed in to construct RenderFrame with `frame_token`; if
  // any. The client end will be used by the RenderFrame to service interface
  // requests originating from the initial empty document.
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeInitialBrowserInterfaceBrokerReceiverForFrame(
      const blink::LocalFrameToken& frame_token);

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Routing ID what will be assigned to the next view, widget, or frame.
  int32_t next_routing_id_;

  // Pending BrowserInterfaceBrokers sent from the renderer when creating a
  // new Frame and informing the browser.
  std::map<blink::LocalFrameToken,
           mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>>
      frame_token_to_initial_browser_brokers_;

  // `blink::WebView`s associated with CreateNewWindow have their
  // lifecycle associated with the mojo channel provided to them.
  std::vector<mojo::AssociatedRemote<blink::mojom::PageBroadcast>>
      page_broadcasts_;

  // Observers to notify.
  base::ObserverList<RenderThreadObserver>::Unchecked observers_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
