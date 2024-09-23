// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/features.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/aura/gesture_nav_simple.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view) {
  auto rv =
      std::make_unique<WebContentsViewAura>(web_contents, std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

class ScopedAllowBlockingForViewAura : public base::ScopedAllowBlocking {};

namespace {

using ::ui::mojom::DragOperation;

WebContentsViewAura::RenderWidgetHostViewCreateFunction
    g_create_render_widget_host_view = nullptr;

RenderWidgetHostViewAura* ToRenderWidgetHostViewAura(
    RenderWidgetHostView* view) {
  if (RenderViewHostFactory::has_factory() &&
      !RenderViewHostFactory::is_real_render_view_host()) {
    return nullptr;  // Can't cast to RenderWidgetHostViewAura in unit tests.
  }

  DCHECK(!view || !static_cast<RenderWidgetHostViewBase*>(view)
                       ->IsRenderWidgetHostViewChildFrame());
  return static_cast<RenderWidgetHostViewAura*>(view);
}

// Listens to all mouse drag events during a drag and drop and sends them to
// the renderer.
class WebDragSourceAura : public content::WebContentsObserver,
                          public aura::WindowObserver {
 public:
  WebDragSourceAura(aura::Window* window, WebContentsImpl* contents)
      : WebContentsObserver(contents), window_(window) {
    window_->AddObserver(this);
  }

  ~WebDragSourceAura() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  WebDragSourceAura(const WebDragSourceAura&) = delete;
  WebDragSourceAura& operator=(const WebDragSourceAura&) = delete;

  // content::WebContentsObserver
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    CancelDrag();
  }

  void WebContentsDestroyed() override { CancelDrag(); }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  aura::Window* window() const { return window_; }

 private:
  void CancelDrag() {
    if (!window_)
      return;

    // Cancel the drag if it is still in progress.
    aura::client::DragDropClient* dnd_client =
        aura::client::GetDragDropClient(window_->GetRootWindow());

    window_->RemoveObserver(this);
    window_ = nullptr;

    if (dnd_client && dnd_client->IsDragDropInProgress())
      dnd_client->DragCancel();
  }

  raw_ptr<aura::Window> window_;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
// Fill out the OSExchangeData with a file contents, synthesizing a name if
// necessary.
void PrepareDragForFileContents(const DropData& drop_data,
                                ui::OSExchangeDataProvider* provider) {
  std::optional<base::FilePath> filename =
      drop_data.GetSafeFilenameForImageFileContents();
  if (filename)
    provider->SetFileContents(*filename, drop_data.file_contents);
}
#endif

#if BUILDFLAG(IS_WIN)
void PrepareDragForDownload(const DropData& drop_data,
                            ui::OSExchangeDataProvider* provider,
                            WebContentsImpl* web_contents) {
  const GURL& page_url = web_contents->GetLastCommittedURL();
  const std::string& page_encoding = web_contents->GetEncoding();

  // Parse the download metadata.
  std::u16string mime_type;
  base::FilePath file_name;
  GURL download_url;
  if (!ParseDownloadMetadata(drop_data.download_metadata,
                             &mime_type,
                             &file_name,
                             &download_url))
    return;

  // Generate the file name based on both mime type and proposed file name.
  std::string default_name =
      GetContentClient()->browser()->GetDefaultDownloadName();
  base::FilePath generated_download_file_name =
      net::GenerateFileName(download_url, std::string(), std::string(),
                            base::WideToUTF8(file_name.value()),
                            base::UTF16ToUTF8(mime_type), default_name);

  // http://crbug.com/332579
  ScopedAllowBlockingForViewAura allow_file_operations;

  base::FilePath temp_dir_path;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_drag"),
                                    &temp_dir_path))
    return;

  base::FilePath download_path =
      temp_dir_path.Append(generated_download_file_name);

  // We cannot know when the target application will be done using the temporary
  // file, so schedule it to be deleted after rebooting.
  base::DeleteFileAfterReboot(download_path);
  base::DeleteFileAfterReboot(temp_dir_path);

  // Provide the data as file (CF_HDROP). A temporary download file with the
  // Zone.Identifier ADS (Alternate Data Stream) attached will be created.
  auto download_file = std::make_unique<DragDownloadFile>(
      download_path, base::File(), download_url,
      Referrer(page_url, drop_data.referrer_policy), page_encoding,
      provider->GetRendererTaintedOrigin(), web_contents);
  ui::DownloadFileInfo file_download(base::FilePath(),
                                     std::move(download_file));
  provider->SetDownloadFileInfo(&file_download);
}
#endif  // BUILDFLAG(IS_WIN)

// Returns the ClipboardFormatType to store file system files.
const ui::ClipboardFormatType& GetFileSystemFileFormatType() {
  static base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"));
  return *format;
}

// Utility to fill a ui::OSExchangeDataProvider object from DropData.
void PrepareDragData(const DropData& drop_data,
                     const url::Origin source_origin,
                     ui::OSExchangeDataProvider* provider,
                     WebContentsImpl* web_contents) {
  provider->MarkRendererTaintedFromOrigin(source_origin);
#if BUILDFLAG(IS_WIN)
  // Put download before file contents to prefer the download of a image over
  // its thumbnail link.
  if (!drop_data.download_metadata.empty())
    PrepareDragForDownload(drop_data, provider, web_contents);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over
  // a shortcut so we add it first.
  if (!drop_data.file_contents.empty())
    PrepareDragForFileContents(drop_data, provider);
#endif
  // Call SetString() before SetURL() when we actually have a custom string.
  // SetURL() will itself do SetString() when a string hasn't been set yet,
  // but we want to prefer drop_data.text.string() over the URL string if it
  // exists.
  if (drop_data.text) {
    provider->SetString(*drop_data.text);
  }
  if (drop_data.url.is_valid())
    provider->SetURL(drop_data.url, drop_data.url_title);
  if (drop_data.html && !drop_data.html->empty())
    provider->SetHtml(*drop_data.html, drop_data.html_base_url);
  if (!drop_data.filenames.empty())
    provider->SetFilenames(drop_data.filenames);
  if (!drop_data.file_system_files.empty()) {
    base::Pickle pickle;
    DropData::FileSystemFileInfo::WriteFileSystemFilesToPickle(
        drop_data.file_system_files, &pickle);
    provider->SetPickledData(GetFileSystemFileFormatType(), pickle);
  }
  if (!drop_data.custom_data.empty()) {
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(drop_data.custom_data, &pickle);
    provider->SetPickledData(ui::ClipboardFormatType::DataTransferCustomType(),
                             pickle);
  }
}

#if BUILDFLAG(IS_WIN)
// Function returning whether this drop target should extract virtual file data
// from the data store.
// (1) As with real files, only add virtual files if the drag did not originate
// in the renderer process. Without this, if an anchor element is dragged and
// then dropped on the same page, the browser will navigate to the URL
// referenced by the anchor. That is because virtual ".url" file data
// (internet shortcut) is added to the data object on drag start, and if
// script doesn't handle the drop, the browser behaves just as if a .url file
// were dragged in from the desktop. Filtering out virtual files if the drag
// is renderer tainted also prevents the possibility of a compromised renderer
// gaining access to the backing temp file paths.
// (2) Even if the drag is not renderer tainted, also exclude virtual files
// if the UniformResourceLocatorW clipboard format is found in the data object.
// Drags initiated in the browser process, such as dragging a bookmark from
// the bookmark bar, will add a virtual .url file to the data object using the
// CFSTR_FILEDESCRIPTORW/CFSTR_FILECONTENTS formats, which represents an
// internet shortcut intended to be  dropped on the desktop. But this causes a
// regression in the behavior of the extensions page (see
// https://crbug.com/963392). The primary scenario for introducing virtual file
// support was for dragging items out of Outlook.exe for upload to a file
// hosting service. The Outlook drag source does not add url data to the data
// object.
// TODO(crbug.com/41456054): DragDrop: Extend virtual filename support
// to DropData, for parity with real filename support.
// TODO(crbug.com/41459545): Drag and drop: Should support both virtual
// file and url data on drop.
bool ShouldIncludeVirtualFiles(const DropData& drop_data) {
  return !drop_data.did_originate_from_renderer && drop_data.url.is_empty();
}
#endif

