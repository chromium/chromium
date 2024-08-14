// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_mac.h"

#import <Carbon/Carbon.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "components/remote_cocoa/browser/ns_view_ids.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "content/app_shim_remote_cocoa/web_contents_ns_view_bridge.h"
#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/common/web_contents_ns_view_bridge.mojom-shared.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/display/display_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using blink::DragOperationsMask;
using remote_cocoa::mojom::DraggingInfoPtr;
using remote_cocoa::mojom::SelectionDirection;

// Ensure that the blink::DragOperationsMask enum values stay in sync with
// NSDragOperation constants, since the code below static_casts between 'em.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "enum mismatch: " #a)
STATIC_ASSERT_ENUM(NSDragOperationNone, blink::kDragOperationNone);
STATIC_ASSERT_ENUM(NSDragOperationCopy, blink::kDragOperationCopy);
STATIC_ASSERT_ENUM(NSDragOperationLink, blink::kDragOperationLink);
STATIC_ASSERT_ENUM(NSDragOperationMove, blink::kDragOperationMove);
STATIC_ASSERT_ENUM(NSDragOperationEvery, blink::kDragOperationEvery);

namespace content {
namespace {

// This helper's sole task is to write out data for a promised file; the caller
// is responsible for opening the file. It takes the drop data and an open file
// stream.
void PromiseWriterHelper(const DropData& drop_data, base::File file) {
  DCHECK(file.IsValid());
  UNSAFE_TODO(file.WriteAtCurrentPos(drop_data.file_contents.data(),
                                     drop_data.file_contents.length()));
}

WebContentsViewMac::RenderWidgetHostViewCreateFunction
    g_create_render_widget_host_view = nullptr;

}  // namespace

// static
void WebContentsViewMac::InstallCreateHookForTests(
    RenderWidgetHostViewCreateFunction create_render_widget_host_view) {
  CHECK_EQ(nullptr, g_create_render_widget_host_view);
  g_create_render_widget_host_view = create_render_widget_host_view;
}

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view) {
  auto rv =
      std::make_unique<WebContentsViewMac>(web_contents, std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewMac::WebContentsViewMac(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      ns_view_id_(remote_cocoa::GetNewNSViewId()),
      deferred_close_weak_ptr_factory_(this) {}

WebContentsViewMac::~WebContentsViewMac() {
  if (views_host_)
    views_host_->OnHostableViewDestroying();
  DCHECK(!views_host_);
  in_process_ns_view_bridge_.reset();
}

WebContentsViewCocoa* WebContentsViewMac::GetInProcessNSView() const {
  return in_process_ns_view_bridge_ ? in_process_ns_view_bridge_->GetNSView()
                                    : nil;
}

gfx::NativeView WebContentsViewMac::GetNativeView() const {
  return GetInProcessNSView();
}

gfx::NativeView WebContentsViewMac::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return nullptr;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewMac::GetTopLevelNativeWindow() const {
  NSWindow* window = [GetInProcessNSView() window];
  if (window)
    return window;
  if (delegate_)
    return delegate_->GetNativeWindow();
  return nullptr;
}

gfx::Rect WebContentsViewMac::GetContainerBounds() const {
  NSWindow* window = [GetInProcessNSView() window];
  NSRect bounds = [GetInProcessNSView() bounds];
  if (window)  {
    // Convert bounds to window coordinate space.
    bounds = [GetInProcessNSView() convertRect:bounds toView:nil];

    // Convert bounds to screen coordinate space.
    bounds = [window convertRectToScreen:bounds];
  }

  return gfx::ScreenRectFromNSRect(bounds);
}

void WebContentsViewMac::OnCapturerCountChanged() {}

void WebContentsViewMac::FullscreenStateChanged(bool is_fullscreen) {}

void WebContentsViewMac::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {
  window_controls_overlay_bounding_rect_ = bounding_rect;
  if (remote_ns_view_) {
    remote_ns_view_->UpdateWindowControlsOverlay(bounding_rect);
  } else {
    in_process_ns_view_bridge_->UpdateWindowControlsOverlay(bounding_rect);
  }
}

BackForwardTransitionAnimationManager*
WebContentsViewMac::GetBackForwardTransitionAnimationManager() {
  return nullptr;
}

void WebContentsViewMac::StartDragging(
    const DropData& drop_data,
    const url::Origin& source_origin,
    DragOperationsMask allowed_operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  // By allowing nested tasks, the code below also allows Close(),
  // which would deallocate |this|.  The same problem can occur while
  // processing -sendEvent:, so Close() is deferred in that case.
  // Drags from web content do not come via -sendEvent:, this sets the
  // same flag -sendEvent: would.
  base::mac::ScopedSendingEvent sending_event_scoper;

  // The drag invokes a nested event loop, arrange to continue
  // processing events.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  NSDragOperation mask = static_cast<NSDragOperation>(allowed_operations);

  [drag_dest_ initiateDragWithRenderWidgetHost:source_rwh dropData:drop_data];
  drag_source_start_rwh_ = source_rwh->GetWeakPtr();

  WebContentsDelegate* contents_delegate = web_contents_->GetDelegate();
  bool is_privileged =
      contents_delegate ? contents_delegate->IsPrivileged() : false;

  // TODO(crbug.com/40825138): The param `drag_obj_rect` is unused.

  if (remote_ns_view_) {
    remote_ns_view_->StartDrag(drop_data, source_origin, mask, image,
                               cursor_offset, is_privileged);
  } else {
    in_process_ns_view_bridge_->StartDrag(drop_data, source_origin, mask, image,
                                          cursor_offset, is_privileged);
  }
}

void WebContentsViewMac::Focus() {
  if (delegate())
    delegate()->ResetStoredFocus();

  // Focus the the fullscreen view, if one exists; otherwise, focus the content
  // native view. This ensures that the view currently attached to a NSWindow is
  // being used to query or set first responder state.
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return;

  static_cast<RenderWidgetHostViewBase*>(rwhv)->Focus();
}

void WebContentsViewMac::SetInitialFocus() {
  if (delegate())
    delegate()->ResetStoredFocus();

  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar();
  else
    Focus();
}

void WebContentsViewMac::StoreFocus() {
  if (delegate())
    delegate()->StoreFocus();
}

void WebContentsViewMac::RestoreFocus() {
  if (delegate() && delegate()->RestoreFocus())
    return;

  // Fall back to the default focus behavior if we could not restore focus.
  // TODO(shess): If location-bar gets focus by default, this will
  // select-all in the field.  If there was a specific selection in
  // the field when we navigated away from it, we should restore
  // that selection.
  SetInitialFocus();
}

void WebContentsViewMac::FocusThroughTabTraversal(bool reverse) {
  if (delegate())
    delegate()->ResetStoredFocus();

  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewMac::GetDropData() const {
  return [drag_dest_ currentDropData];
}

void WebContentsViewMac::UpdateDragOperation(ui::mojom::DragOperation operation,
                                             bool document_is_handling_drag) {
  [drag_dest_ setCurrentOperation:operation
           documentIsHandlingDrag:document_is_handling_drag];
}

void WebContentsViewMac::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewMac::LostFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

// This is called when the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewMac::TakeFocus(bool reverse) {
  if (delegate())
    delegate()->ResetStoredFocus();

  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse))
    return;
  if (delegate() && delegate()->TakeFocus(reverse))
    return;
  if (reverse) {
    [[GetInProcessNSView() window] selectPreviousKeyView:GetInProcessNSView()];
  } else {
    [[GetInProcessNSView() window] selectNextKeyView:GetInProcessNSView()];
  }
  if (remote_ns_view_)
    remote_ns_view_->TakeFocus(reverse);
}

void WebContentsViewMac::ShowContextMenu(RenderFrameHost& render_frame_host,
                                         const ContextMenuParams& params) {
  if (delegate())
    delegate()->ShowContextMenu(render_frame_host, params);
  else
    DLOG(ERROR) << "Cannot show context menus without a delegate.";
}

void WebContentsViewMac::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  popup_menu_helper_ = std::make_unique<PopupMenuHelper>(
      this, render_frame_host, std::move(popup_client));
  popup_menu_helper_->ShowPopupMenu(bounds, item_height, item_font_size,
                                    selected_item, std::move(menu_items),
                                    right_aligned, allow_multiple_selection);
  // Note: |this| may be deleted here.
}

