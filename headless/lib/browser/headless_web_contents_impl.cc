// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_web_contents_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "headless/lib/browser/directory_enumerator.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/protocol/headless_handler.h"
#include "headless/public/switches.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/headless/headless_print_manager.h"
#endif

namespace headless {

namespace {

void UpdatePrefsFromSystemSettings(blink::RendererPreferences* prefs) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
#endif

  // The values were copied from chrome/browser/renderer_preferences_util.cc.
#if BUILDFLAG(IS_MAC)
  prefs->focus_ring_color = SkColorSetRGB(0x00, 0x5F, 0xCC);
#else
  prefs->focus_ring_color = SkColorSetRGB(0x10, 0x10, 0x10);
#endif
}

}  // namespace

// static
HeadlessWebContentsImpl* HeadlessWebContentsImpl::From(
    HeadlessWebContents* web_contents) {
  // This downcast is safe because there is only one implementation of
  // HeadlessWebContents.
  return static_cast<HeadlessWebContentsImpl*>(web_contents);
}

// static
HeadlessWebContentsImpl* HeadlessWebContentsImpl::From(
    HeadlessBrowser* browser,
    content::WebContents* contents) {
  return HeadlessWebContentsImpl::From(
      browser->GetWebContentsForDevToolsAgentHostId(
          content::DevToolsAgentHost::GetOrCreateFor(contents)->GetId()));
}

class HeadlessWebContentsImpl::Delegate : public content::WebContentsDelegate {
 public:
  explicit Delegate(HeadlessWebContentsImpl* headless_web_contents)
      : headless_web_contents_(headless_web_contents) {}

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  void BeforeUnloadFired(content::WebContents* web_contents,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    *proceed_to_fire_unload = proceed;
  }

  void ActivateContents(content::WebContents* contents) override {
    contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget()->Focus();
  }

  void CloseContents(content::WebContents* source) override {
    auto* const headless_contents =
        HeadlessWebContentsImpl::From(browser(), source);
    DCHECK(headless_contents);
    headless_contents->Close();
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    DCHECK(new_contents->GetBrowserContext() ==
           headless_web_contents_->browser_context());

    std::unique_ptr<HeadlessWebContentsImpl> child_contents =
        HeadlessWebContentsImpl::CreateForChildContents(
            headless_web_contents_, std::move(new_contents));
    HeadlessWebContentsImpl* raw_child_contents = child_contents.get();
    headless_web_contents_->browser_context()->RegisterWebContents(
        std::move(child_contents));

    const gfx::Rect default_rect(
        headless_web_contents_->browser()->options()->window_size);
    const gfx::Rect rect = window_features.bounds.IsEmpty()
                               ? default_rect
                               : window_features.bounds;
    raw_child_contents->SetBounds(rect);
    return nullptr;
  }

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    DCHECK_EQ(source, headless_web_contents_->web_contents());
    content::WebContents* target = nullptr;
    switch (params.disposition) {
      case WindowOpenDisposition::CURRENT_TAB:
        target = source;
        break;

      case WindowOpenDisposition::NEW_POPUP:
      case WindowOpenDisposition::NEW_WINDOW:
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
        HeadlessWebContentsImpl* child_contents = HeadlessWebContentsImpl::From(
            headless_web_contents_->browser_context()
                ->CreateWebContentsBuilder()
                .SetWindowSize(source->GetContainerBounds().size())
                .Build());
        target = child_contents->web_contents();
        break;
      }