// Utilities to convert between blink::DragOperationsMask and
// ui::DragDropTypes.
int ConvertFromDragOperationsMask(blink::DragOperationsMask ops) {
  int drag_op = ui::DragDropTypes::DRAG_NONE;
  if (ops & blink::kDragOperationCopy)
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  if (ops & blink::kDragOperationMove)
    drag_op |= ui::DragDropTypes::DRAG_MOVE;
  if (ops & blink::kDragOperationLink)
    drag_op |= ui::DragDropTypes::DRAG_LINK;
  return drag_op;
}

blink::DragOperationsMask ConvertToDragOperationsMask(int drag_op) {
  int web_drag_op = blink::kDragOperationNone;
  if (drag_op & ui::DragDropTypes::DRAG_COPY)
    web_drag_op |= blink::kDragOperationCopy;
  if (drag_op & ui::DragDropTypes::DRAG_MOVE)
    web_drag_op |= blink::kDragOperationMove;
  if (drag_op & ui::DragDropTypes::DRAG_LINK)
    web_drag_op |= blink::kDragOperationLink;
  return static_cast<blink::DragOperationsMask>(web_drag_op);
}

GlobalRoutingID GetRenderViewHostID(RenderViewHost* rvh) {
  return GlobalRoutingID(rvh->GetProcess()->GetID(), rvh->GetRoutingID());
}

// Returns the host window for |window|, or nullpr if it has no host window.
aura::Window* GetHostWindow(aura::Window* window) {
  aura::Window* host_window = window->GetProperty(aura::client::kHostWindowKey);
  if (host_window)
    return host_window;
  return window->parent();
}

}  // namespace

WebContentsViewAura::DropMetadata::DropMetadata(
    const ui::DropTargetEvent& event) {
  localized_location = event.location_f();
  root_location = event.root_location_f();
  source_operations = event.source_operations();
  flags = event.flags();
}

WebContentsViewAura::OnPerformingDropContext::OnPerformingDropContext(
    RenderWidgetHostImpl* target_rwh,
    std::unique_ptr<DropData> drop_data,
    DropMetadata drop_metadata,
    std::unique_ptr<ui::OSExchangeData> data,
    base::ScopedClosureRunner end_drag_runner,
    std::optional<gfx::PointF> transformed_pt,
    gfx::PointF screen_pt)
    : target_rwh(target_rwh->GetWeakPtr()),
      drop_data(std::move(drop_data)),
      drop_metadata(drop_metadata),
      data(std::move(data)),
      end_drag_runner(std::move(end_drag_runner)),
      transformed_pt(std::move(transformed_pt)),
      screen_pt(screen_pt) {}

WebContentsViewAura::OnPerformingDropContext::OnPerformingDropContext(
    OnPerformingDropContext&&) = default;

WebContentsViewAura::OnPerformingDropContext::~OnPerformingDropContext() =
    default;

#if BUILDFLAG(IS_WIN)
// A web contents observer that watches for navigations while an async drop
// operation is in progress during virtual file data retrieval and temp file
// creation. Navigations may cause completion of the drop to be disallowed.
class WebContentsViewAura::AsyncDropNavigationObserver
    : public WebContentsObserver {
 public:
  explicit AsyncDropNavigationObserver(WebContents* watched_contents);

  AsyncDropNavigationObserver(const AsyncDropNavigationObserver&) = delete;
  AsyncDropNavigationObserver& operator=(const AsyncDropNavigationObserver&) =
      delete;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Was a navigation observed while the async drop was being processed that
  // should disallow the drop?
  bool drop_allowed() const { return drop_allowed_; }

 private:
  bool drop_allowed_ = true;
};

WebContentsViewAura::AsyncDropNavigationObserver::AsyncDropNavigationObserver(
    WebContents* watched_contents)
    : WebContentsObserver(watched_contents) {}

void WebContentsViewAura::AsyncDropNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  // This method is called every time any navigation completes in the observed
  // web contents, including subframe navigations. In the case of a subframe
  // navigation, we can't readily determine on the browser process side if the
  // navigated subframe is the intended drop target. Err on the side of security
  // and disallow the drop if any navigation commits to a different url.
  // Note that this method is called twice for prerendering, one when the
  // prerendering starts and the document is created and starts loading and one
  // when the prerendered document has been activated and shown to the user.
  // We should not disallow the drop for the former prerendering state.
  if (navigation_request->HasCommitted() &&
      (navigation_request->GetURL() !=
       navigation_request->GetPreviousMainFrameURL()) &&
      navigation_request->GetRenderFrameHost()->GetLifecycleState() !=
          RenderFrameHost::LifecycleState::kPrerendering) {
    drop_allowed_ = false;
  }
}

// Deletes registered temp files asynchronously when the object goes out of
// scope (when the WebContentsViewAura is deleted on tab closure).
class WebContentsViewAura::AsyncDropTempFileDeleter {
 public:
  AsyncDropTempFileDeleter() = default;

  AsyncDropTempFileDeleter(const AsyncDropTempFileDeleter&) = delete;
  AsyncDropTempFileDeleter& operator=(const AsyncDropTempFileDeleter&) = delete;

  ~AsyncDropTempFileDeleter();
  void RegisterFile(const base::FilePath& path);

 private:
  void DeleteAllFilesAsync() const;
  void DeleteFileAsync(const base::FilePath& path) const;

  std::vector<base::FilePath> scoped_files_to_delete_;
};

WebContentsViewAura::AsyncDropTempFileDeleter::~AsyncDropTempFileDeleter() {
  DeleteAllFilesAsync();
}

void WebContentsViewAura::AsyncDropTempFileDeleter::RegisterFile(
    const base::FilePath& path) {
  scoped_files_to_delete_.push_back(path);
}

void WebContentsViewAura::AsyncDropTempFileDeleter::DeleteAllFilesAsync()
    const {
  for (const auto& path : scoped_files_to_delete_)
    DeleteFileAsync(path);
}

void WebContentsViewAura::AsyncDropTempFileDeleter::DeleteFileAsync(
    const base::FilePath& path) const {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::GetDeleteFileCallback(std::move(path)));
}
#endif

