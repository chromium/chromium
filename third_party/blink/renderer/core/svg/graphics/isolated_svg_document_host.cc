/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/graphics/isolated_svg_document_host.h"

#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"

namespace blink {

// IsolatedSVGDocumentHost::LocalFrameClient is used to wait until the SVG
// document's load event is fired in the case where there are subresources
// asynchronously loaded.
class IsolatedSVGDocumentHost::LocalFrameClient : public EmptyLocalFrameClient {
 public:
  explicit LocalFrameClient(IsolatedSVGDocumentHost* host) : host_(host) {}

  void ClearHost() { host_ = nullptr; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(host_);
    EmptyLocalFrameClient::Trace(visitor);
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    // SVG Images have unique security rules that prevent all subresource
    // requests except for data urls.
    return base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
        WTF::BindOnce(
            [](const network::ResourceRequest& resource_request,
               mojo::PendingReceiver<network::mojom::URLLoader> receiver,
               mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
              NOTREACHED_IN_MIGRATION();
            }));
  }

  void DispatchDidHandleOnloadEvents() override {
    if (host_) {
      host_->LoadCompleted();
    }
  }

  Member<IsolatedSVGDocumentHost> host_;
};

IsolatedSVGDocumentHost::IsolatedSVGDocumentHost(
    IsolatedSVGChromeClient& chrome_client,
    AgentGroupScheduler& agent_group_scheduler) {
  TRACE_EVENT("blink", "IsolatedSVGDocumentHost::IsolatedSVGDocumentHost");

  // The isolated document will fire events (and the default C++ handlers run)
  // but doesn't actually allow scripts to run so it's fine to call into it. We
  // allow this since it means an SVG data url can synchronously load like other
  // image types.
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_user_agent_events;

  CHECK_EQ(load_state_, kNotStarted);
  load_state_ = kPending;

  Page* page;
  {
    TRACE_EVENT("blink",
                "IsolatedSVGDocumentHost::IsolatedSVGDocumentHost::createPage");
    page = Page::CreateNonOrdinary(chrome_client, agent_group_scheduler,
                                   /*color_provider_colors=*/nullptr);

    Settings& settings = page->GetSettings();
    settings.SetScriptEnabled(false);
    settings.SetPluginsEnabled(false);
  }

  LocalFrame* frame = nullptr;
  {
    TRACE_EVENT(
        "blink",
        "IsolatedSVGDocumentHost::IsolatedSVGDocumentHost::createFrame");
    frame_client_ = MakeGarbageCollected<LocalFrameClient>(this);
    frame = MakeGarbageCollected<LocalFrame>(
        frame_client_, *page, nullptr, nullptr, nullptr,
        FrameInsertType::kInsertInConstructor, LocalFrameToken(), nullptr,
        nullptr, mojo::NullRemote());
    frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));
    frame->Init(/*opener=*/nullptr, DocumentToken(),
                /*policy_container=*/nullptr, StorageKey(),
                /*document_ukm_source_id=*/ukm::kInvalidSourceId,
                /*creator_base_url=*/KURL());
  }

  // SVG Images will always synthesize a viewBox, if it's not available, and
  // thus never see scrollbars.
  frame->View()->SetCanHaveScrollbars(false);
  // SVG Images are transparent.
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  // Set up our Page reference after installing our document. This avoids
  // tripping on a non-existing (null) Document if a GC is triggered during the
  // set up and ends up collecting the last owner/observer of this image.
  page_ = page;
}

void IsolatedSVGDocumentHost::InstallDocument(
    scoped_refptr<const SharedBuffer> data,
    base::OnceClosure async_load_callback,
    const Settings* inherited_settings,
    ProcessingMode processing_mode) {
  TRACE_EVENT("blink", "IsolatedSVGDocumentHost::InstallDocument");

  // The isolated document will fire events (and the default C++ handlers run)
  // but doesn't actually allow scripts to run so it's fine to call into it. We
  // allow this since it means an SVG data url can synchronously load like other
  // image types.
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_user_agent_events;

  async_load_callback_ = std::move(async_load_callback);

  Settings& settings = page_->GetSettings();

  if (inherited_settings) {
    CopySettingsFrom(settings, *inherited_settings);
  }

  // If "secure static mode" is requested, set the animation policy to "no
  // animation". This will disable SMIL and image animations.
  if (processing_mode == ProcessingMode::kStatic) {
    settings.SetImageAnimationPolicy(
        mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyNoAnimation);
  }
  CHECK(data);
  LocalFrame* frame = GetFrame();
  frame->ForceSynchronousDocumentInstall(AtomicString("image/svg+xml"), *data);

  // Intrinsic sizing relies on computed style (e.g. font-size and
  // writing-mode).
  frame->GetDocument()->UpdateStyleAndLayoutTree();

  switch (load_state_) {
    case kPending:
      load_state_ = kWaitingForAsyncLoadCompletion;
      break;
    case kCompleted:
      break;
    case kNotStarted:
    case kWaitingForAsyncLoadCompletion:
      CHECK(false);
      break;
  }
}

