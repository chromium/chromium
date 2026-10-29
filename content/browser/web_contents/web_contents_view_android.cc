// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/slim/layer.h"
#include "components/input/features.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/content_ui_event_handler.h"
#include "content/browser/android/drop_data_android.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/select_popup.h"
#include "content/browser/android/selection/selection_popup_controller.h"
#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"
#include "content/browser/navigation_transitions/blur_transition_animation_manager.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/drop_data.h"
#include "net/base/mime_util.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/android/ui_android_features.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_skia.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/jar_jni/DragEvent_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

constexpr char16_t kMimeTypeIntent[] = u"text/vnd.android.intent";
constexpr char16_t kMimeTypeChromeLink[] = u"chrome/link";

WebContentsViewAndroid::RenderWidgetHostViewCreateFunction
    g_create_render_widget_host_view = nullptr;

// Returns the minimum distance in DIPs, for drag event being considered as an
// intentional drag.
int DragMovementThresholdDip() {
  static int radius = features::kTouchDragMovementThresholdDip.Get();
  return radius;
}

// True if we want to disable Android native event batching and use
// compositor event queue.
bool ShouldRequestUnbufferedDispatch() {
  static bool should_request_unbuffered_dispatch =
      !GetContentClient()->UsingSynchronousCompositing() &&
      !base::FeatureList::IsEnabled(
          input::features::kUseAndroidBufferedInputDispatch);
  return should_request_unbuffered_dispatch;
}

bool IsDragAndDropEnabled() {
  // Cache the feature flag value so it isn't queried on every drag start.
  static const bool drag_feature_enabled =
      base::FeatureList::IsEnabled(features::kTouchDragAndContextMenu);
  return drag_feature_enabled;
}

bool IsDragEnabledForDropData(const DropData& drop_data) {
  return IsDragAndDropEnabled() || drop_data.text.has_value();
}