class WebContentsViewAura::WindowObserver
    : public aura::WindowObserver, public aura::WindowTreeHostObserver {
 public:
  explicit WindowObserver(WebContentsViewAura* view) : view_(view) {
    view_->window_->AddObserver(this);
  }

  WindowObserver(const WindowObserver&) = delete;
  WindowObserver& operator=(const WindowObserver&) = delete;

  ~WindowObserver() override {
    view_->window_->RemoveObserver(this);
    if (view_->window_->GetHost())
      view_->window_->GetHost()->RemoveObserver(this);
    if (host_window_)
      host_window_->RemoveObserver(this);
  }

  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override {
    if (window != view_->window_.get())
      return;

    aura::Window* const host_window = GetHostWindow(window);

    if (host_window_)
      host_window_->RemoveObserver(this);

    host_window_ = host_window;
    if (host_window)
      host_window->AddObserver(this);
  }

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    DCHECK(window == host_window_ || window == view_->window_.get());
    if (!ShouldNotifyOfBoundsChanges())
      return;

    if (pending_window_changes_) {
      pending_window_changes_->window_bounds_changed = true;
      if (old_bounds.origin() != new_bounds.origin())
        pending_window_changes_->window_origin_changed = true;
      return;
    }
    ProcessWindowBoundsChange(old_bounds.origin() != new_bounds.origin());
  }

  void OnWindowDestroying(aura::Window* window) override {
    if (window == host_window_) {
      host_window_->RemoveObserver(this);
      host_window_ = nullptr;
    }
  }

  void OnWindowAddedToRootWindow(aura::Window* window) override {
    if (window != view_->window_.get())
      return;

    window->GetHost()->AddObserver(this);
    if (view_->web_contents_->IsBeingCaptured())
      view_->video_capture_lock_ = window->GetHost()->CreateVideoCaptureLock();
  }

  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    if (window == view_->window_.get()) {
      window->GetHost()->RemoveObserver(this);
      pending_window_changes_.reset();
      view_->video_capture_lock_.reset();
    }
  }

  // Overridden WindowTreeHostObserver:
  void OnHostWillProcessBoundsChange(aura::WindowTreeHost* host) override {
    if (!ShouldNotifyOfBoundsChanges())
      return;

    DCHECK(!pending_window_changes_);
    pending_window_changes_ = std::make_unique<PendingWindowChanges>();
  }

  void OnHostDidProcessBoundsChange(aura::WindowTreeHost* host) override {
    if (!ShouldNotifyOfBoundsChanges())
      return;

    if (!pending_window_changes_)
      return;  // Happens if added to a new host during bounds change.

    if (pending_window_changes_->window_bounds_changed)
      ProcessWindowBoundsChange(pending_window_changes_->window_origin_changed);
    else if (pending_window_changes_->host_moved)
      ProcessHostMovedInPixels();
    pending_window_changes_.reset();
  }

  void OnHostMovedInPixels(aura::WindowTreeHost* host) override {
    if (!ShouldNotifyOfBoundsChanges())
      return;

    if (pending_window_changes_) {
      pending_window_changes_->host_moved = true;
      return;
    }
    ProcessHostMovedInPixels();
  }

 private:
  bool ShouldNotifyOfBoundsChanges() const {
    // Do not notify of bounds changes for guests as guests' window bounds are
    // supposed to come from its embedder.
    // We also do not handle bounds changes during destruction.
    return !view_->web_contents_->IsBeingDestroyed() &&
           !view_->web_contents_->IsGuest();
  }

  // Used to avoid multiple calls to SendScreenRects(). In particular, when
  // WindowTreeHost changes its size, it's entirely likely the aura::Windows
  // will change as well. When OnHostWillProcessBoundsChange() is called,
  // |pending_window_changes_| is created and any changes are set in it.
  // In OnHostDidProcessBoundsChange() is called, all accumulated changes are
  // applied.
  struct PendingWindowChanges {
    // Set to true if OnWindowBoundsChanged() is called.
    bool window_bounds_changed = false;

    // Set to true if OnWindowBoundsChanged() is called *and* the origin of the
    // window changed.
    bool window_origin_changed = false;

    // Set to true if OnHostMovedInPixels() is called.
    bool host_moved = false;
  };

  void ProcessWindowBoundsChange(bool did_origin_change) {
    DCHECK(ShouldNotifyOfBoundsChanges());
    SendScreenRects();
    if (did_origin_change) {
      TouchSelectionControllerClientAura* selection_controller_client =
          view_->GetSelectionControllerClient();
      if (selection_controller_client)
        selection_controller_client->OnWindowMoved();
    }
  }

  void ProcessHostMovedInPixels() {
    DCHECK(ShouldNotifyOfBoundsChanges());
    // NOTE: this function is *not* called if OnHostWillProcessBoundsChange()
    // *and* the bounds changes (OnWindowBoundsChanged() is called).
    TRACE_EVENT1(
        "ui", "WebContentsViewAura::WindowObserver::OnHostMovedInPixels",
        "new_origin_in_pixels",
        view_->window_->GetHost()->GetBoundsInPixels().origin().ToString());
    SendScreenRects();
  }

  void SendScreenRects() { view_->web_contents_->SendScreenRects(); }

  raw_ptr<WebContentsViewAura> view_;

  // The parent window that hosts the constrained windows. We cache the old host
  // view so that we can unregister when it's not the parent anymore.
  raw_ptr<aura::Window> host_window_ = nullptr;

  std::unique_ptr<PendingWindowChanges> pending_window_changes_;
};