void WebContentsViewMac::OnMenuClosed() {
  popup_menu_helper_.reset();
}

gfx::Rect WebContentsViewMac::GetViewBounds() const {
  NSRect window_bounds =
      [GetInProcessNSView() convertRect:GetInProcessNSView().bounds toView:nil];
  window_bounds.origin =
      [GetInProcessNSView().window convertPointToScreen:window_bounds.origin];
  return gfx::ScreenRectFromNSRect(window_bounds);
}

void WebContentsViewMac::CreateView(gfx::NativeView context) {
  in_process_ns_view_bridge_ =
      std::make_unique<remote_cocoa::WebContentsNSViewBridge>(ns_view_id_,
                                                              this);

  drag_dest_ = [[WebDragDest alloc] initWithWebContentsImpl:web_contents_];
  if (delegate_)
    [drag_dest_ setDragDelegate:delegate_->GetDragDestDelegate()];
}

RenderWidgetHostViewBase* WebContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  RenderWidgetHostViewMac* view =
      g_create_render_widget_host_view
          ? g_create_render_widget_host_view(render_widget_host)
          : new RenderWidgetHostViewMac(render_widget_host);
  if (delegate()) {
    view->SetDelegate(
        delegate()->GetDelegateForHost(render_widget_host, /*is_popup=*/false));
  }

  // Add the RenderWidgetHostView to the ui::Layer hierarchy.
  child_views_.push_back(view->GetWeakPtr());
  if (views_host_) {
    auto* remote_cocoa_application = views_host_->GetRemoteCocoaApplication();
    view->MigrateNSViewBridge(remote_cocoa_application, ns_view_id_);
    view->SetParentUiLayer(views_host_->GetUiLayer());
    view->SetParentAccessibilityElement(views_host_accessibility_element_);
  }

  // Fancy layout comes later; for now just make it our size and resize it
  // with us. In case there are other siblings of the content area, we want
  // to make sure the content area is on the bottom so other things draw over
  // it.
  NSView* view_view = view->GetNativeView().GetNativeNSView();
  [view_view setFrame:[GetInProcessNSView() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // Add the new view below all other views; this also keeps it below any
  // overlay view installed.
  [GetInProcessNSView() addSubview:view_view
                        positioned:NSWindowBelow
                        relativeTo:nil];
  [GetInProcessNSView() setNextKeyView:view_view];
  return view;
}

RenderWidgetHostViewBase* WebContentsViewMac::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewMac* view =
      new RenderWidgetHostViewMac(render_widget_host);

  // If the parent RenderWidgetHostViewMac is hosted in another process, ensure
  // that the popup window will be created created in the same process.
  // https://crbug.com/1091179
  if (views_host_) {
    auto* remote_cocoa_application = views_host_->GetRemoteCocoaApplication();
    view->MigrateNSViewBridge(remote_cocoa_application,
                              remote_cocoa::kInvalidNSViewId);
  }

  if (delegate()) {
    view->SetDelegate(
        delegate()->GetDelegateForHost(render_widget_host, /*is_popup=*/true));
  }
  return view;
}