// Constructs a provisional `DropData` from `drag_metadata` during
// `ACTION_DRAG_ENTERED` (when Android's `DragEvent` only exposes
// `ClipDescription` MIME types, before `ACTION_DROP` provides `ClipData`) so
// `WebContentsDelegate::CanDragEnter` can inspect the advertised drag types.
// Note: Because actual `ClipData` values (e.g. specific URLs or text content)
// are not available until `ACTION_DROP` on Android, if type-level inspection
// is not sufficient, we will need to rearchitect to a "can drop" check instead
// of a "can enter" check.
DropData BuildProvisionalDropDataFromMetadata(
    const std::vector<DropData::Metadata>& drag_metadata) {
  DropData drop_data;
  for (const auto& item : drag_metadata) {
    switch (item.kind) {
      case DropData::Kind::FILENAME:
        drop_data.filenames.emplace_back(item.filename,
                                         item.filename.BaseName());
        break;
      case DropData::Kind::STRING:
        if (item.mime_type == ui::kMimeTypePlainText16) {
          drop_data.text.emplace();
        } else if (item.mime_type == ui::kMimeTypeHtml16) {
          drop_data.html.emplace();
        } else if (item.mime_type == ui::kMimeTypeMozillaUrl16) {
          drop_data.url_infos.emplace_back(GURL(), std::u16string());
        }
        break;
      case DropData::Kind::FILESYSTEMFILE:
      case DropData::Kind::BINARY:
        break;
    }
  }
  return drop_data;
}

}  // namespace

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  CHECK(contents, base::NotFatalUntil::M158);
  CHECK(client, base::NotFatalUntil::M158);
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(contents)->GetView());
  CHECK(!wcva->synchronous_compositor_client(), base::NotFatalUntil::M158);
  wcva->set_synchronous_compositor_client(client);
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents->GetRenderWidgetHostView());
  if (rwhv) {
    rwhv->SetSynchronousCompositorClient(client);
  }
}

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view) {
  auto rv = std::make_unique<WebContentsViewAndroid>(web_contents,
                                                     std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewAndroid::DragTargetSession::DragTargetSession() = default;
WebContentsViewAndroid::DragTargetSession::~DragTargetSession() = default;

struct WebContentsViewAndroid::OnPerformingDropContext {
  OnPerformingDropContext(const WebContentsViewDragSecurityInfo& security_info,
                          scoped_refptr<DragTargetSession> drag_session,
                          const gfx::PointF& location,
                          const gfx::PointF& screen_location,
                          base::ScopedClosureRunner drop_finished)
      : security_info(security_info),
        drag_session(std::move(drag_session)),
        location(location),
        screen_location(screen_location),
        drop_finished(std::move(drop_finished)) {}
  OnPerformingDropContext(const OnPerformingDropContext&) = delete;
  OnPerformingDropContext& operator=(const OnPerformingDropContext&) = delete;
  OnPerformingDropContext(OnPerformingDropContext&&) = default;
  OnPerformingDropContext& operator=(OnPerformingDropContext&&) = default;
  ~OnPerformingDropContext() = default;

  // Snapshot of the view's drag security info at `ACTION_DROP`.
  WebContentsViewDragSecurityInfo security_info;
  scoped_refptr<DragTargetSession> drag_session;
  gfx::PointF location;
  gfx::PointF screen_location;
  base::WeakPtr<RenderWidgetHostImpl> target_rwh;
  gfx::PointF transformed_pt;
  // Runs `FinishDrop(drag_session, kNone)` when the context is destroyed, so
  // that every way a drop can end without reaching the renderer (invalid
  // target, `CanDragEnter` rejection, delegate abort, or the delegate dropping
  // its callback) still releases a source-end notification deferred by
  // `OnDragEnded()`. `CompleteDrop()` finishes the drop with the real
  // operation first, making this a no-op.
  base::ScopedClosureRunner drop_finished;
};

// static
void WebContentsViewAndroid::InstallCreateHookForTests(
    RenderWidgetHostViewCreateFunction create_render_widget_host_view) {
  CHECK_EQ(nullptr, g_create_render_widget_host_view);
  g_create_render_widget_host_view = create_render_widget_host_view;
}

WebContentsViewAndroid::WebContentsViewAndroid(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      delegate_(std::move(delegate)),
      view_(ui::ViewAndroid::LayoutType::kNormal),
      synchronous_compositor_client_(nullptr) {
  view_.SetLayer(cc::slim::Layer::Create());
  view_.set_event_handler(this);

  // `rwhva_parent_` is a child layer of `view_`.
  parent_for_web_page_widgets_ = cc::slim::Layer::Create();
  view_.GetLayer()->AddChild(parent_for_web_page_widgets_);
  if (GetContentClient()->browser()->ShouldAnimateBackForwardTransitions()) {
    back_forward_animation_manager_ =
        std::make_unique<BackForwardTransitionAnimationManagerAndroid>(
            this, &web_contents_->GetController());
  }
}

WebContentsViewAndroid::~WebContentsViewAndroid() {
  // The animation manager holds a reference to `parent_for_web_page_widgets_`.
  // Explicitly destroy the animation manager before resetting
  // `parent_for_web_page_widgets_`.
  if (back_forward_animation_manager_) {
    back_forward_animation_manager_.reset();
  }

  // Opposite to the construction order - disconnect the child first.
  parent_for_web_page_widgets_->RemoveFromParent();
  parent_for_web_page_widgets_.reset();

  if (view_.GetLayer()) {
    view_.GetLayer()->RemoveFromParent();
  }
  view_.set_event_handler(nullptr);
}

void WebContentsViewAndroid::SetContentUiEventHandler(
    std::unique_ptr<ContentUiEventHandler> handler) {
  content_ui_event_handler_ = std::move(handler);
}

void WebContentsViewAndroid::SetOverscrollRefreshHandler(
    std::unique_ptr<ui::OverscrollRefreshHandler> overscroll_refresh_handler) {
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  auto* rwhva =
      rwhv ? static_cast<RenderWidgetHostViewAndroid*>(rwhv) : nullptr;

  if (rwhva) {
    rwhva->ResetOverscrollController();
  }

  overscroll_refresh_handler_ = std::move(overscroll_refresh_handler);

  if (rwhva && overscroll_refresh_handler_) {
    rwhva->OnOverscrollRefreshHandlerAvailable();
  }
}

ui::OverscrollRefreshHandler*
WebContentsViewAndroid::GetOverscrollRefreshHandler() const {
  return overscroll_refresh_handler_.get();
}

gfx::NativeView WebContentsViewAndroid::GetNativeView() const {
  return const_cast<gfx::NativeView>(&view_);
}

gfx::NativeView WebContentsViewAndroid::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    return rwhv->GetNativeView();
  }

  // TODO(sievers): This should return null.
  return GetNativeView();
}

RenderWidgetHostViewAndroid*
WebContentsViewAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = nullptr;
  rwhv = web_contents_->GetRenderWidgetHostView();
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

gfx::NativeWindow WebContentsViewAndroid::GetTopLevelNativeWindow() const {
  return view_.GetWindowAndroid();
}

gfx::Rect WebContentsViewAndroid::GetContainerBounds() const {
  return GetViewBounds();
}

void WebContentsViewAndroid::SetPageTitle(const std::u16string& title) {
  // Do nothing.
}

void WebContentsViewAndroid::Focus() {
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    static_cast<RenderWidgetHostViewAndroid*>(rwhv)->Focus();
  }
}

void WebContentsViewAndroid::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault()) {
    web_contents_->SetFocusToLocationBar();
  } else {
    Focus();
  }
}

void WebContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::FocusThroughTabTraversal(bool reverse) {
  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewAndroid::GetDropData() const {
  return drop_data_.get();
}

gfx::Rect WebContentsViewAndroid::GetViewBounds() const {
  return gfx::Rect(view_.GetSizeDIPs());
}

void WebContentsViewAndroid::Resize(const gfx::Rect& new_bounds) {
  content::RenderWidgetHostView* view =
      web_contents_->GetRenderWidgetHostView();
  if (view) {
    view->SetBounds(new_bounds);
  }
}

gfx::Size WebContentsViewAndroid::GetSize() const {
  return view_.GetSizeDIPs();
}

void WebContentsViewAndroid::CreateView(gfx::NativeView context) {}

RenderWidgetHostViewBase* WebContentsViewAndroid::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    CHECK(RenderViewHostFactory::has_factory(), base::NotFatalUntil::M158);
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }
  // Note that while this instructs the render widget host to reference
  // |native_view_|, this has no effect without also instructing the
  // native view (i.e. ContentView) how to obtain a reference to this widget in
  // order to paint it.
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  auto* rwhv = g_create_render_widget_host_view
                   ? g_create_render_widget_host_view(
                         rwhi, &view_, parent_for_web_page_widgets_.get())
                   : new RenderWidgetHostViewAndroid(
                         rwhi, &view_, parent_for_web_page_widgets_.get());
  rwhv->SetSynchronousCompositorClient(synchronous_compositor_client_);
  return rwhv;
}