// static
void WebContentsViewAura::InstallCreateHookForTests(
    RenderWidgetHostViewCreateFunction create_render_widget_host_view) {
  CHECK_EQ(nullptr, g_create_render_widget_host_view);
  g_create_render_widget_host_view = create_render_widget_host_view;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, public:

WebContentsViewAura::WebContentsViewAura(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      drag_dest_delegate_(nullptr),
      current_rvh_for_drag_(ChildProcessHost::kInvalidUniqueID,
                            MSG_ROUTING_NONE),
      drag_in_progress_(false),
      init_rwhv_with_null_parent_for_testing_(false) {}

WebContentsViewAura::~WebContentsViewAura() {
  if (!window_)
    return;

  window_observer_.reset();

  // Window needs a valid delegate during its destructor, so we explicitly
  // delete it here.
  window_.reset();
}

void WebContentsViewAura::SetDelegateForTesting(
    std::unique_ptr<WebContentsViewDelegate> delegate) {
  delegate_ = std::move(delegate);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, private:

void WebContentsViewAura::PrepareDropData(
    DropData* drop_data,
    const ui::OSExchangeData& data) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/256022714): Using `IsRendererTainted()` breaks the Files app. Always
  // setting this to false is currently believed to be safe-ish because ChromeOS
  // separates URL and filename metadata and does not implement the DownloadURL
  // protocol.
  drop_data->did_originate_from_renderer = false;
#else
  drop_data->did_originate_from_renderer = data.IsRendererTainted();
#endif
  drop_data->is_from_privileged = data.IsFromPrivileged();

  if (std::optional<std::u16string> string = data.GetString();
      string.has_value() && !string->empty()) {
    drop_data->text = std::move(*string);
  }

  if (std::optional<ui::OSExchangeData::UrlInfo> url = data.GetURLAndTitle(
          ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
      url.has_value() && url->url.is_valid()) {
    drop_data->url = std::move(url->url);
    drop_data->url_title = std::move(url->title);
  }

  if (std::optional<ui::OSExchangeData::HtmlInfo> html = data.GetHtml();
      html.has_value()) {
    drop_data->html = html->html;
    if (html->base_url.is_valid()) {
      drop_data->html_base_url = html->base_url;
    }
  }

  if (std::optional<std::vector<ui::FileInfo>> filenames = data.GetFilenames();
      filenames.has_value()) {
    drop_data->filenames = filenames.value();
  } else {
    // Only add FileContents if Filenames is empty to avoid duplicates
    // (https://crbug.com/1251482). We prefer filenames since it supports
    // multiple files and does not send all file data upfront. Do not add
    // FileContents if this is a tainted-cross-origin same-page image
    // (https://crbug.com/1264873).
    bool access_allowed =
        // Drag began in this top-level WebContents, and image access is allowed
        // (not cross-origin).
        drag_security_info_.IsImageAccessibleFromFrame();
    if (access_allowed) {
      if (std::optional<ui::OSExchangeData::FileContentsInfo> file_contents =
              data.GetFileContents();
          file_contents.has_value()) {
        drop_data->file_contents = std::move(file_contents->file_contents);
        drop_data->file_contents_image_accessible = true;
        drop_data->file_contents_source_url =
            GURL(ui::FilePathToFileURL(file_contents->filename));
        base::FilePath::StringType extension =
            file_contents->filename.Extension();
        if (!extension.empty()) {
          drop_data->file_contents_filename_extension = extension.substr(1);
        }
      }
    }
  }

#if BUILDFLAG(IS_WIN)
  // Get a list of virtual files for later retrieval when a drop is performed
  // (will return empty vector if there are any non-virtual files in the data
  // store).
  if (ShouldIncludeVirtualFiles(*drop_data)) {
    if (std::optional<std::vector<ui::FileInfo>> virtual_filenames =
            data.GetVirtualFilenames();
        virtual_filenames.has_value()) {
      base::ranges::move(virtual_filenames.value(),
                         std::back_inserter(drop_data->filenames));
    }
  }
#endif

  if (std::optional<base::Pickle> pickle =
          data.GetPickledData(GetFileSystemFileFormatType());
      pickle.has_value()) {
    std::vector<DropData::FileSystemFileInfo> file_system_files;
    if (DropData::FileSystemFileInfo::ReadFileSystemFilesFromPickle(
            pickle.value(), &file_system_files)) {
      drop_data->file_system_files = file_system_files;
    }
  }

  if (std::optional<base::Pickle> pickle = data.GetPickledData(
          ui::ClipboardFormatType::DataTransferCustomType());
      pickle.has_value()) {
    if (std::optional<std::unordered_map<std::u16string, std::u16string>>
            maybe_custom_data = ui::ReadCustomDataIntoMap(pickle.value());
        maybe_custom_data.has_value()) {
      drop_data->custom_data = std::move(maybe_custom_data.value());
    }
  }
}

void WebContentsViewAura::EndDrag(
    base::WeakPtr<RenderWidgetHostImpl> source_rwh_weak_ptr,
    DragOperation op) {
  drag_security_info_.OnDragEnded();

  if (!web_contents_)
    return;

  // It is OK for source_rwh to be null.
  RenderWidgetHost* source_rwh = source_rwh_weak_ptr.get();

  aura::Window* window = GetContentNativeView();
  CHECK(window);

  gfx::PointF screen_loc =
      gfx::PointF(display::Screen::GetScreen()->GetCursorScreenPoint());
  gfx::PointF client_loc = screen_loc;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client)
    screen_position_client->ConvertPointFromScreen(window, &client_loc);

  // |client_loc| is in the root coordinate space, for non-root
  // RenderWidgetHosts it needs to be transformed.
  gfx::PointF transformed_point = client_loc;
  if (source_rwh && web_contents_->GetRenderWidgetHostView()) {
    static_cast<RenderWidgetHostViewBase*>(
        web_contents_->GetRenderWidgetHostView())
        ->TransformPointToCoordSpaceForView(
            client_loc,
            static_cast<RenderWidgetHostViewBase*>(source_rwh->GetView()),
            &transformed_point);
  }

  web_contents_->DragSourceEndedAt(transformed_point.x(), transformed_point.y(),
                                   screen_loc.x(), screen_loc.y(), op,
                                   source_rwh);

  web_contents_->SystemDragEnded(source_rwh);
}

void WebContentsViewAura::InstallOverscrollControllerDelegate(
    RenderWidgetHostViewAura* view) {
  if (!base::FeatureList::IsEnabled(features::kOverscrollHistoryNavigation))
    return;

  if (!gesture_nav_simple_)
    gesture_nav_simple_ = std::make_unique<GestureNavSimple>(web_contents_);
  if (view) {
    view->overscroll_controller()->set_delegate(
        gesture_nav_simple_->GetWeakPtr());
  }
}

ui::TouchSelectionController* WebContentsViewAura::GetSelectionController()
    const {
  RenderWidgetHostViewAura* view =
      ToRenderWidgetHostViewAura(web_contents_->GetRenderWidgetHostView());
  return view ? view->selection_controller() : nullptr;
}

TouchSelectionControllerClientAura*
WebContentsViewAura::GetSelectionControllerClient() const {
  RenderWidgetHostViewAura* view =
      ToRenderWidgetHostViewAura(web_contents_->GetRenderWidgetHostView());
  return view ? view->selection_controller_client() : nullptr;
}

gfx::NativeView WebContentsViewAura::GetRenderWidgetHostViewParent() const {
  if (init_rwhv_with_null_parent_for_testing_)
    return nullptr;
  return window_.get();
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsView implementation:

gfx::NativeView WebContentsViewAura::GetNativeView() const {
  return window_.get();
}

gfx::NativeView WebContentsViewAura::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : nullptr;
}

gfx::NativeWindow WebContentsViewAura::GetTopLevelNativeWindow() const {
  gfx::NativeWindow window = window_->GetToplevelWindow();
  if (window)
    return window;
  if (delegate_)
    return delegate_->GetNativeWindow();
  return nullptr;
}

gfx::Rect WebContentsViewAura::GetContainerBounds() const {
  return GetNativeView()->GetBoundsInScreen();
}

void WebContentsViewAura::Focus() {
  if (delegate_)
    delegate_->ResetStoredFocus();

  if (delegate_ && delegate_->Focus())
    return;

  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->Focus();
}

void WebContentsViewAura::SetInitialFocus() {
  if (delegate_)
    delegate_->ResetStoredFocus();

  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar();
  else
    Focus();
}

void WebContentsViewAura::StoreFocus() {
  if (delegate_)
    delegate_->StoreFocus();
}

void WebContentsViewAura::RestoreFocus() {
  if (delegate_ && delegate_->RestoreFocus())
    return;
  SetInitialFocus();
}

void WebContentsViewAura::FocusThroughTabTraversal(bool reverse) {
  if (delegate_)
    delegate_->ResetStoredFocus();

  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewAura::GetDropData() const {
  return current_drag_data_.get();
}

gfx::Rect WebContentsViewAura::GetViewBounds() const {
  return GetNativeView()->GetBoundsInScreen();
}

void WebContentsViewAura::CreateAuraWindow(aura::Window* context) {
  DCHECK(aura::Env::HasInstance());
  DCHECK(!window_);
  window_ =
      std::make_unique<aura::Window>(this, aura::client::WINDOW_TYPE_CONTROL);
  window_->set_owned_by_parent(false);
  window_->SetName("WebContentsViewAura");
  window_->Init(ui::LAYER_NOT_DRAWN);
  aura::Window* root_window = context ? context->GetRootWindow() : nullptr;
  if (root_window) {
    // There are places where there is no context currently because object
    // hierarchies are built before they're attached to a Widget. (See
    // views::WebView as an example; GetWidget() returns NULL at the point
    // where we are created.)
    //
    // It should be OK to not set a default parent since such users will
    // explicitly add this WebContentsViewAura to their tree after they create
    // us.
    aura::client::ParentWindowWithContext(window_.get(), root_window,
                                          root_window->GetBoundsInScreen(),
                                          display::kInvalidDisplayId);
  }
  window_->layer()->SetMasksToBounds(true);
  window_->TrackOcclusionState();

  window_observer_ = std::make_unique<WindowObserver>(this);
}

void WebContentsViewAura::UpdateWebContentsVisibility() {
  if (web_contents_->IsBeingDestroyed())
    return;

  web_contents_->UpdateWebContentsVisibility(GetVisibility());
}

Visibility WebContentsViewAura::GetVisibility() const {
  if (window_->GetOcclusionState() == aura::Window::OcclusionState::VISIBLE)
    return Visibility::VISIBLE;

  if (window_->GetOcclusionState() == aura::Window::OcclusionState::OCCLUDED)
    return Visibility::OCCLUDED;

  DCHECK_EQ(window_->GetOcclusionState(), aura::Window::OcclusionState::HIDDEN);
  return Visibility::HIDDEN;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsView implementation:

void WebContentsViewAura::CreateView(gfx::NativeView context) {
  CreateAuraWindow(context);

  // delegate_->GetDragDestDelegate() creates a new delegate on every call.
  // Hence, we save a reference to it locally. Similar model is used on other
  // platforms as well.
  if (delegate_)
    drag_dest_delegate_ = delegate_->GetDragDestDelegate();
}

RenderWidgetHostViewBase* WebContentsViewAura::CreateViewForWidget(
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

  RenderWidgetHostViewAura* view =
      g_create_render_widget_host_view
          ? g_create_render_widget_host_view(render_widget_host)
          : new RenderWidgetHostViewAura(render_widget_host);
  view->InitAsChild(GetRenderWidgetHostViewParent());

  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(render_widget_host);

  if (!host_impl->is_hidden())
    view->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::client::SetDragDropDelegate(view->GetNativeView(), this);

  if (view->overscroll_controller() &&
      (!web_contents_->GetDelegate() ||
       web_contents_->GetDelegate()->CanOverscrollContent())) {
    InstallOverscrollControllerDelegate(view);
  }

  return view;
}

RenderWidgetHostViewBase* WebContentsViewAura::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  return new RenderWidgetHostViewAura(render_widget_host);
}

void WebContentsViewAura::SetPageTitle(const std::u16string& title) {
  window_->SetTitle(title);
  aura::Window* child_window = GetContentNativeView();
  if (child_window)
    child_window->SetTitle(title);
}

void WebContentsViewAura::RenderViewReady() {}

void WebContentsViewAura::RenderViewHostChanged(RenderViewHost* old_host,
                                                RenderViewHost* new_host) {
  WebContentsDelegate* delegate = web_contents_->GetDelegate();
  SetOverscrollControllerEnabled(!delegate || delegate->CanOverscrollContent());
}

void WebContentsViewAura::SetOverscrollControllerEnabled(bool enabled) {
  RenderWidgetHostViewAura* view =
      ToRenderWidgetHostViewAura(web_contents_->GetRenderWidgetHostView());
  if (view)
    view->SetOverscrollControllerEnabled(enabled);
  if (enabled)
    InstallOverscrollControllerDelegate(view);
  else
    gesture_nav_simple_.reset();
}

void WebContentsViewAura::OnCapturerCountChanged() {
  if (web_contents_->IsBeingCaptured()) {
    if (!video_capture_lock_ && window_->GetHost())
      video_capture_lock_ = window_->GetHost()->CreateVideoCaptureLock();
  } else {
    video_capture_lock_.reset();
  }
}

void WebContentsViewAura::FullscreenStateChanged(bool is_fullscreen) {}

void WebContentsViewAura::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

BackForwardTransitionAnimationManager*
WebContentsViewAura::GetBackForwardTransitionAnimationManager() {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, RenderViewHostDelegateView implementation:

void WebContentsViewAura::ShowContextMenu(RenderFrameHost& render_frame_host,
                                          const ContextMenuParams& params) {
  TouchSelectionControllerClientAura* selection_controller_client =
      GetSelectionControllerClient();
  if (selection_controller_client &&
      selection_controller_client->HandleContextMenu(params)) {
    return;
  }

  if (delegate_) {
    delegate_->ShowContextMenu(render_frame_host, params);
    // WARNING: we may have been deleted during the call to ShowContextMenu().
  }
}

void WebContentsViewAura::StartDragging(
    const DropData& drop_data,
    const url::Origin& source_origin,
    blink::DragOperationsMask operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  aura::Window* root_window = GetNativeView()->GetRootWindow();
  if (!aura::client::GetDragDropClient(root_window)) {
    web_contents_->SystemDragEnded(source_rwh);
    return;
  }

  // Grab a weak pointer to the RenderWidgetHost, since it can be destroyed
  // during the drag and drop nested run loop in StartDragAndDrop.
  // For example, the RenderWidgetHost can be deleted if a cross-process
  // transfer happens while dragging, since the RenderWidgetHost is deleted in
  // that case.
  base::WeakPtr<RenderWidgetHostImpl> source_rwh_weak_ptr =
      source_rwh->GetWeakPtr();
  base::WeakPtr<WebContentsViewAura> weak_this = weak_ptr_factory_.GetWeakPtr();

  drag_security_info_.OnDragInitiated(source_rwh, drop_data);

  ui::TouchSelectionController* selection_controller = GetSelectionController();
  if (selection_controller)
    selection_controller->HideAndDisallowShowingAutomatically();
  std::unique_ptr<ui::OSExchangeDataProvider> provider =
      ui::OSExchangeDataProviderFactory::CreateProvider();
  PrepareDragData(drop_data, source_origin, provider.get(), web_contents_);

  auto data = std::make_unique<ui::OSExchangeData>(std::move(provider));
  data->SetSource(std::make_unique<ui::DataTransferEndpoint>(
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ui::DataTransferEndpointOptions{
          .off_the_record =
              web_contents_->GetBrowserContext()->IsOffTheRecord()}));
  WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (delegate && delegate->IsPrivileged())
    data->MarkAsFromPrivileged();

  if (!image.isNull())
    data->provider().SetDragImage(image, cursor_offset);

  // TODO(crbug.com/40825138): The param `drag_obj_rect` is unused.

  std::unique_ptr<WebDragSourceAura> drag_source(
      new WebDragSourceAura(GetNativeView(), web_contents_));

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  DragOperation result_op;
  {
    gfx::NativeView content_native_view = GetContentNativeView();
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    result_op =
        aura::client::GetDragDropClient(root_window)
            ->StartDragAndDrop(std::move(data), root_window,
                               content_native_view, event_info.location,
                               ConvertFromDragOperationsMask(operations),
                               event_info.source);
  }

  // Bail out immediately if the contents view window is gone. Note that it is
  // not safe to access any class members in this case since |this| may already
  // be destroyed. The local variable |drag_source| will still be valid though,
  // so we can use it to determine if the window is gone.
  if (!drag_source->window()) {
    // Note that in this case, we don't need to call SystemDragEnded() since the
    // renderer is going away.
    return;
  }

  // |this| should still be alive at this point.
  CHECK(weak_this, base::NotFatalUntil::M130);

  // If drag is still in progress that means we haven't received drop targeting
  // callback yet. So we have to make sure to delay calling EndDrag until drop
  // is done.
  if (!drag_in_progress_) {
    EndDrag(std::move(source_rwh_weak_ptr), result_op);
  } else {
    end_drag_runner_.ReplaceClosure(
        base::BindOnce(&WebContentsViewAura::EndDrag, std::move(weak_this),
                       std::move(source_rwh_weak_ptr), result_op));
  }
}

void WebContentsViewAura::UpdateDragOperation(DragOperation operation,
                                              bool document_is_handling_drag) {
  // This asynchronous update may arrive after a drop has already been cancelled
  // or completed, in which case `current_drag_data_` will have been reset.
  if (current_drag_data_) {
    current_drag_data_->operation = operation;
    current_drag_data_->document_is_handling_drag = document_is_handling_drag;
  }
}

void WebContentsViewAura::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewAura::LostFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

void WebContentsViewAura::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      !web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse) &&
      delegate_.get()) {
    delegate_->TakeFocus(reverse);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::WindowDelegate implementation:

gfx::Size WebContentsViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size WebContentsViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void WebContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(new_bounds.size());

  // Constrained web dialogs, need to be kept centered over our content area.
  for (size_t i = 0; i < window_->children().size(); i++) {
    if (window_->children()[i]->GetProperty(
            aura::client::kConstrainedWindowKey)) {
      gfx::Rect bounds = window_->children()[i]->bounds();
      bounds.set_origin(
          gfx::Point((new_bounds.width() - bounds.width()) / 2,
                     (new_bounds.height() - bounds.height()) / 2));
      window_->children()[i]->SetBounds(bounds);
    }
  }
}

gfx::NativeCursor WebContentsViewAura::GetCursor(const gfx::Point& point) {
  return gfx::NativeCursor{};
}

int WebContentsViewAura::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool WebContentsViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool WebContentsViewAura::CanFocus() {
  // Do not take the focus if the render widget host view aura is gone or
  // is in the process of shutting down because neither the view window nor
  // this window can handle key events.
  RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
      web_contents_->GetRenderWidgetHostView());
  if (view == nullptr || view->IsClosing()) {
    return false;
  }

  if (web_contents_->ShouldIgnoreInputEvents()) {
    return false;
  }

  return true;
}

