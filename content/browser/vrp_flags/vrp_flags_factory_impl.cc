// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vrp_flags/vrp_flags_factory_impl.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "components/vrp_flags/vrp_flags_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {

namespace {

// Observer attached to a newly spawned victim WebContents to bind the target
// VrpFlags Mojo receiver once navigation commits in the primary main frame.
// Managed safely via WebContentsUserData to tie lifetime directly to the
// WebContents without raw 'delete this' statements.
class VictimSpawnObserver : public WebContentsObserver,
                            public WebContentsUserData<VictimSpawnObserver> {
 public:
  ~VictimSpawnObserver() override = default;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    if (navigation_handle->HasCommitted() && receiver_) {
      RenderProcessHost* process =
          navigation_handle->GetRenderFrameHost()->GetProcess();
      process->BindReceiver(std::move(receiver_));
    }
    // Remove user data to clean up this observer after navigation completes.
    GetWebContents().RemoveUserData(UserDataKey());
  }

 private:
  friend class WebContentsUserData<VictimSpawnObserver>;

  VictimSpawnObserver(
      WebContents* contents,
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver)
      : WebContentsObserver(contents),
        WebContentsUserData<VictimSpawnObserver>(*contents),
        receiver_(std::move(receiver)) {}

  mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(VictimSpawnObserver);

}  // namespace

VrpFlagsFactoryImpl::VrpFlagsFactoryImpl() = default;
VrpFlagsFactoryImpl::~VrpFlagsFactoryImpl() = default;

// static
void VrpFlagsFactoryImpl::Bind(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlagsFactory> receiver) {
  static base::NoDestructor<VrpFlagsFactoryImpl> instance;
  GlobalRenderFrameHostId id =
      rfh ? rfh->GetGlobalId() : GlobalRenderFrameHostId();
  instance->receivers_.Add(instance.get(), std::move(receiver), id);
}

void VrpFlagsFactoryImpl::BindBrowserVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  vrp_flags::VrpFlagsImpl::GetInstance()->Bind(std::move(receiver));
}

void VrpFlagsFactoryImpl::BindNetworkVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  GetNetworkService()->GetVrpFlags(base::BindOnce(
      [](mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver,
         mojo::PendingRemote<vrp_flags::mojom::VrpFlags> remote) {
        if (remote) {
          mojo::FusePipes(std::move(receiver), std::move(remote));
        }
      },
      std::move(receiver)));
}

void VrpFlagsFactoryImpl::BindGpuVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  GpuProcessHost* gpu_host = GpuProcessHost::Get();
  if (!gpu_host) {
    receiver.ResetWithReason(0, "GPU Host is not available.");
    return;
  }
  gpu_host->gpu_service()->GetVrpFlags(base::BindOnce(
      [](mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver,
         mojo::PendingRemote<vrp_flags::mojom::VrpFlags> remote) {
        if (remote) {
          mojo::FusePipes(std::move(receiver), std::move(remote));
        }
      },
      std::move(receiver)));
}

void VrpFlagsFactoryImpl::StartRendererForVrpFlags(
    vrp_flags::mojom::VictimDisposition disposition,
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver,
    StartRendererForVrpFlagsCallback callback) {
  // Incrementing port numbers to take advantage of site isolation putting the
  // victim sites in different renderer processes. Note that these aren't real
  // ports with servers running on them, they are just all mapped to a server
  // running on localhost by --host-resolver-rules.
  static uint16_t next_port = 8000;
  uint16_t port = next_port++;
  std::move(callback).Run(port);

  GlobalRenderFrameHostId rfh_id = receivers_.current_context();
  RenderFrameHost* rfh = RenderFrameHost::FromID(rfh_id);
  if (!rfh) {
    LOG(ERROR) << "Failed to start victim renderer for VRP flags: "
                  "RenderFrameHost is null.";
    return;
  }
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    LOG(ERROR) << "Failed to start victim renderer for VRP flags: WebContents "
                  "is null.";
    return;
  }
  WindowOpenDisposition open_disposition =
      WindowOpenDisposition::NEW_FOREGROUND_TAB;
  if (disposition == vrp_flags::mojom::VictimDisposition::kSpawnBackgroundTab) {
    open_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

  GURL target_url(base::StringPrintf("http://victim.test:%u/poc.html", port));
  OpenURLParams params(target_url, Referrer(), open_disposition,
                       ui::PAGE_TRANSITION_LINK,
                       /*is_renderer_initiated=*/false);
  WebContents* new_contents =
      web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
  if (!new_contents) {
    LOG(ERROR)
        << "Failed to start victim renderer for VRP flags: OpenURL failed.";
    return;
  }

  VictimSpawnObserver::CreateForWebContents(new_contents, std::move(receiver));
}

}  // namespace content