RenderWidgetHostViewBase* WebContentsViewAndroid::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  return new RenderWidgetHostViewAndroid(rwhi, /*parent_native_view=*/nullptr,
                                         /*parent_layer=*/nullptr);
}

void WebContentsViewAndroid::RenderViewReady() {
  if (device_orientation_ == 0) {
    return;
  }
  auto* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva) {
    rwhva->UpdateScreenInfo();
  }

  web_contents_->OnScreenOrientationChange();
}

void WebContentsViewAndroid::RenderViewHostChanged(RenderViewHost* old_host,
                                                   RenderViewHost* new_host) {
  if (old_host) {
    auto* rwhv = old_host->GetWidget()->GetView();
    if (rwhv && rwhv->GetNativeView()) {
      auto* rwhva = static_cast<RenderWidgetHostViewAndroid*>(rwhv);
      rwhva->ResetOverscrollController();
      rwhva->UpdateNativeViewTree(/*parent_native_view=*/nullptr,
                                  /*parent_layer=*/nullptr);
      rwhva->UpdateTooltip(std::u16string());
    }
  }

  auto* rwhv = new_host->GetWidget()->GetView();
  if (rwhv && rwhv->GetNativeView()) {
    auto* rwhva = static_cast<RenderWidgetHostViewAndroid*>(rwhv);
    rwhva->UpdateNativeViewTree(&view_, parent_for_web_page_widgets_.get());
    SetFocus(view_.HasFocus());
    rwhva->ResetOverscrollController();
    if (overscroll_refresh_handler_) {
      rwhva->OnOverscrollRefreshHandlerAvailable();
    }
  }
}

void WebContentsViewAndroid::SetFocus(bool focused) {
  auto* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva) {
    return;
  }
  if (focused) {
    rwhva->GotFocus();
  } else {
    rwhva->LostFocus();
  }
}

void WebContentsViewAndroid::SetOverscrollControllerEnabled(bool enabled) {}

void WebContentsViewAndroid::OnCapturerCountChanged() {}

void WebContentsViewAndroid::FullscreenStateChanged(bool is_fullscreen) {
  if (is_fullscreen && select_popup_) {
    select_popup_->HideMenu();
  }
}

BackForwardTransitionAnimationManager*
WebContentsViewAndroid::GetBackForwardTransitionAnimationManager() {
  if (!back_forward_animation_manager_ &&
      GetContentClient()->browser()->ShouldAnimateBackForwardTransitions()) {
    back_forward_animation_manager_ =
        std::make_unique<BackForwardTransitionAnimationManagerAndroid>(
            this, &web_contents_->GetController());
  }
  return back_forward_animation_manager_.get();
}

void WebContentsViewAndroid::DestroyBackForwardTransitionAnimationManager() {
  back_forward_animation_manager_.reset();
}

void WebContentsViewAndroid::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  // Do nothing if the BlurTransitionAnimationManager is already instantiated.
  if (BlurTransitionAnimationManager::FromWebContents(web_contents_)) {
    return;
  }

  // Lazily instantiate the BlurTransitionAnimationManager.
  if (ShouldShowBlurTransitionAnimation(navigation_handle)) {
    BlurTransitionAnimationManager::CreateForWebContents(web_contents_);
    // In this lazy instantiation flow, the ReadyToCommitNavigation notification
    // was not received by the animation manager. We manually invoke to
    // ensure we don't miss the animation.
    BlurTransitionAnimationManager::FromWebContents(web_contents_)
        ->ReadyToCommitNavigation(navigation_handle);
  }
}

void WebContentsViewAndroid::ShowContextMenu(RenderFrameHost& render_frame_host,
                                             const ContextMenuParams& params) {
  if (is_active_drag_ && drag_exceeded_movement_threshold_) {
    return;
  }

  auto* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      web_contents_->GetRenderWidgetHostView());

  // See if context menu is handled by SelectionController as a selection menu.
  // If not, use the delegate to show it.
  if (rwhv && rwhv->ShowSelectionMenu(&render_frame_host, params)) {
    return;
  }

  if (delegate_) {
    delegate_->ShowContextMenu(render_frame_host, params);
  }
}

SelectPopup* WebContentsViewAndroid::GetSelectPopup() {
  if (!select_popup_) {
    select_popup_ = std::make_unique<SelectPopup>(web_contents_);
  }
  return select_popup_.get();
}

SelectionPopupController*
WebContentsViewAndroid::GetSelectionPopupController() {
  SelectionPopupController* controller = nullptr;
  if (auto* rwhva = GetRenderWidgetHostViewAndroid()) {
    controller = rwhva->selection_popup_controller();
  }
  return controller;
}

void WebContentsViewAndroid::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  GetSelectPopup()->ShowMenu(std::move(popup_client), bounds, item_font_size,
                             std::move(menu_items), selected_item,
                             allow_multiple_selection, right_aligned);
}