void WebContentsViewAura::OnCaptureLost() {
}

void WebContentsViewAura::OnPaint(const ui::PaintContext& context) {
}

void WebContentsViewAura::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void WebContentsViewAura::OnWindowDestroying(aura::Window* window) {}

void WebContentsViewAura::OnWindowDestroyed(aura::Window* window) {
}

void WebContentsViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

void WebContentsViewAura::OnWindowOcclusionChanged(
    aura::Window::OcclusionState old_occlusion_state,
    aura::Window::OcclusionState new_occlusion_state) {
  UpdateWebContentsVisibility();
}

bool WebContentsViewAura::HasHitTestMask() const {
  return false;
}

void WebContentsViewAura::GetHitTestMask(SkPath* mask) const {}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, ui::EventHandler implementation:

void WebContentsViewAura::OnKeyEvent(ui::KeyEvent* event) {
}

void WebContentsViewAura::OnMouseEvent(ui::MouseEvent* event) {
  if (!web_contents_->GetDelegate())
    return;

  if (event->type() == ui::EventType::kMousePressed) {
    // Linux window managers like to handle raise-on-click themselves.  If we
    // raise-on-click manually, this may override user settings that prevent
    // focus-stealing.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
    // It is possible for the web-contents to be destroyed while it is being
    // activated. Use a weak-ptr to track whether that happened or not.
    // More in https://crbug.com/1040725
    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    web_contents_->GetDelegate()->ActivateContents(web_contents_);
    if (!weak_this)
      return;
#endif
  }

  web_contents_->GetDelegate()->ContentsMouseEvent(web_contents_, *event);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::client::DragDropDelegate implementation:

void WebContentsViewAura::DragEnteredCallback(
    DropMetadata drop_metadata,
    std::unique_ptr<DropData> drop_data,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    std::optional<gfx::PointF> transformed_pt) {
  drag_in_progress_ = true;
  if (!target) {
    return;
  }
  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!drag_security_info_.IsValidDragTarget(target_rwh)) {
    return;
  }

  current_rwh_for_drag_ = target_rwh->GetWeakPtr();
  current_rvh_for_drag_ =
      GetRenderViewHostID(web_contents_->GetRenderViewHost());
  current_drag_data_ = std::move(drop_data);
  current_rwh_for_drag_->FilterDropData(current_drag_data_.get());

  blink::DragOperationsMask op_mask =
      ConvertToDragOperationsMask(drop_metadata.source_operations);

  WebContentsDelegate* delegate = web_contents_->GetDelegate();

  auto allow_drag = [&]() {
    // We only allow drags from privileged WebContents to
    // another privileged WebContents.
    // Do not allow dragging privileged WebContents to
    // non-priviledged WebContents or vice versa.
    if (current_drag_data_->is_from_privileged !=
        (delegate && delegate->IsPrivileged())) {
      return false;
    }

    // Give the delegate an opportunity to cancel the drag
    if (delegate && !delegate->CanDragEnter(
                        web_contents_, *current_drag_data_.get(), op_mask)) {
      return false;
    }
    return true;
  };

  if (!allow_drag()) {
    current_drag_data_ = nullptr;
    return;
  }

  DCHECK(transformed_pt.has_value());
  gfx::PointF screen_pt(display::Screen::GetScreen()->GetCursorScreenPoint());
  current_rwh_for_drag_->DragTargetDragEnter(
      *current_drag_data_, transformed_pt.value(), screen_pt, op_mask,
      ui::EventFlagsToWebEventModifiers(drop_metadata.flags),
      base::DoNothing());

  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnDragEnter();
  }
}

