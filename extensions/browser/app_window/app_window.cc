// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/app_window/app_web_contents_helper.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_geometry_cache.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_MAC)
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"
#endif

using blink::mojom::ConsoleMessageLevel;
using content::BrowserContext;
using content::WebContents;
using web_modal::WebContentsModalDialogHost;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

namespace {

const int kDefaultWidth = 512;
const int kDefaultHeight = 384;

void SetConstraintProperty(const std::string& name,
                           int value,
                           base::Value::Dict* bounds_properties) {
  DCHECK(bounds_properties);
  if (value != SizeConstraints::kUnboundedSize)
    bounds_properties->Set(name, value);
  else
    bounds_properties->Set(name, base::Value());
}

void SetBoundsProperties(const gfx::Rect& bounds,
                         const gfx::Size& min_size,
                         const gfx::Size& max_size,
                         const std::string& bounds_name,
                         base::Value::Dict* window_properties) {
  DCHECK(window_properties);
  base::Value::Dict bounds_properties;

  bounds_properties.Set("left", bounds.x());
  bounds_properties.Set("top", bounds.y());
  bounds_properties.Set("width", bounds.width());
  bounds_properties.Set("height", bounds.height());

  SetConstraintProperty("minWidth", min_size.width(), &bounds_properties);
  SetConstraintProperty("minHeight", min_size.height(), &bounds_properties);
  SetConstraintProperty("maxWidth", max_size.width(), &bounds_properties);
  SetConstraintProperty("maxHeight", max_size.height(), &bounds_properties);

  window_properties->Set(bounds_name, std::move(bounds_properties));
}

// Combines the constraints of the content and window, and returns constraints
// for the window. `is_minimum_size_constraint` is true when combining the
// minimum_size constraints for the window.
gfx::Size GetCombinedWindowConstraints(const gfx::Size& window_constraints,
                                       const gfx::Size& content_constraints,
                                       const gfx::Insets& frame_insets,
                                       const gfx::RoundedCornersF& window_radii,
                                       bool is_minimum_size_constraint) {
  gfx::Size combined_constraints(window_constraints);
  if (content_constraints.width() > 0) {
    combined_constraints.set_width(content_constraints.width() +
                                   frame_insets.width());
  }
  if (content_constraints.height() > 0) {
    combined_constraints.set_height(content_constraints.height() +
                                    frame_insets.height());
  }

  // To prevent the rounded corners of the window from overlapping, the rounded
  // window should have a minimum size equal to `rounded_window_minimum_size`.
  // Adjust the combined constraint (minimum size or maximum size of the window)
  // to enforce the minimum size of the window to at least be equal to
  // `rounded_window_minimum_size`, and if a maximum size is set, it should be
  // greater than or equal to `rounded_window_minimum_size`.
  const gfx::Size rounded_window_minimum_size =
      SizeConstraints::GetMinimumSizeSupportingRoundedCorners(window_radii);
  const bool override_width_constraint =
      is_minimum_size_constraint || combined_constraints.width() > 0;
  const bool override_height_constraint =
      is_minimum_size_constraint || combined_constraints.height() > 0;

  if (override_width_constraint) {
    combined_constraints.set_width(std::max(rounded_window_minimum_size.width(),
                                            combined_constraints.width()));
  }

  if (override_height_constraint) {
    combined_constraints.set_height(std::max(
        rounded_window_minimum_size.height(), combined_constraints.height()));
  }

  return combined_constraints;
}

// Combines the constraints of the content and window, and returns constraints
// for the content.
gfx::Size GetCombinedContentConstraints(const gfx::Size& window_constraints,
                                        const gfx::Size& content_constraints,
                                        const gfx::Insets& frame_insets) {
  gfx::Size combined_constraints(content_constraints);
  if (window_constraints.width() > 0) {
    combined_constraints.set_width(
        std::max(0, window_constraints.width() - frame_insets.width()));
  }
  if (window_constraints.height() > 0) {
    combined_constraints.set_height(
        std::max(0, window_constraints.height() - frame_insets.height()));
  }
  return combined_constraints;
}

}  // namespace

// AppWindow::BoundsSpecification

const int AppWindow::BoundsSpecification::kUnspecifiedPosition = INT_MIN;

AppWindow::BoundsSpecification::BoundsSpecification()
    : bounds(kUnspecifiedPosition, kUnspecifiedPosition, 0, 0) {}

AppWindow::BoundsSpecification::~BoundsSpecification() = default;