void WebContentsViewAndroid::StartDragging(
    RenderFrameHost& source_rfh,
    const DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info) {
  // Disallow reentrant drag which could be an attempt to exploit drag state.
  // Also refuse to start while the previous drag's deferred source
  // notification is pending: the source renderer's drag session is still
  // open for that drop, and a new drag from it would be ended by the deferred
  // `DragSourceEndedAt` (or, if starting failed, would cancel the pending
  // drop).
  if (drag_security_info_.did_initiate() || source_drag_end_pending_) {
    return;
  }
  RenderWidgetHostImpl* const source_rwh =
      static_cast<RenderWidgetHostImpl*>(source_rfh.GetRenderWidgetHost());
  current_source_rwh_for_drag_ = source_rwh->GetWeakPtr();
  if (!IsDragEnabledForDropData(drop_data)) {
    ClearDragStateOnStartFailure(source_rwh);
    return;
  }

  gfx::NativeView native_view = GetNativeView();
  if (!native_view) {
    ClearDragStateOnStartFailure(source_rwh);
    return;
  }

  ClipboardEndpoint source_endpoint = CreateClipboardEndpoint(source_rfh);

  if (!IsDragAllowedByDataControlPolicy(source_endpoint, drop_data)) {
    ClearDragStateOnStartFailure(source_rwh);
    return;
  }
  drag_security_info_.OnDragInitiated(source_rwh, drop_data);

  const SkBitmap* bitmap = image.bitmap();
  SkBitmap dummy_bitmap;

  if (image.size().IsEmpty()) {
    // An empty drag image is possible if the Javascript sets an empty drag
    // image on purpose.
    // Create a dummy 1x1 pixel image to avoid crashes when converting to java
    // bitmap.
    dummy_bitmap.allocN32Pixels(1, 1);
    dummy_bitmap.eraseColor(0);
    bitmap = &dummy_bitmap;
  }

  // TODO(crbug.com/40886472): Consolidate cursor_offset and drag_obj_rect with
  // drop_data.

  ScopedJavaLocalRef<jobject> jdrop_data = ToJavaDropData(drop_data);
  if (!native_view->StartDragAndDrop(
          gfx::ConvertToJavaBitmap(*bitmap), jdrop_data, cursor_offset.x(),
          cursor_offset.y(), drag_obj_rect.width(), drag_obj_rect.height())) {
    ClearDragStateOnStartFailure(source_rwh);
    return;
  }

  if (auto* selection_popup_controller = GetSelectionPopupController()) {
    selection_popup_controller->HidePopupsAndPreserveSelection();
    // Hide the handles temporarily.
    auto* rwhva = GetRenderWidgetHostViewAndroid();
    if (rwhva) {
      rwhva->SetTextHandlesTemporarilyHidden(true);
    }
  }
}

void WebContentsViewAndroid::UpdateDragOperation(
    ui::mojom::DragOperation op,
    bool document_is_handling_drag,
    RenderWidgetHostImpl* source_rwh) {
  // On Android, ACTION_DRAG_ENTERED carries no valid coordinates, so
  // OnDragEntered always enters the main-frame RenderWidgetHost first before
  // ACTION_DRAG_LOCATION hit-tests into child/inner RenderWidgetHosts (such as
  // an OOPIF or <webview> guest) — unlike Aura and Mac, where DragEnter carries
  // coordinates and hit-tests the target RenderWidgetHost directly. Ignore
  // stale asynchronous replies from a previously entered RenderWidgetHost (or
  // after the drag has exited/ended) so they cannot clobber `active_drag_`
  // before ACTION_DROP copies `document_is_handling_drag` into the drop's
  // `DropData`.
  if (!active_drag_ || !active_drag_->target_rwh ||
      source_rwh != active_drag_->target_rwh.get()) {
    return;
  }
  active_drag_->operation = op;
  active_drag_->document_is_handling_drag = document_is_handling_drag;
}

scoped_refptr<WebContentsViewAndroid::DragTargetSession>
WebContentsViewAndroid::CreateDragTargetSession(
    const ui::DragEventAndroid& event) const {
  auto session = base::MakeRefCounted<DragTargetSession>();
  session->drag_id = ParseDragIdFromEvent(event);
  for (const std::u16string& mime_type : event.mime_types()) {
    // The old Android ClipData API does not let you detect whether URIs
    // (files) are included in drag-start event, only in drop event.  We
    // guess that text, html, and some other types are not files.  New API
    // ClipDescription.hasContentUri() provides this information.
    bool should_guess = true;
    if (base::FeatureList::IsEnabled(ui::kHasContentUri)) {
      should_guess = !event.has_files();
    }
    if (should_guess &&
        (mime_type == ui::kMimeTypePlainText16 ||
         mime_type == ui::kMimeTypeHtml16 ||
         mime_type == ui::kMimeTypeMozillaUrl16 ||
         mime_type == kMimeTypeIntent || mime_type == kMimeTypeChromeLink)) {
      session->metadata.push_back(DropData::Metadata::CreateForMimeType(
          DropData::Kind::STRING, mime_type));
    } else if (mime_type == ui::kMimeTypeDataTransferCustomData16) {
      // Ignore, we will unpack custom data from the event.
      continue;
    } else {
      if (!drag_security_info_.IsImageAccessibleFromFrame()) {
        continue;
      }
      // Create a file extension from the mime type.
      std::string ext = base::UTF16ToUTF8(mime_type);
      if (!net::GetPreferredExtensionForMimeType(ext, &ext)) {
        // Use mime subtype as a fallback.
        net::ParseMimeTypeWithoutParameter(ext, nullptr, &ext);
      }
      session->metadata.push_back(
          DropData::Metadata::CreateForFilePath(base::FilePath("file." + ext)));
    }
  }
  std::optional<base::Value> custom_data = ParseCustomDataFromEvent(event);
  if (custom_data) {
    for (auto item : custom_data->GetDict()) {
      if (!item.second.is_string()) {
        continue;
      }
      session->metadata.push_back(DropData::Metadata::CreateForMimeType(
          DropData::Kind::STRING, base::UTF8ToUTF16(item.first)));
    }
  }
  return session;
}