void WebContentsViewMac::SetPageTitle(const std::u16string& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewMac::RenderViewReady() {}

void WebContentsViewMac::RenderViewHostChanged(RenderViewHost* old_host,
                                               RenderViewHost* new_host) {}

void WebContentsViewMac::SetOverscrollControllerEnabled(bool enabled) {
}

// Arrange to call CloseTab() after we're back to the main event loop.
// The obvious way to do this would be to post a NonNestable task, but that
// would fire when the event-tracking loop polls for events.  So we need to
// bounce the message via Cocoa, instead.
bool WebContentsViewMac::CloseTabAfterEventTrackingIfNeeded() {
  if (!base::message_pump_apple::IsHandlingSendEvent()) {
    return false;
  }

  deferred_close_weak_ptr_factory_.InvalidateWeakPtrs();
  auto weak_ptr = deferred_close_weak_ptr_factory_.GetWeakPtr();
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, ^{
    if (weak_ptr)
      weak_ptr->CloseTab();
  });
  return true;
}

void WebContentsViewMac::CloseTab() {
  web_contents_->Close();
}

std::list<RenderWidgetHostViewMac*> WebContentsViewMac::GetChildViews() {
  // Remove any child NSViews that have been destroyed.
  std::list<RenderWidgetHostViewMac*> result;
  for (auto iter = child_views_.begin(); iter != child_views_.end();) {
    if (*iter) {
      result.push_back(static_cast<RenderWidgetHostViewMac*>(iter->get()));
      iter++;
    } else {
      iter = child_views_.erase(iter);
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewMac, mojom::WebContentsNSViewHost:

void WebContentsViewMac::OnMouseEvent(std::unique_ptr<ui::Event> event) {
  if (!web_contents_ || !web_contents_->GetDelegate() || !event) {
    return;
  }

  web_contents_->GetDelegate()->ContentsMouseEvent(web_contents_, *event);
}

void WebContentsViewMac::OnBecameFirstResponder(SelectionDirection direction) {
  if (!web_contents_)
    return;
  if (direction == SelectionDirection::kDirect)
    return;

  web_contents_->FocusThroughTabTraversal(direction ==
                                          SelectionDirection::kReverse);
}

void WebContentsViewMac::OnWindowVisibilityChanged(
    remote_cocoa::mojom::Visibility mojo_visibility) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed())
    return;

  // TODO: make content use the mojo type for visibility.
  Visibility visibility = Visibility::VISIBLE;
  switch (mojo_visibility) {
    case remote_cocoa::mojom::Visibility::kVisible:
      visibility = Visibility::VISIBLE;
      break;
    case remote_cocoa::mojom::Visibility::kOccluded:
      visibility = Visibility::OCCLUDED;
      break;
    case remote_cocoa::mojom::Visibility::kHidden:
      visibility = Visibility::HIDDEN;
      break;
  }

  web_contents_->UpdateWebContentsVisibility(visibility);
}

void WebContentsViewMac::SetDropData(const DropData& drop_data) {
  [drag_dest_ setDropData:drop_data];
}

bool WebContentsViewMac::DraggingEntered(DraggingInfoPtr dragging_info,
                                         uint32_t* out_result) {
  *out_result = [drag_dest_ draggingEntered:dragging_info.get()];
  return true;
}

void WebContentsViewMac::DraggingExited() {
  [drag_dest_ draggingExited];
}

bool WebContentsViewMac::DraggingUpdated(DraggingInfoPtr dragging_info,
                                         uint32_t* out_result) {
  *out_result = [drag_dest_ draggingUpdated:dragging_info.get()];
  return true;
}

bool WebContentsViewMac::PerformDragOperation(DraggingInfoPtr dragging_info,
                                              bool* out_result) {
  *out_result = [drag_dest_ performDragOperation:dragging_info.get()
                     withWebContentsViewDelegate:delegate_.get()];
  return true;
}

bool WebContentsViewMac::DragPromisedFileTo(const base::FilePath& file_path,
                                            const DropData& drop_data,
                                            const GURL& download_url,
                                            const url::Origin& source_origin,
                                            base::FilePath* out_file_path) {
  *out_file_path = file_path;
  // This is called by -namesOfPromisedFilesDroppedAtDestination, which is
  // requesting, on the UI thread, the name of the file that will be written
  // by a drag operation. To know the name of this file, it is necessary to
  // query the filesystem before returning, which will block the UI thread.
  base::ScopedAllowBlocking allow_blocking;
  base::File file(content::CreateFileForDrop(out_file_path));
  if (!file.IsValid()) {
    *out_file_path = base::FilePath();
    return true;
  }

  if (download_url.is_valid() && web_contents_) {
    auto drag_file_downloader = std::make_unique<DragDownloadFile>(
        *out_file_path, std::move(file), download_url,
        content::Referrer(web_contents_->GetLastCommittedURL(),
                          drop_data.referrer_policy),
        web_contents_->GetEncoding(), source_origin, web_contents_);

    DragDownloadFile* downloader = drag_file_downloader.get();
    // The finalizer will take care of closing and deletion.
    downloader->Start(
        new PromiseFileFinalizer(std::move(drag_file_downloader)));
  } else {
    // The writer will take care of closing and deletion.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&PromiseWriterHelper, drop_data, std::move(file)));
  }

  // The DragDownloadFile constructor may have altered the value of
  // |*out_file_path| if, say, an existing file at the drop site has the same
  // name. Return the actual name that was used to write the file.
  *out_file_path = file_path;
  return true;
}

void WebContentsViewMac::EndDrag(uint32_t drag_operation,
                                 const gfx::PointF& local_point,
                                 const gfx::PointF& screen_point) {
  [drag_dest_ endDrag];

  web_contents_->SystemDragEnded(drag_source_start_rwh_.get());

  // |localPoint| and |screenPoint| are in the root coordinate space, for
  // non-root RenderWidgetHosts they need to be transformed.
  gfx::PointF transformed_point = local_point;
  gfx::PointF transformed_screen_point = screen_point;
  if (drag_source_start_rwh_ && web_contents_->GetRenderWidgetHostView()) {
    content::RenderWidgetHostViewBase* contentsViewBase =
        static_cast<content::RenderWidgetHostViewBase*>(
            web_contents_->GetRenderWidgetHostView());
    content::RenderWidgetHostViewBase* dragStartViewBase =
        static_cast<content::RenderWidgetHostViewBase*>(
            drag_source_start_rwh_->GetView());
    contentsViewBase->TransformPointToCoordSpaceForView(
        local_point, dragStartViewBase, &transformed_point);
    contentsViewBase->TransformPointToCoordSpaceForView(
        screen_point, dragStartViewBase, &transformed_screen_point);
  }

  web_contents_->DragSourceEndedAt(
      transformed_point.x(), transformed_point.y(),
      transformed_screen_point.x(), transformed_screen_point.y(),
      static_cast<ui::mojom::DragOperation>(drag_operation),
      drag_source_start_rwh_.get());
}

void WebContentsViewMac::DraggingEntered(DraggingInfoPtr dragging_info,
                                         DraggingEnteredCallback callback) {
  uint32_t result = 0;
  DraggingEntered(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

void WebContentsViewMac::DraggingUpdated(DraggingInfoPtr dragging_info,
                                         DraggingUpdatedCallback callback) {
  uint32_t result = false;
  DraggingUpdated(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

void WebContentsViewMac::PerformDragOperation(
    DraggingInfoPtr dragging_info,
    PerformDragOperationCallback callback) {
  bool result = false;
  PerformDragOperation(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

void WebContentsViewMac::DragPromisedFileTo(
    const base::FilePath& file_path,
    const DropData& drop_data,
    const GURL& download_url,
    const url::Origin& source_origin,
    DragPromisedFileToCallback callback) {
  base::FilePath actual_file_path;
  DragPromisedFileTo(file_path, drop_data, download_url, source_origin,
                     &actual_file_path);
  std::move(callback).Run(actual_file_path);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewMac, ViewsHostableView:

void WebContentsViewMac::ViewsHostableAttach(
    ViewsHostableView::Host* views_host) {
  views_host_ = views_host;
  // Create an NSView in the target process, if one exists.
  auto* remote_cocoa_application = views_host_->GetRemoteCocoaApplication();
  if (remote_cocoa_application) {
    mojo::PendingAssociatedRemote<remote_cocoa::mojom::WebContentsNSViewHost>
        host;
    remote_ns_view_host_receiver_.Bind(
        host.InitWithNewEndpointAndPassReceiver());
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::WebContentsNSView>
        ns_view_receiver = remote_ns_view_.BindNewEndpointAndPassReceiver();

    // Cast from mojo::PendingAssociatedRemote<mojom::WebContentsNSViewHost> and
    // mojo::PendingAssociatedReceiver<remote_cocoa::mojom::WebContentsNSView>
    // to the public interfaces accepted by the application.
    // TODO(ccameron): Remove the need for this cast.
    // https://crbug.com/888290
    mojo::PendingAssociatedRemote<remote_cocoa::mojom::StubInterface> stub_host(
        host.PassHandle(), 0);
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::StubInterface>
        stub_ns_view_receiver(ns_view_receiver.PassHandle());

    remote_cocoa_application->CreateWebContentsNSView(
        ns_view_id_, std::move(stub_host), std::move(stub_ns_view_receiver));
    remote_ns_view_->SetParentNSView(views_host_->GetNSViewId());
    if (!window_controls_overlay_bounding_rect_.IsEmpty()) {
      remote_ns_view_->UpdateWindowControlsOverlay(
          window_controls_overlay_bounding_rect_);
    }

    // Because this view is being displayed from a remote process, reset the
    // in-process NSView's client pointer, so that the in-process NSView will
    // not call back into |this|.
    [GetInProcessNSView() setHost:nullptr];
  }

  // TODO(crbug.com/41442285): WebContentsNSViewBridge::SetParentView
  // will look up the parent NSView by its id, but this has been observed to
  // fail in the field, so assume that the caller handles updating the NSView
  // hierarchy.
  // in_process_ns_view_bridge_->SetParentNSView(views_host_->GetNSViewId());

  for (auto* rwhv_mac : GetChildViews()) {
    rwhv_mac->MigrateNSViewBridge(remote_cocoa_application, ns_view_id_);
    rwhv_mac->SetParentUiLayer(views_host_->GetUiLayer());
  }
}

void WebContentsViewMac::ViewsHostableDetach() {
  DCHECK(views_host_);
  // Disconnect from the remote bridge, if it exists. This will have the effect
  // of destroying the associated bridge instance with its NSView.
  if (remote_ns_view_) {
    remote_ns_view_->SetVisible(false);
    remote_ns_view_->ResetParentNSView();
    remote_ns_view_host_receiver_.reset();
    remote_ns_view_->Destroy();
    remote_ns_view_.reset();
    // Permit the in-process NSView to call back into |this| again.
    [GetInProcessNSView() setHost:this];
  }
  in_process_ns_view_bridge_->SetVisible(false);
  in_process_ns_view_bridge_->ResetParentNSView();
  views_host_ = nullptr;

  for (auto* rwhv_mac : GetChildViews()) {
    rwhv_mac->MigrateNSViewBridge(nullptr, 0);
    rwhv_mac->SetParentUiLayer(nullptr);
    rwhv_mac->SetParentAccessibilityElement(nil);
  }
}

void WebContentsViewMac::ViewsHostableSetBounds(
    const gfx::Rect& bounds_in_window) {
  // Update both the in-process and out-of-process NSViews' bounds.
  in_process_ns_view_bridge_->SetBounds(bounds_in_window);
  if (remote_ns_view_)
    remote_ns_view_->SetBounds(bounds_in_window);
}

void WebContentsViewMac::ViewsHostableSetVisible(bool visible) {
  // Update both the in-process and out-of-process NSViews' visibility.
  in_process_ns_view_bridge_->SetVisible(visible);
  if (remote_ns_view_)
    remote_ns_view_->SetVisible(visible);
}

void WebContentsViewMac::ViewsHostableMakeFirstResponder() {
  // Only make the true NSView become the first responder.
  if (remote_ns_view_)
    remote_ns_view_->MakeFirstResponder();
  else
    in_process_ns_view_bridge_->MakeFirstResponder();
}

void WebContentsViewMac::ViewsHostableSetParentAccessible(
    gfx::NativeViewAccessible parent_accessibility_element) {
  views_host_accessibility_element_ = parent_accessibility_element;
  for (auto* rwhv_mac : GetChildViews())
    rwhv_mac->SetParentAccessibilityElement(views_host_accessibility_element_);
}

gfx::NativeViewAccessible
WebContentsViewMac::ViewsHostableGetParentAccessible() {
  return views_host_accessibility_element_;
}

gfx::NativeViewAccessible
WebContentsViewMac::ViewsHostableGetAccessibilityElement() {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return nil;
  return rwhv->GetNativeViewAccessible();
}

}  // namespace content