void AppWindow::BoundsSpecification::ResetBounds() {
  bounds.SetRect(kUnspecifiedPosition, kUnspecifiedPosition, 0, 0);
}

// AppWindow::CreateParams

AppWindow::CreateParams::CreateParams()
    : window_type(AppWindow::WINDOW_TYPE_DEFAULT),
      frame(AppWindow::FRAME_CHROME),
      has_frame_color(false),
      active_frame_color(SK_ColorBLACK),
      inactive_frame_color(SK_ColorBLACK),
      alpha_enabled(false),
      is_ime_window(false),
      creator_process_id(0),
      state(ui::mojom::WindowShowState::kDefault),
      hidden(false),
      resizable(true),
      focused(true),
      always_on_top(false),
      visible_on_all_workspaces(false),
      show_on_lock_screen(false),
      show_in_shelf(false) {}

AppWindow::CreateParams::CreateParams(const CreateParams& other) = default;

AppWindow::CreateParams::~CreateParams() = default;

gfx::Rect AppWindow::CreateParams::GetInitialWindowBounds(
    const gfx::Insets& frame_insets,
    const gfx::RoundedCornersF& window_radii) const {
  // Combine into a single window bounds.
  gfx::Rect combined_bounds(window_spec.bounds);
  if (content_spec.bounds.x() != BoundsSpecification::kUnspecifiedPosition)
    combined_bounds.set_x(content_spec.bounds.x() - frame_insets.left());
  if (content_spec.bounds.y() != BoundsSpecification::kUnspecifiedPosition)
    combined_bounds.set_y(content_spec.bounds.y() - frame_insets.top());
  if (content_spec.bounds.width() > 0) {
    combined_bounds.set_width(content_spec.bounds.width() +
                              frame_insets.width());
  }
  if (content_spec.bounds.height() > 0) {
    combined_bounds.set_height(content_spec.bounds.height() +
                               frame_insets.height());
  }

  // Constrain the bounds.
  SizeConstraints constraints(
      GetCombinedWindowConstraints(
          window_spec.minimum_size, content_spec.minimum_size, frame_insets,
          window_radii, /*is_minimum_size_constraint=*/true),
      GetCombinedWindowConstraints(
          window_spec.maximum_size, content_spec.maximum_size, frame_insets,
          window_radii, /*is_minimum_size_constraint=*/false));
  combined_bounds.set_size(constraints.ClampSize(combined_bounds.size()));

  return combined_bounds;
}

gfx::Size AppWindow::CreateParams::GetContentMinimumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedContentConstraints(window_spec.minimum_size,
                                       content_spec.minimum_size, frame_insets);
}

gfx::Size AppWindow::CreateParams::GetContentMaximumSize(
    const gfx::Insets& frame_insets) const {
  return GetCombinedContentConstraints(window_spec.maximum_size,
                                       content_spec.maximum_size, frame_insets);
}

gfx::Size AppWindow::CreateParams::GetWindowMinimumSize(
    const gfx::Insets& frame_insets,
    const gfx::RoundedCornersF& window_radii) const {
  return GetCombinedWindowConstraints(
      window_spec.minimum_size, content_spec.minimum_size, frame_insets,
      window_radii, /*is_minimum_size_constraint=*/true);
}

gfx::Size AppWindow::CreateParams::GetWindowMaximumSize(
    const gfx::Insets& frame_insets,
    const gfx::RoundedCornersF& window_radii) const {
  return GetCombinedWindowConstraints(
      window_spec.maximum_size, content_spec.maximum_size, frame_insets,
      window_radii, /*is_minimum_size_constraint=*/false);
}

// AppWindow

AppWindow::AppWindow(BrowserContext* context,
                     std::unique_ptr<AppDelegate> app_delegate,
                     const Extension* extension)
    : browser_context_(context),
      extension_id_(extension->id()),
      session_id_(SessionID::NewUnique()),
      app_delegate_(std::move(app_delegate)) {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  CHECK(!client->IsGuestSession(context) || context->IsOffTheRecord())
      << "Only off the record window may be opened in the guest mode.";
}