// Pass events to the renderer. In order to support OOPIF, we need to call
// WebContents::GetRenderWidgetHostAtPointAsynchronously() with the location of
// the event to determine which process to send the event to. This function
// seems to always return synchronously in this context, but has the potential
// to be async if there are pending events queued.
// GetRenderWidgetHostAtPointAsynchronously() is called for DRAG_LOCATION
// and DROP, but not for DRAG_ENTERED, DRAG_EXITED, or DRAG_ENDED since they do
// not contain a location. This creates a potential for events to arrive out of
// order, but testing with blink shows that it handles this ok.
//
// As the mouse moves across a page, if we detect that the RenderWidgetHost
// changes, we resend the entered event before sending the update or drop.
bool WebContentsViewAndroid::OnDragEvent(const ui::DragEventAndroid& event) {
  switch (event.action()) {
    case DragEventJni::ACTION_DRAG_ENTERED: {
      if (active_drag_) {
        OnDragExited();
      }
      active_drag_ = CreateDragTargetSession(event);
      OnDragEntered(event.location(), event.screen_location());
      break;
    }
    case DragEventJni::ACTION_DRAG_LOCATION:
      if (!active_drag_) {
        active_drag_ = CreateDragTargetSession(event);
      }
      OnDragUpdated(event.location(), event.screen_location());
      break;
    case DragEventJni::ACTION_DROP: {
      if (!active_drag_) {
        active_drag_ = CreateDragTargetSession(event);
      }
      if (active_drag_->rejected_by_delegate || active_drag_->dropped) {
        break;
      }
      active_drag_->dropped = true;
      active_drag_->drop_in_flight = true;
      auto initial_drop_data = std::make_unique<DropData>();
      initial_drop_data->did_originate_from_renderer = false;
      PopulateDropDataFromEvent(event, drag_security_info_,
                                initial_drop_data.get());

      OnPerformDrop(std::move(initial_drop_data),
                    OnPerformingDropContext(
                        drag_security_info_, active_drag_, event.location(),
                        event.screen_location(),
                        base::ScopedClosureRunner(base::BindOnce(
                            &WebContentsViewAndroid::FinishDrop,
                            weak_ptr_factory_.GetWeakPtr(), active_drag_,
                            ui::mojom::DragOperation::kNone))));
      break;
    }
    case DragEventJni::ACTION_DRAG_EXITED:
      OnDragExited();
      break;
    case DragEventJni::ACTION_DRAG_ENDED:
      OnDragEnded();
      break;
    case DragEventJni::ACTION_DRAG_STARTED:
      // Nothing meaningful to do.
      break;
  }
  return true;
}

void WebContentsViewAndroid::OnDragEntered(const gfx::PointF& location,
                                           const gfx::PointF& screen_location) {
  if (!active_drag_) {
    return;
  }
  // Android does not pass a valid location for ACTION_DRAG_STARTED, so do not
  // try to find GetRenderWidgetHostAtPointAsynchronously().
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    DragEnteredCallback(
        *active_drag_, drag_security_info_, location, screen_location,
        static_cast<RenderWidgetHostViewBase*>(rwhv)->GetWeakPtr());
  }
}

void WebContentsViewAndroid::DragEnteredCallback(
    DragTargetSession& drag_session,
    const WebContentsViewDragSecurityInfo& security_info,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    base::WeakPtr<RenderWidgetHostViewBase> target) {
  if (!target || drag_session.rejected_by_delegate) {
    return;
  }

  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!security_info.IsValidDragTarget(target_rwh)) {
    return;
  }

  blink::DragOperationsMask allowed_ops =
      static_cast<blink::DragOperationsMask>(blink::kDragOperationCopy |
                                             blink::kDragOperationMove);
  // Give the WebContentsDelegate an opportunity to reject the drag before
  // setting the active drag target or forwarding DragTargetDragEnter to the
  // renderer (matching Desktop's WebContentsViewAura and WebDragDest).
  if (WebContentsDelegate* delegate = web_contents_->GetDelegate()) {
    DropData provisional_drop_data =
        BuildProvisionalDropDataFromMetadata(drag_session.metadata);
    provisional_drop_data.drag_id = drag_session.drag_id;
    if (!delegate->CanDragEnter(web_contents_, provisional_drop_data,
                                allowed_ops)) {
      drag_session.rejected_by_delegate = true;
      return;
    }
  }

  drag_session.target_rwh = target_rwh->GetWeakPtr();
  // Reset per-target drag state until the newly entered RWH replies via
  // UpdateDragOperation().
  drag_session.operation = ui::mojom::DragOperation::kNone;
  drag_session.document_is_handling_drag = false;
  drag_session.target_rwh->DragTargetDragEnterWithMetaData(
      drag_session.metadata, location, screen_location, allowed_ops, 0,
      base::DoNothing());
}