void WebContentsViewAura::OnDragEntered(const ui::DropTargetEvent& event) {
  if (web_contents_->ShouldIgnoreInputEvents()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  async_drop_navigation_observer_.reset();
#endif

  std::unique_ptr<DropData> drop_data = std::make_unique<DropData>();
  // Calling this here as event.data might become invalid inside the callback.
  PrepareDropData(drop_data.get(), event.data());

  if (drag_dest_delegate_) {
    drag_dest_delegate_->DragInitialize(web_contents_);
    drag_dest_delegate_->OnReceiveDragData(event.data());
  }

  DropMetadata drop_metadata(event);
  web_contents_->GetRenderWidgetHostAtPointAsynchronously(
      web_contents_->GetRenderViewHost()->GetWidget()->GetView(),
      event.location_f(),
      base::BindOnce(&WebContentsViewAura::DragEnteredCallback,
                     weak_ptr_factory_.GetWeakPtr(), drop_metadata,
                     std::move(drop_data)));
}

void WebContentsViewAura::DragUpdatedCallback(
    DropMetadata drop_metadata,
    std::unique_ptr<DropData> drop_data,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    std::optional<gfx::PointF> transformed_pt) {
  // If drag is not in progress it means drag has already finished and we get
  // this callback after that already. This happens for example when drag leaves
  // out window and we get the exit signal while still waiting for this
  // targeting callback to be called for the previous drag update signal. In
  // this case we just ignore this operation.
  if (!drag_in_progress_)
    return;
  if (!target) {
    return;
  }
  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!drag_security_info_.IsValidDragTarget(target_rwh)) {
    return;
  }

  aura::Window* root_window = GetNativeView()->GetRootWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  gfx::PointF screen_pt = drop_metadata.root_location;
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(root_window, &screen_pt);

  if (target_rwh != current_rwh_for_drag_.get()) {
    if (current_rwh_for_drag_) {
      gfx::PointF transformed_leave_point = drop_metadata.localized_location;
      static_cast<RenderWidgetHostViewBase*>(
          web_contents_->GetRenderWidgetHostView())
          ->TransformPointToCoordSpaceForView(
              drop_metadata.localized_location,
              static_cast<RenderWidgetHostViewBase*>(
                  current_rwh_for_drag_->GetView()),
              &transformed_leave_point);
      current_rwh_for_drag_->DragTargetDragLeave(transformed_leave_point,
                                                 screen_pt);
    }
    DragEnteredCallback(drop_metadata, std::move(drop_data), target,
                        transformed_pt);
  }

  if (!current_drag_data_) {
    return;
  }

  DCHECK(transformed_pt.has_value());
  blink::DragOperationsMask op_mask =
      ConvertToDragOperationsMask(drop_metadata.source_operations);
  target_rwh->DragTargetDragOver(
      transformed_pt.value(), screen_pt, op_mask,
      ui::EventFlagsToWebEventModifiers(drop_metadata.flags),
      base::DoNothing());

  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragOver();
}