void AppWindow::Init(const GURL& url,
                     std::unique_ptr<AppWindowContents> app_window_contents,
                     content::RenderFrameHost* creator_frame,
                     const CreateParams& params) {
  // Initialize the render interface and web contents
  app_window_contents_ = std::move(app_window_contents);
  app_window_contents_->Initialize(browser_context(), creator_frame, url);

  initial_url_ = url;

  content::WebContentsObserver::Observe(web_contents());
  SetViewType(web_contents(), mojom::ViewType::kAppWindow);
  app_delegate_->InitWebContents(web_contents());

  ExtensionWebContentsObserver::GetForWebContents(web_contents())
      ->dispatcher()
      ->set_delegate(this);

  WebContentsModalDialogManager::CreateForWebContents(web_contents());

  web_contents()->SetDelegate(this);
  WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(this);

  // Initialize the window
  CreateParams new_params = LoadDefaults(params);
  window_type_ = new_params.window_type;
  window_key_ = new_params.window_key;

  // Windows cannot be always-on-top in fullscreen mode for security reasons.
  cached_always_on_top_ = new_params.always_on_top;
  if (new_params.state == ui::mojom::WindowShowState::kFullscreen &&
      !ExtensionsBrowserClient::Get()->IsScreensaverInDemoMode(
          extension_id())) {
    new_params.always_on_top = false;
  }

  requested_alpha_enabled_ = new_params.alpha_enabled;
  is_ime_window_ = params.is_ime_window;
  show_on_lock_screen_ = params.show_on_lock_screen;
  show_in_shelf_ = params.show_in_shelf;

  AppWindowClient* app_window_client = AppWindowClient::Get();
  native_app_window_ =
      app_window_client->CreateNativeAppWindow(this, &new_params);

  helper_ = std::make_unique<AppWebContentsHelper>(
      browser_context_, extension_id_, web_contents(), app_delegate_.get());

  native_app_window_->UpdateWindowIcon();

  if (params.window_icon_url.is_valid())
    SetAppIconUrl(params.window_icon_url);

  AppWindowRegistry::Get(browser_context_)->AddAppWindow(this);

  if (new_params.hidden) {
    // Although the window starts hidden by default, calling Hide() here
    // notifies observers of the window being hidden.
    Hide();
  } else {
    // These states may cause the window to show, so they are ignored if the
    // window is initially hidden.
    if (new_params.state == ui::mojom::WindowShowState::kFullscreen) {
      Fullscreen();
    } else if (new_params.state == ui::mojom::WindowShowState::kMaximized) {
      Maximize();
    } else if (new_params.state == ui::mojom::WindowShowState::kMinimized) {
      Minimize();
    }

    Show(new_params.focused ? SHOW_ACTIVE : SHOW_INACTIVE);
  }

  OnNativeWindowChanged();

  ExtensionRegistry::Get(browser_context_)->AddObserver(this);

  // Close when the browser process is exiting.
  app_delegate_->SetTerminatingCallback(base::BindOnce(
      &NativeAppWindow::Close, base::Unretained(native_app_window_.get())));

  app_window_contents_->LoadContents(new_params.creator_process_id);
}

AppWindow::~AppWindow() {
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
}

void AppWindow::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(AppWindow::web_contents(), web_contents);
  helper_->RequestMediaAccessPermission(request, std::move(callback));
}

bool AppWindow::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(render_frame_host)
                ->GetOutermostWebContents());
  return helper_->CheckMediaAccessPermission(render_frame_host, security_origin,
                                             type);
}

WebContents* AppWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  DCHECK_EQ(web_contents(), source);
  return helper_->OpenURLFromTab(params, std::move(navigation_handle_callback));
}

content::WebContents* AppWindow::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK(new_contents->GetBrowserContext() == browser_context_);
  app_delegate_->AddNewContents(browser_context_, std::move(new_contents),
                                target_url, disposition, window_features,
                                user_gesture);
  return nullptr;
}

content::KeyboardEventProcessingResult AppWindow::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  const Extension* extension = GetExtension();
  if (!extension)
    return content::KeyboardEventProcessingResult::NOT_HANDLED;

  // Here, we can handle a key event before the content gets it. When we are
  // fullscreen and it is not forced, we want to allow the user to leave
  // when ESC is pressed.
  // However, if the application has the "overrideEscFullscreen" permission, we
  // should let it override that behavior.
  // ::HandleKeyboardEvent() will only be called if the KeyEvent's default
  // action is not prevented.
  // Thus, we should handle the KeyEvent here only if the permission is not set.
  if (event.windows_key_code == ui::VKEY_ESCAPE && IsFullscreen() &&
      !IsForcedFullscreen() &&
      !extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kOverrideEscFullscreen)) {
    Restore();
    return content::KeyboardEventProcessingResult::HANDLED;
  }

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool AppWindow::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // If the window is currently fullscreen and not forced, ESC should leave
  // fullscreen.  If this code is being called for ESC, that means that the
  // KeyEvent's default behavior was not prevented by the content.
  if (event.windows_key_code == ui::VKEY_ESCAPE && IsFullscreen() &&
      !IsForcedFullscreen()) {
    Restore();
    return true;
  }

  return native_app_window_->HandleKeyboardEvent(event);
}