void WebContentsViewAndroid::OnDragUpdated(const gfx::PointF& location,
                                           const gfx::PointF& screen_location) {
  drag_location_ = location;
  drag_screen_location_ = screen_location;

  // When drag and drop is enabled, attempt to dismiss the context menu if drag
  // leaves start location.
  if (IsDragAndDropEnabled()) {
    // On Android DragEvent.ACTION_DRAG_ENTER does not have a valid location.
    // See
    // https://developer.android.com/develop/ui/views/touch-and-input/drag-drop/concepts#table2.
    if (!is_active_drag_) {
      is_active_drag_ = true;
      drag_entered_location_ = location;
    } else if (!drag_exceeded_movement_threshold_) {
      int radius = DragMovementThresholdDip();
      if (!drag_location_.IsWithinDistance(drag_entered_location_, radius)) {
        drag_exceeded_movement_threshold_ = true;
        if (delegate_) {
          delegate_->DismissContextMenu();
        }
      }
    }
  }

  if (!active_drag_ || active_drag_->rejected_by_delegate ||
      active_drag_->dropped) {
    return;
  }

  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    web_contents_->GetRenderWidgetHostAtPointAsynchronously(
        static_cast<RenderWidgetHostViewBase*>(rwhv), location,
        base::BindOnce(&WebContentsViewAndroid::DragUpdatedCallback,
                       weak_ptr_factory_.GetWeakPtr(), active_drag_, location,
                       screen_location));
  }
}

void WebContentsViewAndroid::DragUpdatedCallback(
    scoped_refptr<DragTargetSession> drag_session,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    std::optional<gfx::PointF> transformed_pt) {
  if (drag_session != active_drag_ || !target || !transformed_pt.has_value() ||
      drag_session->rejected_by_delegate || drag_session->dropped) {
    return;
  }
  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!drag_security_info_.IsValidDragTarget(target_rwh)) {
    return;
  }

  if (target_rwh != drag_session->target_rwh.get()) {
    if (drag_session->target_rwh) {
      auto* rwhv = web_contents_->GetRenderWidgetHostView();
      if (!rwhv) {
        return;
      }
      gfx::PointF transformed_leave_point = location;
      if (auto* prev_target_view = static_cast<RenderWidgetHostViewBase*>(
              drag_session->target_rwh->GetView())) {
        static_cast<RenderWidgetHostViewBase*>(rwhv)
            ->TransformPointToCoordSpaceForView(location, prev_target_view,
                                                &transformed_leave_point);
      }
      drag_session->target_rwh->DragTargetDragLeave(transformed_leave_point,
                                                    screen_location);
      drag_session->target_rwh.reset();
      drag_session->operation = ui::mojom::DragOperation::kNone;
      drag_session->document_is_handling_drag = false;
    }
    DragEnteredCallback(*drag_session, drag_security_info_, *transformed_pt,
                        screen_location, target);
    if (drag_session->target_rwh.get() != target_rwh) {
      return;
    }
  }

  blink::DragOperationsMask allowed_ops =
      static_cast<blink::DragOperationsMask>(blink::kDragOperationCopy |
                                             blink::kDragOperationMove);
  target_rwh->DragTargetDragOver(*transformed_pt, screen_location, allowed_ops,
                                 0, base::DoNothing());
}

void WebContentsViewAndroid::OnDragExited() {
  if (active_drag_ && active_drag_->target_rwh) {
    active_drag_->target_rwh->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    active_drag_->target_rwh.reset();
  }
  active_drag_.reset();
}

void WebContentsViewAndroid::OnPerformDrop(
    std::unique_ptr<DropData> initial_drop_data,
    OnPerformingDropContext drop_context) {
  if (!drop_context.drag_session ||
      drop_context.drag_session->rejected_by_delegate) {
    return;
  }
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    const gfx::PointF location = drop_context.location;
    web_contents_->GetRenderWidgetHostAtPointAsynchronously(
        static_cast<RenderWidgetHostViewBase*>(rwhv), location,
        base::BindOnce(&WebContentsViewAndroid::PerformDropCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(initial_drop_data), std::move(drop_context)));
  }
}

void WebContentsViewAndroid::PerformDropCallback(
    std::unique_ptr<DropData> initial_drop_data,
    OnPerformingDropContext drop_context,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    std::optional<gfx::PointF> transformed_pt) {
  CHECK(drop_context.drag_session);
  auto leave_previous_target = [&]() {
    RenderWidgetHostImpl* prev_target =
        drop_context.drag_session->target_rwh.get();
    if (!prev_target) {
      return;
    }
    if (!active_drag_ || active_drag_ == drop_context.drag_session ||
        active_drag_->target_rwh.get() != prev_target) {
      prev_target->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    }
    drop_context.drag_session->target_rwh.reset();
    drop_context.drag_session->operation = ui::mojom::DragOperation::kNone;
    drop_context.drag_session->document_is_handling_drag = false;
  };

  if (!target || !transformed_pt.has_value() ||
      drop_context.drag_session->rejected_by_delegate) {
    leave_previous_target();
    return;
  }
  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!drop_context.security_info.IsValidDragTarget(target_rwh)) {
    leave_previous_target();
    return;
  }

  if (target_rwh != drop_context.drag_session->target_rwh.get()) {
    leave_previous_target();
    DragEnteredCallback(*drop_context.drag_session, drop_context.security_info,
                        *transformed_pt, drop_context.screen_location, target);
    if (drop_context.drag_session->target_rwh.get() != target_rwh) {
      return;
    }
  }

  initial_drop_data->document_is_handling_drag =
      drop_context.drag_session->document_is_handling_drag;
  target_rwh->FilterDropData(initial_drop_data.get());

  drop_context.target_rwh = target_rwh->GetWeakPtr();
  drop_context.transformed_pt = *transformed_pt;

  // Give the delegate a chance to inspect, modify or abort the drop before it
  // is dispatched to the renderer, as WebContentsViewAura does. The default
  // WebContentsViewDelegate::OnPerformingDrop() passes the drop through
  // unchanged. `delegate_` may be null when the embedder does not provide a
  // WebContentsViewDelegate (or in unit tests).
  if (delegate_) {
    delegate_->OnPerformingDrop(
        *initial_drop_data,
        base::BindOnce(&WebContentsViewAndroid::GotModifiedDropDataFromDelegate,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(drop_context)));
  } else {
    CompleteDrop(std::move(drop_context), std::move(initial_drop_data));
  }
}