      // TODO(veluca): add support for other disposition types.
      case WindowOpenDisposition::SINGLETON_TAB:
      case WindowOpenDisposition::OFF_THE_RECORD:
      case WindowOpenDisposition::SAVE_TO_DISK:
      case WindowOpenDisposition::IGNORE_ACTION:
      default:
        return nullptr;
    }

    content::NavigationController::LoadURLParams load_url_params(params);
    load_url_params.force_new_browsing_instance =
        headless_web_contents_->browser()
            ->options()
            ->force_new_browsing_instance;

    base::WeakPtr<content::NavigationHandle> navigation =
        target->GetController().LoadURLWithParams(load_url_params);
    if (navigation_handle_callback && navigation) {
      std::move(navigation_handle_callback).Run(*navigation);
    }
    return target;
  }

  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override {
    return headless_web_contents_->browser_context()
        ->options()
        ->block_new_web_contents();
  }

  void EnumerateDirectory(content::WebContents* web_contents,
                          scoped_refptr<content::FileSelectListener> listener,
                          const base::FilePath& path) override {
    DirectoryEnumerator::Start(path, std::move(listener));
  }

  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override {
    return content::PictureInPictureResult::kSuccess;
  }

  bool IsBackForwardCacheSupported(
      content::WebContents& web_contents) override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    return command_line->HasSwitch(switches::kEnableBackForwardCache);
  }

  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    web_contents->GotResponseToPointerLockRequest(
        blink::mojom::PointerLockResult::kSuccess);
  }

  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    SetFullscreenModeForTab(
        content::WebContents::FromRenderFrameHost(requesting_frame),
        /*fullscreen=*/true);
  }

  void ExitFullscreenModeForTab(content::WebContents* web_contents) override {
    SetFullscreenModeForTab(web_contents, /*fullscreen=*/false);
  }

  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override {
    return is_fullscreen_;
  }

 private:
  HeadlessBrowserImpl* browser() { return headless_web_contents_->browser(); }

  void SetFullscreenModeForTab(content::WebContents* web_contents,
                               bool fullscreen) {
    if (is_fullscreen_ == fullscreen) {
      return;
    }

    is_fullscreen_ = fullscreen;

    content::RenderViewHost* rvh =
        web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
    CHECK(rvh);

    content::RenderWidgetHostView* view = rvh->GetWidget()->GetView();
    if (view) {
      if (fullscreen) {
        // Headless chrome does not have screen to set the view bounds to, so
        // just double the size of the existing view to trigger the expected
        // window size change notifications.
        before_fullscreen_bounds_ = view->GetViewBounds();
        gfx::Rect bounds = before_fullscreen_bounds_;
        bounds.set_width(bounds.width() * 2);
        bounds.set_height(bounds.height() * 2);
        view->SetBounds(bounds);
      } else {
        view->SetBounds(before_fullscreen_bounds_);
      }
    }

    rvh->GetWidget()->SynchronizeVisualProperties();
  }

  raw_ptr<HeadlessWebContentsImpl> headless_web_contents_;  // Not owned.

  bool is_fullscreen_ = false;
  gfx::Rect before_fullscreen_bounds_;
};

namespace {
constexpr uint64_t kBeginFrameSourceId = viz::BeginFrameArgs::kManualSourceId;
}

class HeadlessWebContentsImpl::PendingFrame final
    : public base::RefCounted<HeadlessWebContentsImpl::PendingFrame> {
 public:
  PendingFrame(uint64_t sequence_number, FrameFinishedCallback callback)
      : sequence_number_(sequence_number), callback_(std::move(callback)) {}

  PendingFrame(const PendingFrame&) = delete;
  PendingFrame& operator=(const PendingFrame&) = delete;

  void OnFrameComplete(const viz::BeginFrameAck& ack) {
    DCHECK_EQ(kBeginFrameSourceId, ack.frame_id.source_id);
    DCHECK_EQ(sequence_number_, ack.frame_id.sequence_number);
    has_damage_ = ack.has_damage;
  }

  void OnReadbackComplete(const SkBitmap& bitmap) {
    TRACE_EVENT2(
        "headless", "HeadlessWebContentsImpl::PendingFrame::OnReadbackComplete",
        "sequence_number", sequence_number_, "success", !bitmap.drawsNothing());
    if (bitmap.drawsNothing()) {
      LOG(WARNING) << "Readback from surface failed.";
      return;
    }
    bitmap_ = std::make_unique<SkBitmap>(bitmap);
  }

  base::WeakPtr<PendingFrame> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<PendingFrame>;

  ~PendingFrame() {
    std::move(callback_).Run(has_damage_, std::move(bitmap_), "");
  }

  const uint64_t sequence_number_;

  FrameFinishedCallback callback_;
  bool has_damage_ = false;
  std::unique_ptr<SkBitmap> bitmap_;
  base::WeakPtrFactory<PendingFrame> weak_ptr_factory_{this};
};