aura::client::DragUpdateInfo WebContentsViewAura::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  if (web_contents_->ShouldIgnoreInputEvents()) {
    return aura::client::DragUpdateInfo();
  }
  aura::client::DragUpdateInfo drag_info;
  auto* focused_frame = web_contents_->GetFocusedFrame();
  if (focused_frame && !web_contents_->GetBrowserContext()->IsOffTheRecord() &&
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL().is_valid()) {
    drag_info.data_endpoint = ui::DataTransferEndpoint(
        web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
        {.off_the_record =
             web_contents_->GetBrowserContext()->IsOffTheRecord()});
  }

  std::unique_ptr<DropData> drop_data = std::make_unique<DropData>();
  // Calling this here as event.data might become invalid inside the callback.
  PrepareDropData(drop_data.get(), event.data());
  DropMetadata drop_metadata(event);
  web_contents_->GetRenderWidgetHostAtPointAsynchronously(
      web_contents_->GetRenderViewHost()->GetWidget()->GetView(),
      event.location_f(),
      base::BindOnce(&WebContentsViewAura::DragUpdatedCallback,
                     weak_ptr_factory_.GetWeakPtr(), drop_metadata,
                     std::move(drop_data)));

  drag_info.drag_operation =
      static_cast<int>(current_drag_data_ ? current_drag_data_->operation
                                          : ui::mojom::DragOperation::kNone);
  return drag_info;
}

void WebContentsViewAura::OnDragExited() {
  if (web_contents_->ShouldIgnoreInputEvents())
    return;
  CompleteDragExit();
}

void WebContentsViewAura::CompleteDragExit() {
  drag_in_progress_ = false;

  if (current_rwh_for_drag_ && !web_contents_->IsBeingDestroyed() &&
      current_rvh_for_drag_ ==
          GetRenderViewHostID(web_contents_->GetRenderViewHost())) {
    current_rwh_for_drag_->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
  }

  if (current_rwh_for_drag_) {
    current_rwh_for_drag_.reset();
  }

  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnDragLeave();
  }

  current_drag_data_.reset();
}

// PerformDropCallback() is called once the user releases the mouse button
// over this window.  This function completes the drop if possible.  A drop
// may not be possible for example if the RWH has changed since the user's drag
// entered this view.
//
// Performing the drop is an asynchronous operation that involves the RWH and
// the web contents delegate.  A drop is not considered done by this view until
// all the asynchronous operations complete.
//
// Assuming that a drop is allowed, an instance of OnPerformingDropContext is
// created to keep track of the drop state during the various async operations.
// This context is saved in the `drop_context` argument passed around to the
// various methods. The data being dropped, stored in `current_drag_data_`, is
// moved into the context.
//
// On the Windows platform, if the drop includes virtuals files (for example,
// dropping an email attachment dragged out of the native Outlook application)
// these are first converted into temp real files using the async function
// GetVirtualFilesAsTempFiles().  The callback OnGotVirtualFilesAsTempFiles()
// uses AsyncDropTempFileDeleter to make sure the temp files are deleted once
// the drop completes.  Other platform don't have handling of virtual files.
//
// Next, the delegate is given a chance to handle the dropped data in an async
// manner.  The delegate may perform additional checks on the dropped data,
// may filter that data according to specific criteria, and may even block the
// drop altogether.  For example, some enterprise policies may block
// sensitive data from being dropped on unsanctioned web pages.  This step is
// kicked off by calling MaybeLetDelegateProcessDrop() and the async response is
// handled by GotModifiedDropDataFromDelegate().  In tests it's possible that
// no delegate exists, in which case CompleteDrop() is called
// directly.
//
// GotModifiedDropDataFromDelegate() is called only when a delegate exists and
// processes the result of the delegate's handling of the dropped data.
// Assuming the delegate allows the drop, the dropped data in `drop_context`
// is updated and CompleteDrop() is called.
//
// CompleteDrop() calls CompleteDrop() to send the dropped data to
// the RWH.  At this point the drop is considered completed from this view's
// point of view.
//
// Note that many of the methods above are callback to async operations,
// like this method itself, OnGotVirtualFilesAsTempFiles(),
// GotModifiedDropDataFromDelegate().  Therefore these methods begin with
// similar checks to make sure the drop is still allowed. For example, checks
// to make sure the target RWH has not changed.  See
// drag_security_info_.IsValidDragTarget() for details.
void WebContentsViewAura::PerformDropCallback(
    DropMetadata drop_metadata,
    std::unique_ptr<ui::OSExchangeData> data,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    std::optional<gfx::PointF> transformed_pt) {
  drag_in_progress_ = false;
  base::ScopedClosureRunner end_drag_runner(std::move(end_drag_runner_));

  if (!target) {
    return;
  }
  RenderWidgetHostImpl* target_rwh =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (!drag_security_info_.IsValidDragTarget(target_rwh)) {
    return;
  }

  DCHECK(transformed_pt.has_value());

  gfx::PointF screen_pt(display::Screen::GetScreen()->GetCursorScreenPoint());
  if (target_rwh != current_rwh_for_drag_.get()) {
    if (current_rwh_for_drag_)
      current_rwh_for_drag_->DragTargetDragLeave(transformed_pt.value(),
                                                 screen_pt);

    std::unique_ptr<DropData> drop_data = std::make_unique<DropData>();
    PrepareDropData(drop_data.get(), *data.get());
    DragEnteredCallback(drop_metadata, std::move(drop_data), target,
                        transformed_pt);
  }

  // `current_drag_data_` is set in DragEnteredCallback() when the user begins
  // to drag over this view and the drag is allowed.  It's possible after the
  // call to DragEnteredCallback() above that this member becomes null
  // indicating that the drop should not happen.
  if (!current_drag_data_) {
    return;
  }

  OnPerformingDropContext drop_context(
      target_rwh, std::move(current_drag_data_), drop_metadata, std::move(data),
      std::move(end_drag_runner), transformed_pt, screen_pt);

#if BUILDFLAG(IS_WIN)
  if (ShouldIncludeVirtualFiles(*drop_context.drop_data) &&
      drop_context.data->HasVirtualFilenames()) {
    // Asynchronously retrieve the actual content of any virtual files now (this
    // step is not needed for "real" files already on the file system, e.g.
    // those dropped on Chromium from the desktop). When all content has been
    // written to temporary files, the OnGotVirtualFilesAsTempFiles
    // callback will be invoked and the drop communicated to the renderer
    // process.
    async_drop_navigation_observer_ =
        std::make_unique<AsyncDropNavigationObserver>(web_contents_);
    ui::OSExchangeData* data_ptr = drop_context.data.get();
    data_ptr->GetVirtualFilesAsTempFiles(base::BindOnce(
        &WebContentsViewAura::OnGotVirtualFilesAsTempFiles,
        weak_ptr_factory_.GetWeakPtr(), std::move(drop_context)));
    return;
  }
#endif

  MaybeLetDelegateProcessDrop(std::move(drop_context));
}