void AppWindow::RequestPointerLock(WebContents* web_contents,
                                   bool user_gesture,
                                   bool last_unlocked_by_target) {
  DCHECK_EQ(AppWindow::web_contents(), web_contents);
  helper_->RequestPointerLock();
}

bool AppWindow::PreHandleGestureEvent(WebContents* source,
                                      const blink::WebGestureEvent& event) {
  return AppWebContentsHelper::ShouldSuppressGestureEvent(event);
}

bool AppWindow::TakeFocus(WebContents* source, bool reverse) {
  return app_delegate_->TakeFocus(source, reverse);
}

content::PictureInPictureResult AppWindow::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return app_delegate_->EnterPictureInPicture(web_contents);
}

void AppWindow::ExitPictureInPicture() {
  app_delegate_->ExitPictureInPicture();
}

bool AppWindow::ShouldShowStaleContentOnEviction(content::WebContents* source) {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void AppWindow::RenderFrameCreated(content::RenderFrameHost* frame_host) {
  app_delegate_->RenderFrameCreated(frame_host);
}

void AppWindow::AddOnDidFinishFirstNavigationCallback(
    DidFinishFirstNavigationCallback callback) {
  on_did_finish_first_navigation_callbacks_.push_back(std::move(callback));
}

void AppWindow::OnDidFinishFirstNavigation() {
  did_finish_first_navigation_ = true;
  std::vector<DidFinishFirstNavigationCallback> callbacks;
  std::swap(callbacks, on_did_finish_first_navigation_callbacks_);
  for (auto&& callback : callbacks)
    std::move(callback).Run(true /* did_finish */);
}

void AppWindow::OnNativeClose() {
  AppWindowRegistry::Get(browser_context_)->RemoveAppWindow(this);

  // Run pending |on_did_finish_first_navigation_callback_| so that
  // AppWindowCreateFunction can respond with an error properly.
  std::vector<DidFinishFirstNavigationCallback> callbacks;
  std::swap(callbacks, on_did_finish_first_navigation_callbacks_);
  // No "OnClosed" event on window creation error.
  const bool send_onclosed = callbacks.empty();
  for (auto&& callback : callbacks)
    std::move(callback).Run(false /* did_finish */);

  if (app_window_contents_) {
    WebContentsModalDialogManager* modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(web_contents());
    if (modal_dialog_manager)  // May be null in unit tests.
      modal_dialog_manager->SetDelegate(nullptr);
    app_window_contents_->NativeWindowClosed(send_onclosed);
  }

  delete this;
}

void AppWindow::OnNativeWindowChanged() {
  // This may be called during Init before |native_app_window_| is set.
  if (!native_app_window_)
    return;

#if BUILDFLAG(IS_MAC)
  // On Mac the user can change the window's fullscreen state. If that has
  // happened, update AppWindow's internal state.
  if (native_app_window_->IsFullscreen()) {
    if (!IsFullscreen())
      fullscreen_types_ = FULLSCREEN_TYPE_OS;
  } else {
    fullscreen_types_ = FULLSCREEN_TYPE_NONE;
  }

  RestoreAlwaysOnTop();  // Same as in SetNativeWindowFullscreen.
#endif

  SaveWindowPosition();

#if BUILDFLAG(IS_WIN)
  if (cached_always_on_top_ && !IsFullscreen() &&
      !native_app_window_->IsMaximized() &&
      !native_app_window_->IsMinimized()) {
    UpdateNativeAlwaysOnTop();
  }
#endif

  if (app_window_contents_)
    app_window_contents_->NativeWindowChanged(native_app_window_.get());
}

void AppWindow::OnNativeWindowActivated() {
  AppWindowRegistry::Get(browser_context_)->AppWindowActivated(this);
}

content::WebContents* AppWindow::web_contents() const {
  if (app_window_contents_)
    return app_window_contents_->GetWebContents();
  return nullptr;
}

const Extension* AppWindow::GetExtension() const {
  return ExtensionRegistry::Get(browser_context_)
      ->enabled_extensions()
      .GetByID(extension_id_);
}

NativeAppWindow* AppWindow::GetBaseWindow() {
  return native_app_window_.get();
}

gfx::NativeWindow AppWindow::GetNativeWindow() {
  return GetBaseWindow()->GetNativeWindow();
}

gfx::Rect AppWindow::GetClientBounds() const {
  gfx::Rect bounds = native_app_window_->GetBounds();
  bounds.Inset(native_app_window_->GetFrameInsets());
  return bounds;
}

std::u16string AppWindow::GetTitle() const {
  const Extension* extension = GetExtension();
  if (!extension)
    return std::u16string();

  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  std::u16string title;
  content::NavigationEntry* entry =
      web_contents() ? web_contents()->GetController().GetLastCommittedEntry()
                     : nullptr;
  if (!entry || entry->GetTitle().empty()) {
    title = base::UTF8ToUTF16(extension->name());
  } else {
    title = web_contents()->GetTitle();
  }
  base::RemoveChars(title, u"\n", &title);
  return title;
}

void AppWindow::SetAppIconUrl(const GURL& url) {
  app_icon_url_ = url;

  // Don't start custom app icon loading in the case window is not ready yet.
  // see crbug.com/788531.
  if (!window_ready_)
    return;

  StartAppIconDownload();
}

void AppWindow::UpdateShape(std::unique_ptr<ShapeRects> rects) {
  native_app_window_->UpdateShape(std::move(rects));
}

void AppWindow::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  CHECK_EQ(contents, web_contents())
      << "Received DraggableRegionsChanged() notification for unexpected web "
         "contents";

  native_app_window_->DraggableRegionsChanged(regions);

  if (on_update_draggable_regions_callback_for_testing_) {
    std::move(on_update_draggable_regions_callback_for_testing_).Run();
  }
}