void IsolatedSVGDocumentHost::CopySettingsFrom(
    Settings& settings,
    const Settings& inherited_settings) {
  settings.GetGenericFontFamilySettings() =
      inherited_settings.GetGenericFontFamilySettings();
  settings.SetMinimumFontSize(inherited_settings.GetMinimumFontSize());
  settings.SetMinimumLogicalFontSize(
      inherited_settings.GetMinimumLogicalFontSize());
  settings.SetDefaultFontSize(inherited_settings.GetDefaultFontSize());
  settings.SetDefaultFixedFontSize(
      inherited_settings.GetDefaultFixedFontSize());

  settings.SetImageAnimationPolicy(
      inherited_settings.GetImageAnimationPolicy());
  settings.SetPrefersReducedMotion(
      inherited_settings.GetPrefersReducedMotion());

  // Also copy the preferred-color-scheme to ensure a responsiveness to
  // dark/light color schemes.
  settings.SetPreferredColorScheme(
      inherited_settings.GetPreferredColorScheme());
  settings.SetInForcedColors(inherited_settings.GetInForcedColors());
}

LocalFrame* IsolatedSVGDocumentHost::GetFrame() {
  return To<LocalFrame>(page_->MainFrame());
}

SVGSVGElement* IsolatedSVGDocumentHost::RootElement() {
  return DynamicTo<SVGSVGElement>(GetFrame()->GetDocument()->documentElement());
}

void IsolatedSVGDocumentHost::LoadCompleted() {
  switch (load_state_) {
    case kPending:
      load_state_ = kCompleted;
      break;

    case kWaitingForAsyncLoadCompletion:
      load_state_ = kCompleted;

      // Because LoadCompleted() is called synchronously from
      // Document::ImplicitClose(), we defer AsyncLoadCompleted() to avoid
      // potential bugs and timing dependencies around ImplicitClose() and
      // to make LoadEventFinished() true when AsyncLoadCompleted() is called.
      async_load_task_handle_ = PostCancellableTask(
          *GetFrame()->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
          std::move(async_load_callback_));
      break;

    case kNotStarted:
    case kCompleted:
      CHECK(false);
      break;
  }
}

void IsolatedSVGDocumentHost::Shutdown() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;

  // The constructor initializes `page_` and we tear it down here. Shutdown()
  // shouldn't be called twice. Ditto for `frame_client_`.
  DCHECK(page_);
  DCHECK(frame_client_);

  // Sever the link from the frame client back to us to prevent any pending
  // loads from completing.
  frame_client_->ClearHost();

  // Cancel any in-flight async load task.
  async_load_task_handle_.Cancel();

  // It is safe to allow UA events within this scope, because event
  // dispatching inside the isolated document doesn't trigger JavaScript
  // execution. All script execution is forbidden when an SVG is loaded as an
  // image subresource - see SetScriptEnabled in IsolatedSVGDocumentHost().
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
  Page* current_page = page_.Release();
  // Break both the loader and view references to the frame.
  current_page->WillBeDestroyed();
}

IsolatedSVGDocumentHost::~IsolatedSVGDocumentHost() {
  DCHECK(!page_);  // Expecting explicit shutdown.
}

void IsolatedSVGDocumentHost::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(frame_client_);
}

// static
IsolatedSVGDocumentHostInitializer* IsolatedSVGDocumentHostInitializer::Get() {
  DEFINE_STATIC_LOCAL(
      Persistent<IsolatedSVGDocumentHostInitializer>, initializer,
      (MakeGarbageCollected<IsolatedSVGDocumentHostInitializer>()));
  return initializer;
}