void WebContentsViewAura::MaybeLetDelegateProcessDrop(
    OnPerformingDropContext drop_context) {
  // |delegate_| may be null in unit tests.
  // TODO(crbug.com/40274271): Tests should use a delegate.
  if (delegate_) {
    auto* drop_data_ptr = drop_context.drop_data.get();
    delegate_->OnPerformingDrop(
        *drop_data_ptr,
        base::BindOnce(&WebContentsViewAura::GotModifiedDropDataFromDelegate,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(drop_context)));
  } else {
    CompleteDrop(std::move(drop_context));
  }
}

void WebContentsViewAura::GotModifiedDropDataFromDelegate(
    OnPerformingDropContext drop_context,
    std::optional<DropData> drop_data) {
  // This is possibly an async callback.  Make sure the RWH is still valid.
  if (!drop_context.target_rwh ||
      !drag_security_info_.IsValidDragTarget(drop_context.target_rwh.get())) {
    return;
  }

  if (!drop_data.has_value()) {
    if (!drop_callback_for_testing_.is_null()) {
      const int key_modifiers =
          ui::EventFlagsToWebEventModifiers(drop_context.drop_metadata.flags);
      std::move(drop_callback_for_testing_)
          .Run(drop_context.target_rwh.get(), *drop_context.drop_data,
               drop_context.transformed_pt.value(), drop_context.screen_pt,
               key_modifiers,
               /*drop_allowed=*/false);
    }

    // The drop not being continued requires this to cleanup the drag data.
    CompleteDragExit();

    return;
  }

  *drop_context.drop_data = std::move(drop_data.value());
  CompleteDrop(std::move(drop_context));
}

aura::client::DragDropDelegate::DropCallback
WebContentsViewAura::GetDropCallback(const ui::DropTargetEvent& event) {
  if (web_contents_->ShouldIgnoreInputEvents())
    return base::DoNothing();
  base::ScopedClosureRunner drag_exit(base::BindOnce(
      &WebContentsViewAura::CompleteDragExit, weak_ptr_factory_.GetWeakPtr()));
  DropMetadata drop_metadata(event);
  return base::BindOnce(&WebContentsViewAura::PerformDropOrExitDrag,
                        weak_ptr_factory_.GetWeakPtr(), std::move(drag_exit),
                        drop_metadata);
}

void WebContentsViewAura::CompleteDrop(OnPerformingDropContext drop_context) {
  web_contents_->Focus();

  const int key_modifiers =
      ui::EventFlagsToWebEventModifiers(drop_context.drop_metadata.flags);
  drop_context.target_rwh.get()->DragTargetDrop(
      *drop_context.drop_data, drop_context.transformed_pt.value(),
      drop_context.screen_pt, key_modifiers, base::DoNothing());
  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnDrop();
  }

  if (!drop_callback_for_testing_.is_null()) {
    std::move(drop_callback_for_testing_)
        .Run(drop_context.target_rwh.get(), *drop_context.drop_data,
             drop_context.transformed_pt.value(), drop_context.screen_pt,
             key_modifiers,
             /*drop_allowed=*/true);
  }
}

void WebContentsViewAura::PerformDropOrExitDrag(
    base::ScopedClosureRunner exit_drag,
    DropMetadata drop_metadata,
    std::unique_ptr<ui::OSExchangeData> data,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  // Set output_drag_op before calling on `web_contents_` below because it
  // is possible for the drop to end and the member `current_drag_data_` to be
  // reset.
  output_drag_op = current_drag_data_ ? current_drag_data_->operation
                                      : ui::mojom::DragOperation::kNone;

  web_contents_->GetRenderWidgetHostAtPointAsynchronously(
      web_contents_->GetRenderViewHost()->GetWidget()->GetView(),
      drop_metadata.localized_location,
      base::BindOnce(&WebContentsViewAura::PerformDropCallback,
                     weak_ptr_factory_.GetWeakPtr(), drop_metadata,
                     std::move(data)));
  exit_drag.ReplaceClosure(base::DoNothing());
}

void WebContentsViewAura::RegisterDropCallbackForTesting(
    DropCallbackForTesting callback) {
  drop_callback_for_testing_ = std::move(callback);
}

#if BUILDFLAG(IS_WIN)
void WebContentsViewAura::OnGotVirtualFilesAsTempFiles(
    OnPerformingDropContext drop_context,
    const std::vector<std::pair<base::FilePath, base::FilePath>>&
        filepaths_and_names) {
  if (!async_drop_navigation_observer_) {
    return;
  }

  if (!filepaths_and_names.empty()) {
    std::unique_ptr<AsyncDropNavigationObserver> drop_observer(
        std::move(async_drop_navigation_observer_));

    RenderWidgetHostImpl* target_rwh = drop_context.target_rwh.get();

    // Security check--don't allow the drop if a navigation occurred since the
    // drop was initiated or the render widget host has changed or it is not a
    // valid target.
    if (!drop_observer->drop_allowed() ||
        !(target_rwh && target_rwh == current_rwh_for_drag_.get() &&
          drag_security_info_.IsValidDragTarget(target_rwh))) {
      // Signal test code that the drop is disallowed.
      if (!drop_callback_for_testing_.is_null()) {
        std::move(drop_callback_for_testing_)
            .Run(target_rwh, *drop_context.drop_data,
                 drop_context.transformed_pt.value(), drop_context.screen_pt,
                 drop_context.drop_metadata.flags,
                 drop_observer->drop_allowed());
      }

      CompleteDragExit();
      return;
    }

    // The vector of filenames will still have items added during dragenter
    // (script is allowed to enumerate the files in the data store but not
    // retrieve the file contents in dragenter). But the temp file path in the
    // FileInfo structs will just be a placeholder. Clear out the vector before
    // replacing it with FileInfo structs that have the paths to the retrieved
    // file contents.
    drop_context.drop_data->filenames.clear();

    // Ensure we have temp file deleter.
    if (!async_drop_temp_file_deleter_) {
      async_drop_temp_file_deleter_ =
          std::make_unique<AsyncDropTempFileDeleter>();
    }

    for (const auto& filepath_and_name : filepaths_and_names) {
      drop_context.drop_data->filenames.push_back(
          ui::FileInfo(filepath_and_name.first, filepath_and_name.second));

      // Make sure the temp file eventually gets cleaned up.
      async_drop_temp_file_deleter_->RegisterFile(filepath_and_name.first);
    }
  }

  MaybeLetDelegateProcessDrop(std::move(drop_context));
}
#endif

int WebContentsViewAura::GetTopControlsHeight() const {
  WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (!delegate)
    return 0;
  return delegate->GetTopControlsHeight();
}

int WebContentsViewAura::GetBottomControlsHeight() const {
  WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (!delegate)
    return 0;
  return delegate->GetBottomControlsHeight();
}

bool WebContentsViewAura::DoBrowserControlsShrinkRendererSize() const {
  WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (!delegate)
    return false;
  return delegate->DoBrowserControlsShrinkRendererSize(web_contents_);
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void WebContentsViewAura::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  NOTIMPLEMENTED() << " show " << menu_items.size() << " menu items";
}
#endif

}  // namespace content