void AppWindow::UpdateAppIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return;
  custom_app_icon_ = image;
  native_app_window_->UpdateWindowIcon();
}

void AppWindow::SetFullscreen(FullscreenType type, bool enable) {
  DCHECK_NE(FULLSCREEN_TYPE_NONE, type);

  if (enable) {
#if !BUILDFLAG(IS_MAC)
    // Do not enter fullscreen mode if disallowed by pref.
    // TODO(bartfab): Add a test once it becomes possible to simulate a user
    // gesture. http://crbug.com/174178
    if (type != FULLSCREEN_TYPE_FORCED) {
      PrefService* prefs =
          ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
              browser_context());
      if (!prefs->GetBoolean(pref_names::kAppFullscreenAllowed))
        return;
    }
#endif
    fullscreen_types_ |= type;
  } else {
    fullscreen_types_ &= ~type;
  }
  SetNativeWindowFullscreen();
}

bool AppWindow::IsFullscreen() const {
  return fullscreen_types_ != FULLSCREEN_TYPE_NONE;
}

bool AppWindow::IsForcedFullscreen() const {
  return (fullscreen_types_ & FULLSCREEN_TYPE_FORCED) != 0;
}

bool AppWindow::IsHtmlApiFullscreen() const {
  return (fullscreen_types_ & FULLSCREEN_TYPE_HTML_API) != 0;
}

bool AppWindow::IsOsFullscreen() const {
  return (fullscreen_types_ & FULLSCREEN_TYPE_OS) != 0;
}

void AppWindow::Fullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_WINDOW_API, true);
}

void AppWindow::Maximize() {
  GetBaseWindow()->Maximize();
}

void AppWindow::Minimize() {
  GetBaseWindow()->Minimize();
}

void AppWindow::Restore() {
  if (IsFullscreen()) {
    fullscreen_types_ = FULLSCREEN_TYPE_NONE;
    SetNativeWindowFullscreen();
  } else {
    GetBaseWindow()->Restore();
  }
}

void AppWindow::OSFullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_OS, true);
}

void AppWindow::ForcedFullscreen() {
  SetFullscreen(FULLSCREEN_TYPE_FORCED, true);
}

void AppWindow::SetContentSizeConstraints(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SizeConstraints constraints(min_size, max_size);
  native_app_window_->SetContentSizeConstraints(constraints.GetMinimumSize(),
                                                constraints.GetMaximumSize());

  gfx::Rect bounds = GetClientBounds();
  gfx::Size constrained_size = constraints.ClampSize(bounds.size());
  if (bounds.size() != constrained_size) {
    bounds.set_size(constrained_size);
    bounds.Inset(-native_app_window_->GetFrameInsets());
    native_app_window_->SetBounds(bounds);
  }
  OnNativeWindowChanged();
}