IsolatedSVGDocumentHostInitializer::IsolatedSVGDocumentHostInitializer()
    : max_pre_initialize_count_(
          base::checked_cast<wtf_size_t>(
              !base::SysInfo::IsLowEndDevice() &&
              base::FeatureList::IsEnabled(
                  features::kPreInitializePageAndFrameForSVGImage))
              ? features::kMaxCountOfPreInitializePageAndFrameForSVGImage.Get()
              : 0),
      agent_group_scheduler_(Thread::MainThread()
                                 ->Scheduler()
                                 ->ToMainThreadScheduler()
                                 ->CreateAgentGroupScheduler()),
      task_runner_(agent_group_scheduler_->DefaultTaskRunner()) {}

std::pair<SVGImageChromeClient*, IsolatedSVGDocumentHost*>
IsolatedSVGDocumentHostInitializer::GetOrCreate() {
  if (max_pre_initialize_count_ > 0 &&
      !chrome_clients_and_document_hosts_.empty()) {
    TRACE_EVENT("blink", "IsolatedSVGDocumentHostInitializer::GetOrCreate",
                "return_from_cache", true);
    auto [chrome_client, document_host] =
        chrome_clients_and_document_hosts_.back();
    chrome_clients_and_document_hosts_.pop_back();
    return {chrome_client, document_host};
  }

  TRACE_EVENT("blink", "IsolatedSVGDocumentHostInitializer::GetOrCreate",
              "return_from_cache", false);
  return Create();
}

void IsolatedSVGDocumentHostInitializer::MaybePrepareIsolatedSVGDocumentHost() {
  paused_ = false;

  if (chrome_clients_and_document_hosts_.size() >= max_pre_initialize_count_) {
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(
          [](IsolatedSVGDocumentHostInitializer* initializer) {
            if (initializer->paused_) {
              return;
            }
            if (initializer->chrome_clients_and_document_hosts_.size() >=
                initializer->max_pre_initialize_count_) {
              return;
            }
            TRACE_EVENT("blink",
                        "IsolatedSVGDocumentHostInitializer::"
                        "MaybePrepareIsolatedSVGDocumentHost");
            initializer->chrome_clients_and_document_hosts_.push_back(
                initializer->Create());
            initializer->MaybePrepareIsolatedSVGDocumentHost();
          },
          WrapWeakPersistent(this)));
}

std::pair<SVGImageChromeClient*, IsolatedSVGDocumentHost*>
IsolatedSVGDocumentHostInitializer::Create() {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES(
      "Blink.SVGImage.IsolatedSVGDocumentHostCreationTime");

  // SVG will be shared via MemoryCache (which is renderer process
  // global cache) across multiple AgentSchedulingGroups. That's
  // why we can't use an existing AgentSchedulingGroup for now. If
  // we incorrectly use the existing ASG/AGS and if we freeze task
  // queues on a AGS, it will affect SVGs on other AGS. To
  // mitigate this problem, we need to split the MemoryCache into
  // smaller granularity. There is an active effort to mitigate
  // this which is called "Memory Cache Per Context"
  // (https://crbug.com/1127971).
  AgentGroupScheduler* agent_group_scheduler =
      Thread::MainThread()
          ->Scheduler()
          ->ToMainThreadScheduler()
          ->CreateAgentGroupScheduler();

  SVGImageChromeClient* chrome_client =
      MakeGarbageCollected<SVGImageChromeClient>();

  chrome_client->InitAnimationTimer(
      agent_group_scheduler->CompositorTaskRunner());

  IsolatedSVGDocumentHost* isolated_svg_document_host =
      MakeGarbageCollected<IsolatedSVGDocumentHost>(*chrome_client,
                                                    *agent_group_scheduler);

  return {chrome_client, isolated_svg_document_host};
}

void IsolatedSVGDocumentHostInitializer::Clear() {
  // Pause creation so that it doesn't continue creating.
  paused_ = true;
  for (auto [svg_image_chrome_client, isolated_svg_document_host] :
       chrome_clients_and_document_hosts_) {
    isolated_svg_document_host->Shutdown();
  }
  chrome_clients_and_document_hosts_.clear();
}

void IsolatedSVGDocumentHostInitializer::Trace(Visitor* visitor) const {
  visitor->Trace(chrome_clients_and_document_hosts_);
  visitor->Trace(agent_group_scheduler_);
}

}  // namespace blink