void WebContentsViewAndroid::GotModifiedDropDataFromDelegate(
    OnPerformingDropContext drop_context,
    std::optional<DropData> modified_drop_data) {
  // This callback may run asynchronously after `OnPerformingDrop()` returns
  // (for example, after `ACTION_DRAG_ENDED` or after a new drag has started).
  // Verify that the target RenderWidgetHost still exists and remains a valid
  // drop target for the drag that initiated this drop.
  if (!drop_context.target_rwh || !drop_context.security_info.IsValidDragTarget(
                                      drop_context.target_rwh.get())) {
    return;
  }

  if (!modified_drop_data.has_value()) {
    // The delegate aborted the drop. Leave the renderer's drag session so it
    // dismisses any drop-target UI without dispatching a DOM drop, without
    // disturbing a newer drag session if one has started since ACTION_DROP.
    if (drop_context.drag_session == active_drag_) {
      OnDragExited();
    } else if (!active_drag_ || active_drag_->target_rwh.get() !=
                                    drop_context.target_rwh.get()) {
      drop_context.target_rwh->DragTargetDragLeave(gfx::PointF(),
                                                   gfx::PointF());
      drop_context.drag_session->target_rwh.reset();
    }
    return;
  }

  CompleteDrop(std::move(drop_context),
               std::make_unique<DropData>(std::move(*modified_drop_data)));
}

void WebContentsViewAndroid::CompleteDrop(OnPerformingDropContext drop_context,
                                          std::unique_ptr<DropData> drop_data) {
  web_contents_->Focus();
  if (!drop_context.target_rwh) {
    return;
  }
  drop_context.target_rwh->FilterDropData(drop_data.get());
  drop_context.target_rwh->DragTargetDrop(
      *drop_data, drop_context.transformed_pt, drop_context.screen_location, 0,
      base::DoNothing());
  drop_data_ = std::move(drop_data);
  // Notify the drag source, if `OnDragEnded()` deferred that until now, only
  // after the renderer has received the drop.
  FinishDrop(drop_context.drag_session, drop_context.drag_session->operation);
}

void WebContentsViewAndroid::FinishDrop(
    scoped_refptr<DragTargetSession> drag_session,
    ui::mojom::DragOperation operation) {
  if (!drag_session || !drag_session->drop_in_flight) {
    return;
  }
  drag_session->drop_in_flight = false;
  if (drag_session->pending_source_drag_end) {
    source_drag_end_pending_ = false;
    std::move(drag_session->pending_source_drag_end).Run(operation);
  }
}

void WebContentsViewAndroid::OnSystemDragEnded(RenderWidgetHost* source_rwh) {
  web_contents_->SystemDragEnded(source_rwh);

  // Restore the selection popups and the text handles if necessary.
  if (auto* selection_popup_controller = GetSelectionPopupController()) {
    selection_popup_controller->RestoreSelectionPopupsIfNecessary();
    auto* rwhva = GetRenderWidgetHostViewAndroid();
    if (rwhva) {
      rwhva->SetTextHandlesTemporarilyHidden(false);
    }
  }
}

void WebContentsViewAndroid::ClearDragStateOnStartFailure(
    RenderWidgetHost* source_rwh) {
  // Notify Blink to end the drag session. Since the system/OS drag was never
  // actually initiated, this resets renderer state without sending any DOM
  // events.
  OnSystemDragEnded(source_rwh);
  current_source_rwh_for_drag_.reset();
  drag_security_info_.OnDragEnded();
}

void WebContentsViewAndroid::OnDragEnded() {
  if (current_source_rwh_for_drag_) {
    auto end_source_drag = base::BindOnce(
        &WebContentsViewAndroid::EndSourceDrag, weak_ptr_factory_.GetWeakPtr(),
        current_source_rwh_for_drag_, drag_location_, drag_screen_location_);
    if (active_drag_ && active_drag_->drop_in_flight) {
      // `ACTION_DRAG_ENDED` arrives right after `ACTION_DROP` returns, so a
      // drop whose hit-testing or `OnPerformingDrop()` completes
      // asynchronously is still pending here. Telling the source now would
      // make Blink cancel its drag session (and fire `dragend`) before
      // `DragTargetDrop` reaches it, which drops the drop when the source and
      // target are the same widget. Defer until `FinishDrop()`, which reports
      // the operation the drop actually completed with.
      active_drag_->pending_source_drag_end = std::move(end_source_drag);
      source_drag_end_pending_ = true;
    } else {
      std::move(end_source_drag)
          .Run((active_drag_ && active_drag_->dropped)
                   ? active_drag_->operation
                   : ui::mojom::DragOperation::kNone);
    }
  }
  drag_security_info_.OnDragEnded();

  // Note: `drop_data_` is intentionally NOT reset here so post-drop callers of
  // `WebContents::GetDropData()` (in embedders and tests) can still inspect the
  // most recent completed drop after `ACTION_DRAG_ENDED`.
  current_source_rwh_for_drag_.reset();
  active_drag_.reset();
  is_active_drag_ = false;
  drag_exceeded_movement_threshold_ = false;
  drag_entered_location_ = gfx::PointF();
  drag_location_ = gfx::PointF();
  drag_screen_location_ = gfx::PointF();
}