void AppWindow::Show(ShowType show_type) {
  app_delegate_->OnShow();
  bool was_hidden = is_hidden_ || !has_been_shown_;
  is_hidden_ = false;

  switch (show_type) {
    case SHOW_ACTIVE:
      GetBaseWindow()->Show();
      break;
    case SHOW_INACTIVE:
      GetBaseWindow()->ShowInactive();
      break;
  }
  AppWindowRegistry::Get(browser_context_)->AppWindowShown(this, was_hidden);
  has_been_shown_ = true;
}

void AppWindow::Hide() {
  is_hidden_ = true;
  GetBaseWindow()->Hide();
  AppWindowRegistry::Get(browser_context_)->AppWindowHidden(this);
  app_delegate_->OnHide();
}

void AppWindow::SetAlwaysOnTop(bool always_on_top) {
  if (cached_always_on_top_ == always_on_top)
    return;

  cached_always_on_top_ = always_on_top;

  // As a security measure, do not allow fullscreen windows or windows that
  // overlap the taskbar to be on top. The property will be applied when the
  // window exits fullscreen and moves away from the taskbar.
  if ((!IsFullscreen() ||
       ExtensionsBrowserClient::Get()->IsScreensaverInDemoMode(
           extension_id())) &&
      !IntersectsWithTaskbar()) {
    native_app_window_->SetZOrderLevel(always_on_top
                                           ? ui::ZOrderLevel::kFloatingWindow
                                           : ui::ZOrderLevel::kNormal);
  }

  OnNativeWindowChanged();
}

bool AppWindow::IsAlwaysOnTop() const {
  return cached_always_on_top_;
}

void AppWindow::RestoreAlwaysOnTop() {
  if (cached_always_on_top_)
    UpdateNativeAlwaysOnTop();
}

void AppWindow::GetSerializedState(base::Value::Dict* properties) const {
  DCHECK(properties);

  properties->Set("fullscreen", native_app_window_->IsFullscreenOrPending());
  properties->Set("minimized", native_app_window_->IsMinimized());
  properties->Set("maximized", native_app_window_->IsMaximized());
  properties->Set("alwaysOnTop", IsAlwaysOnTop());
  properties->Set("hasFrameColor", native_app_window_->HasFrameColor());
  properties->Set(
      "alphaEnabled",
      requested_alpha_enabled_ && native_app_window_->CanHaveAlphaEnabled());

  // These properties are undocumented and are to enable testing. Alpha is
  // removed to
  // make the values easier to check.
  SkColor transparent_white = ~SK_ColorBLACK;
  properties->Set("activeFrameColor",
                  static_cast<int>(native_app_window_->ActiveFrameColor() &
                                   transparent_white));
  properties->Set("inactiveFrameColor",
                  static_cast<int>(native_app_window_->InactiveFrameColor() &
                                   transparent_white));

  gfx::Rect content_bounds = GetClientBounds();
  gfx::Size content_min_size = native_app_window_->GetContentMinimumSize();
  gfx::Size content_max_size = native_app_window_->GetContentMaximumSize();
  SetBoundsProperties(content_bounds, content_min_size, content_max_size,
                      "innerBounds", properties);

  gfx::Insets frame_insets = native_app_window_->GetFrameInsets();
  gfx::RoundedCornersF window_radii = native_app_window_->GetWindowRadii();
  gfx::Rect window_bounds = native_app_window_->GetBounds();
  gfx::Size window_min_size = SizeConstraints::AddWindowToConstraints(
      content_min_size, frame_insets, window_radii);
  gfx::Size window_max_size = SizeConstraints::AddWindowToConstraints(
      content_max_size, frame_insets, window_radii);
  SetBoundsProperties(window_bounds, window_min_size, window_max_size,
                      "outerBounds", properties);
}

//------------------------------------------------------------------------------
// Private methods
void AppWindow::StartAppIconDownload() {
  DCHECK(app_icon_url_.is_valid());

  // Avoid using any previous icons that were being downloaded.
  image_loader_ptr_factory_.InvalidateWeakPtrs();
  const gfx::Size preferred_size(app_delegate_->PreferredIconSize(),
                                 app_delegate_->PreferredIconSize());
  web_contents()->DownloadImage(
      app_icon_url_,
      true,  // is a favicon
      preferred_size,
      0,      // no maximum size
      false,  // normal cache policy
      base::BindOnce(&AppWindow::DidDownloadFavicon,
                     image_loader_ptr_factory_.GetWeakPtr()));
}