// static
std::unique_ptr<HeadlessWebContentsImpl> HeadlessWebContentsImpl::Create(
    HeadlessWebContents::Builder* builder) {
  content::WebContents::CreateParams create_params(builder->browser_context_);
  auto headless_web_contents = base::WrapUnique(new HeadlessWebContentsImpl(
      content::WebContents::Create(create_params), builder->browser_context_,
      builder->use_tab_target_));

  headless_web_contents->begin_frame_control_enabled_ =
      builder->enable_begin_frame_control_ ||
      headless_web_contents->browser()->options()->enable_begin_frame_control;
  headless_web_contents->InitializeWindow(gfx::Rect(builder->window_size_));
  if (!headless_web_contents->OpenURL(builder->initial_url_))
    return nullptr;
  return headless_web_contents;
}

// static
std::unique_ptr<HeadlessWebContentsImpl>
HeadlessWebContentsImpl::CreateForChildContents(
    HeadlessWebContentsImpl* parent,
    std::unique_ptr<content::WebContents> child_contents) {
  auto child = base::WrapUnique(new HeadlessWebContentsImpl(
      std::move(child_contents), parent->browser_context(),
      parent->use_tab_target_));

  // Child contents have their own root window and inherit the BeginFrameControl
  // setting.
  child->begin_frame_control_enabled_ = parent->begin_frame_control_enabled_;
  child->InitializeWindow(child->web_contents_->GetContainerBounds());

  // There may already be frames and we may have missed the RenderFrameCreated
  // callback so make sure they also have our services.
  // We want to iterate all frame trees because RenderFrameCreated gets called
  // for any RenderFrame created. base::Unretained is safe here because
  // ForEachRenderFrameHost is synchronous.
  child->web_contents_->ForEachRenderFrameHost(
      [&child](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive())
          child->RenderFrameCreated(render_frame_host);
      });

  return child;
}

void HeadlessWebContentsImpl::InitializeWindow(
    const gfx::Rect& initial_bounds) {
  static int window_id = 1;
  window_id_ = window_id++;
  window_state_ = "normal";

  browser()->PlatformInitializeWebContents(this);
  SetBounds(initial_bounds);
}

void HeadlessWebContentsImpl::SetBounds(const gfx::Rect& bounds) {
  browser()->PlatformSetWebContentsBounds(this, bounds);
}

HeadlessWebContentsImpl::HeadlessWebContentsImpl(
    std::unique_ptr<content::WebContents> web_contents,
    HeadlessBrowserContextImpl* browser_context,
    bool use_tab_target)
    : content::WebContentsObserver(web_contents.get()),
      browser_context_(browser_context),
      render_process_host_(web_contents->GetPrimaryMainFrame()->GetProcess()),
      web_contents_delegate_(new HeadlessWebContentsImpl::Delegate(this)),
      web_contents_(std::move(web_contents)),
      agent_host_(use_tab_target
                      ? content::DevToolsAgentHost::GetOrCreateForTab(
                            web_contents_.get())
                      : content::DevToolsAgentHost::GetOrCreateFor(
                            web_contents_.get())),
      use_tab_target_(use_tab_target) {
#if BUILDFLAG(ENABLE_PRINTING)
  HeadlessPrintManager::CreateForWebContents(web_contents_.get());
#endif
  UpdatePrefsFromSystemSettings(web_contents_->GetMutableRendererPrefs());
  web_contents_->GetMutableRendererPrefs()->accept_languages =
      browser_context->options()->accept_language();
  web_contents_->GetMutableRendererPrefs()->hinting =
      browser_context->options()->font_render_hinting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kForceWebRtcIPHandlingPolicy)) {
    web_contents_->GetMutableRendererPrefs()->webrtc_ip_handling_policy =
        command_line->GetSwitchValueASCII(
            ::switches::kForceWebRtcIPHandlingPolicy);
  }

  web_contents_->SetDelegate(web_contents_delegate_.get());
  render_process_host_->AddObserver(this);
  agent_host_->AddObserver(this);
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  agent_host_->RemoveObserver(this);
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
  // Defer destruction of WindowTreeHost, as it does sync mojo calls
  // in the destructor of ui::Compositor.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(window_tree_host_));
}