void WebContentsViewAndroid::EndSourceDrag(
    base::WeakPtr<RenderWidgetHostImpl> source_rwh,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    ui::mojom::DragOperation operation) {
  if (!source_rwh) {
    return;
  }
  web_contents_->DragSourceEndedAt(location.x(), location.y(),
                                   screen_location.x(), screen_location.y(),
                                   operation, source_rwh.get());
  OnSystemDragEnded(source_rwh.get());
}

void WebContentsViewAndroid::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewAndroid::LostFocus(
    RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewAndroid::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse)) {
    return;
  }
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->Focus();
  }
}

int WebContentsViewAndroid::GetTopControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsHeight() : 0;
}

int WebContentsViewAndroid::GetTopControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsMinHeight() : 0;
}

int WebContentsViewAndroid::GetBottomControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsHeight() : 0;
}

int WebContentsViewAndroid::GetBottomControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsMinHeight() : 0;
}

bool WebContentsViewAndroid::ShouldAnimateBrowserControlsHeightChanges() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->ShouldAnimateBrowserControlsHeightChanges();
}

bool WebContentsViewAndroid::DoBrowserControlsShrinkRendererSize() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate &&
         delegate->DoBrowserControlsShrinkRendererSize(web_contents_);
}

bool WebContentsViewAndroid::OnlyExpandTopControlsAtPageTop() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->OnlyExpandTopControlsAtPageTop();
}

bool WebContentsViewAndroid::OnTouchEvent(const ui::MotionEventAndroid& event) {
  if (event.GetAction() == ui::MotionEventAndroid::Action::DOWN &&
      ShouldRequestUnbufferedDispatch()) {
    view_.RequestUnbufferedDispatch(event);
  }
  return false;  // let the children handle the actual event.
}

bool WebContentsViewAndroid::OnMouseEvent(const ui::MotionEventAndroid& event) {
  // Hover events can be intercepted when in accessibility mode.
  auto action = event.GetAction();
  if (action != ui::MotionEventAndroid::Action::HOVER_ENTER &&
      action != ui::MotionEventAndroid::Action::HOVER_EXIT &&
      action != ui::MotionEventAndroid::Action::HOVER_MOVE) {
    return false;
  }

  auto* manager = static_cast<BrowserAccessibilityManagerAndroid*>(
      web_contents_->GetRootBrowserAccessibilityManager());
  return manager && manager->OnHoverEvent(event);
}

bool WebContentsViewAndroid::OnGenericMotionEvent(
    const ui::MotionEventAndroid& event) {
  if (content_ui_event_handler_) {
    return content_ui_event_handler_->OnGenericMotionEvent(event);
  }
  return false;
}

bool WebContentsViewAndroid::OnKeyUp(const ui::KeyEventAndroid& event) {
  if (content_ui_event_handler_) {
    return content_ui_event_handler_->OnKeyUp(event);
  }
  return false;
}

bool WebContentsViewAndroid::DispatchKeyEvent(
    const ui::KeyEventAndroid& event) {
  if (content_ui_event_handler_) {
    return content_ui_event_handler_->DispatchKeyEvent(event);
  }
  return false;
}

bool WebContentsViewAndroid::ScrollBy(float delta_x, float delta_y) {
  if (content_ui_event_handler_) {
    content_ui_event_handler_->ScrollBy(delta_x, delta_y);
  }
  return false;
}

bool WebContentsViewAndroid::ScrollTo(float x, float y) {
  if (content_ui_event_handler_) {
    content_ui_event_handler_->ScrollTo(x, y);
  }
  return false;
}

void WebContentsViewAndroid::OnSizeChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    web_contents_->SendScreenRects();
    auto deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(
        ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames());
    rwhv->SynchronizeVisualProperties(deadline_policy, std::nullopt);
  }
}

void WebContentsViewAndroid::OnPhysicalBackingSizeChanged(
    std::optional<base::TimeDelta> deadline_override) {
  if (back_forward_animation_manager_) {
    back_forward_animation_manager_->OnPhysicalBackingSizeChanged();
  }
  if (web_contents_->GetRenderWidgetHostView()) {
    web_contents_->SendScreenRects();
  }
}

void WebContentsViewAndroid::OnBrowserControlsHeightChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    rwhv->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      std::nullopt);
  }
}

void WebContentsViewAndroid::OnControlsResizeViewChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    rwhv->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      std::nullopt);
  }
}

void WebContentsViewAndroid::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    rwhv->NotifyVirtualKeyboardOverlayRect(keyboard_rect);
  }
}

void WebContentsViewAndroid::ShowInterestInElement(int nodeID) {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    rwhv->ShowInterestInElement(nodeID);
  }
}

bool WebContentsViewAndroid::ShouldShowBlurTransitionAnimation(
    NavigationHandle* navigation_handle) {
  if (delegate_) {
    return delegate_->ShouldShowBlurTransitionAnimation(navigation_handle);
  }
  return false;
}

bool WebContentsViewAndroid::IsDragAllowedByDataControlPolicy(
    const ClipboardEndpoint& source,
    const DropData& drop_data) {
  return GetContentClient()->browser()->IsDragAllowedByPolicy(source,
                                                              drop_data);
}

}  // namespace content