void AppWindow::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (image_url != app_icon_url_ || bitmaps.empty())
    return;

  // Bitmaps are ordered largest to smallest. Choose the smallest bitmap
  // whose height >= the preferred size.
  int largest_index = 0;
  for (size_t i = 1; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < app_delegate_->PreferredIconSize())
      break;
    largest_index = i;
  }
  const SkBitmap& largest = bitmaps[largest_index];
  UpdateAppIcon(gfx::Image::CreateFrom1xBitmap(largest));
}

void AppWindow::SetNativeWindowFullscreen() {
  native_app_window_->SetFullscreen(fullscreen_types_);

  RestoreAlwaysOnTop();
}

bool AppWindow::IntersectsWithTaskbar() const {
#if BUILDFLAG(IS_WIN)
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect window_bounds = native_app_window_->GetRestoredBounds();
  std::vector<display::Display> displays = screen->GetAllDisplays();

  for (std::vector<display::Display>::const_iterator it = displays.begin();
       it != displays.end(); ++it) {
    gfx::Rect taskbar_bounds = it->bounds();
    taskbar_bounds.Subtract(it->work_area());
    if (taskbar_bounds.IsEmpty())
      continue;

    if (window_bounds.Intersects(taskbar_bounds))
      return true;
  }
#endif

  return false;
}

void AppWindow::UpdateNativeAlwaysOnTop() {
  DCHECK(cached_always_on_top_);
  bool is_on_top =
      native_app_window_->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow;
  bool fullscreen = IsFullscreen();
  bool intersects_taskbar = IntersectsWithTaskbar();

  if (is_on_top && (fullscreen || intersects_taskbar)) {
    // When entering fullscreen or overlapping the taskbar, ensure windows are
    // not always-on-top.
    native_app_window_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  } else if (!is_on_top && !fullscreen && !intersects_taskbar) {
    // When exiting fullscreen and moving away from the taskbar, reinstate
    // always-on-top.
    native_app_window_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  }
}

void AppWindow::ActivateContents(WebContents* contents) {
  native_app_window_->Activate();
}

void AppWindow::CloseContents(WebContents* contents) {
  native_app_window_->Close();
}

bool AppWindow::ShouldSuppressDialogs(WebContents* source) {
  return true;
}

void AppWindow::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  app_delegate_->RunFileChooser(render_frame_host, std::move(listener), params);
}

void AppWindow::SetContentsBounds(WebContents* source,
                                  const gfx::Rect& bounds) {
  native_app_window_->SetBounds(bounds);
}

void AppWindow::NavigationStateChanged(content::WebContents* source,
                                       content::InvalidateTypes changed_flags) {
  if (changed_flags & content::INVALIDATE_TYPE_TITLE)
    native_app_window_->UpdateWindowTitle();
  else if (changed_flags & content::INVALIDATE_TYPE_TAB)
    native_app_window_->UpdateWindowIcon();
}

void AppWindow::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  ToggleFullscreenModeForTab(WebContents::FromRenderFrameHost(requesting_frame),
                             true);
}

void AppWindow::ExitFullscreenModeForTab(content::WebContents* source) {
  ToggleFullscreenModeForTab(source, false);
}

void AppWindow::AppWindowReady() {
  window_ready_ = true;

  if (app_icon_url_.is_valid())
    StartAppIconDownload();
}

void AppWindow::ToggleFullscreenModeForTab(content::WebContents* source,
                                           bool enter_fullscreen) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (!IsExtensionWithPermissionOrSuggestInConsole(
          mojom::APIPermissionID::kFullscreen, extension,
          source->GetPrimaryMainFrame())) {
    return;
  }

  SetFullscreen(FULLSCREEN_TYPE_HTML_API, enter_fullscreen);
}

bool AppWindow::IsFullscreenForTabOrPending(
    const content::WebContents* source) {
  return IsHtmlApiFullscreen();
}

blink::mojom::DisplayMode AppWindow::GetDisplayMode(
    const content::WebContents* source) {
  return IsFullscreen() ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kStandalone;
}

WindowController* AppWindow::GetExtensionWindowController() const {
  return app_window_contents_->GetWindowController();
}

content::WebContents* AppWindow::GetAssociatedWebContents() const {
  return web_contents();
}

void AppWindow::OnExtensionUnloaded(BrowserContext* browser_context,
                                    const Extension* extension,
                                    UnloadedExtensionReason reason) {
  if (extension_id_ == extension->id())
    native_app_window_->Close();
}