void HeadlessWebContentsImpl::RenderViewReady() {
  DCHECK(web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());

  if (devtools_target_ready_notification_sent_)
    return;

  for (auto& observer : observers_)
    observer.DevToolsTargetReady();

  devtools_target_ready_notification_sent_ = true;
}

bool HeadlessWebContentsImpl::OpenURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  params.force_new_browsing_instance =
      browser()->options()->force_new_browsing_instance;

  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_delegate_->ActivateContents(web_contents_.get());
  web_contents_->Focus();
  return true;
}

void HeadlessWebContentsImpl::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_context()->DestroyWebContents(this);
}

std::string HeadlessWebContentsImpl::GetDevToolsAgentHostId() {
  return agent_host_->GetId();
}

void HeadlessWebContentsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HeadlessWebContentsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HeadlessWebContentsImpl::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_EQ(render_process_host_, host);
  for (auto& observer : observers_)
    observer.RenderProcessExited(info.status, info.exit_code);
}

void HeadlessWebContentsImpl::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_EQ(render_process_host_, host);
  render_process_host_->RemoveObserver(this);
  render_process_host_ = nullptr;
}

content::WebContents* HeadlessWebContentsImpl::web_contents() const {
  return web_contents_.get();
}

HeadlessBrowserImpl* HeadlessWebContentsImpl::browser() const {
  return browser_context_->browser();
}

HeadlessBrowserContextImpl* HeadlessWebContentsImpl::browser_context() const {
  return browser_context_;
}

void HeadlessWebContentsImpl::BeginFrame(
    const base::TimeTicks& frame_timeticks,
    const base::TimeTicks& deadline,
    const base::TimeDelta& interval,
    bool animate_only,
    bool capture_screenshot,
    FrameFinishedCallback frame_finished_callback) {
  DCHECK(begin_frame_control_enabled_);
  if (pending_frame_) {
    std::move(frame_finished_callback)
        .Run(false, nullptr, "Another frame is pending");
    return;
  }
  TRACE_EVENT2("headless", "HeadlessWebContentsImpl::BeginFrame", "frame_time",
               frame_timeticks, "capture_screenshot", capture_screenshot);

  int64_t sequence_number = begin_frame_sequence_number_++;
  auto pending_frame = base::MakeRefCounted<PendingFrame>(
      sequence_number, std::move(frame_finished_callback));
  pending_frame_ = pending_frame->AsWeakPtr();
  if (capture_screenshot) {
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderWidgetHostView();
    if (view && view->IsSurfaceAvailableForCopy()) {
      view->CopyFromSurface(
          gfx::Rect(), gfx::Size(),
          base::BindOnce(&PendingFrame::OnReadbackComplete, pending_frame));
    } else {
      LOG(WARNING) << "Surface not ready for screenshot.";
    }
  }

  auto args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, kBeginFrameSourceId, sequence_number,
      frame_timeticks, deadline, interval, viz::BeginFrameArgs::NORMAL);
  args.animate_only = animate_only;

  ui::Compositor* compositor = browser()->PlatformGetCompositor(this);
  CHECK(compositor);
  compositor->IssueExternalBeginFrame(
      args, /*force=*/true,
      base::BindOnce(&PendingFrame::OnFrameComplete, pending_frame));
}

HeadlessWebContents::Builder::Builder(
    HeadlessBrowserContextImpl* browser_context)
    : browser_context_(browser_context),
      window_size_(browser_context->options()->window_size()) {}

HeadlessWebContents::Builder::~Builder() = default;

HeadlessWebContents::Builder::Builder(Builder&&) = default;

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetInitialURL(
    const GURL& initial_url) {
  initial_url_ = initial_url;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetWindowSize(
    const gfx::Size& size) {
  window_size_ = size;
  return *this;
}

HeadlessWebContents::Builder&
HeadlessWebContents::Builder::SetEnableBeginFrameControl(
    bool enable_begin_frame_control) {
  enable_begin_frame_control_ = enable_begin_frame_control;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetUseTabTarget(
    bool use_tab_target) {
  use_tab_target_ = use_tab_target;
  return *this;
}

HeadlessWebContents* HeadlessWebContents::Builder::Build() {
  return browser_context_->CreateWebContents(this);
}

}  // namespace headless