void AppWindow::SetWebContentsBlocked(content::WebContents* web_contents,
                                      bool blocked) {
  app_delegate_->SetWebContentsBlocked(web_contents, blocked);
}

bool AppWindow::IsWebContentsVisible(content::WebContents* web_contents) {
  return app_delegate_->IsWebContentsVisible(web_contents);
}

WebContentsModalDialogHost* AppWindow::GetWebContentsModalDialogHost() {
  return native_app_window_.get();
}

void AppWindow::SaveWindowPosition() {
  DCHECK(native_app_window_);
  if (window_key_.empty())
    return;

  AppWindowGeometryCache* cache =
      AppWindowGeometryCache::Get(browser_context());

  gfx::Rect bounds = native_app_window_->GetRestoredBounds();
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(bounds).work_area();
  ui::mojom::WindowShowState window_state =
      native_app_window_->GetRestoredState();
  cache->SaveGeometry(extension_id(), window_key_, bounds, screen_bounds,
                      window_state);
}

void AppWindow::AdjustBoundsToBeVisibleOnScreen(
    const gfx::Rect& cached_bounds,
    const gfx::Rect& cached_screen_bounds,
    const gfx::Rect& current_screen_bounds,
    const gfx::Size& minimum_size,
    gfx::Rect* bounds) const {
  *bounds = cached_bounds;

  // Reposition and resize the bounds if the cached_screen_bounds is different
  // from the current screen bounds and the current screen bounds doesn't
  // completely contain the bounds.
  if (cached_screen_bounds != current_screen_bounds &&
      !current_screen_bounds.Contains(cached_bounds)) {
    bounds->set_width(
        std::max(minimum_size.width(),
                 std::min(bounds->width(), current_screen_bounds.width())));
    bounds->set_height(
        std::max(minimum_size.height(),
                 std::min(bounds->height(), current_screen_bounds.height())));
    bounds->set_x(std::max(current_screen_bounds.x(),
                           std::min(bounds->x(), current_screen_bounds.right() -
                                                     bounds->width())));
    bounds->set_y(
        std::max(current_screen_bounds.y(),
                 std::min(bounds->y(),
                          current_screen_bounds.bottom() - bounds->height())));
  }
}

AppWindow::CreateParams AppWindow::LoadDefaults(CreateParams params) const {
  // Ensure width and height are specified.
  if (params.content_spec.bounds.width() == 0 &&
      params.window_spec.bounds.width() == 0) {
    params.content_spec.bounds.set_width(kDefaultWidth);
  }
  if (params.content_spec.bounds.height() == 0 &&
      params.window_spec.bounds.height() == 0) {
    params.content_spec.bounds.set_height(kDefaultHeight);
  }

  // If left and top are left undefined, the native app window will center
  // the window on the main screen in a platform-defined manner.

  // Load cached state if it exists.
  if (!params.window_key.empty()) {
    AppWindowGeometryCache* cache =
        AppWindowGeometryCache::Get(browser_context());

    gfx::Rect cached_bounds;
    gfx::Rect cached_screen_bounds;
    ui::mojom::WindowShowState cached_state =
        ui::mojom::WindowShowState::kDefault;
    if (cache->GetGeometry(extension_id(), params.window_key, &cached_bounds,
                           &cached_screen_bounds, &cached_state)) {
      // App window has cached screen bounds, make sure it fits on screen in
      // case the screen resolution changed.
      display::Screen* screen = display::Screen::GetScreen();
      display::Display display = screen->GetDisplayMatching(cached_bounds);
      gfx::Rect current_screen_bounds = display.work_area();
      SizeConstraints constraints(
          params.GetWindowMinimumSize(gfx::Insets(), gfx::RoundedCornersF()),
          params.GetWindowMaximumSize(gfx::Insets(), gfx::RoundedCornersF()));
      AdjustBoundsToBeVisibleOnScreen(
          cached_bounds, cached_screen_bounds, current_screen_bounds,
          constraints.GetMinimumSize(), &params.window_spec.bounds);
      params.state = cached_state;

      // Since we are restoring a cached state, reset the content bounds spec to
      // ensure it is not used.
      params.content_spec.ResetBounds();
    }
  }

  return params;
}

// static
SkRegion* AppWindow::RawDraggableRegionsToSkRegion(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  SkRegion* sk_region = new SkRegion;
  for (const auto& region : regions) {
    sk_region->op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.right(), region->bounds.bottom()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  return sk_region;
}

}  // namespace extensions
