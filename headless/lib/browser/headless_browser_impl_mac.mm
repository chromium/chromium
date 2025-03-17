// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/popup_menu.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_screen_mac.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"
#import "ui/base/cocoa/base_view.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

namespace headless {

namespace {

NSString* const kActivityReason = @"Batch headless process";
const NSActivityOptions kActivityOptions =
    (NSActivityUserInitiatedAllowingIdleSystemSleep |
     NSActivityLatencyCritical) &
    ~(NSActivitySuddenTerminationDisabled |
      NSActivityAutomaticTerminationDisabled);

}  // namespace

void HeadlessBrowserImpl::PlatformInitialize() {
  if (!geolocation_system_permission_manager_) {
    geolocation_system_permission_manager_ =
        device::SystemGeolocationSourceApple::
            CreateGeolocationSystemPermissionManager();
  }

  HeadlessScreen* screen = HeadlessScreenMac::Create(
      options()->window_size, options()->screen_info_spec);
  display::Screen::SetScreenInstance(screen);

  content::DontShowPopupMenus();
}

void HeadlessBrowserImpl::PlatformStart() {
  // Disallow headless to be throttled as a background process.
  [NSProcessInfo.processInfo beginActivityWithOptions:kActivityOptions
                                               reason:kActivityReason];
}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  NSView* web_view =
      web_contents->web_contents()->GetNativeView().GetNativeNSView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  // TODO(eseckler): Support enabling BeginFrameControl on Mac. This is tricky
  // because it's a ui::Compositor startup setting and ui::Compositors are
  // recycled on Mac, see browser_compositor_view_mac.mm.
}

void HeadlessBrowserImpl::PlatformSetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  content::WebContents* content_web_contents = web_contents->web_contents();
  NSView* ns_view = content_web_contents->GetNativeView().GetNativeNSView();

  // Note that by now -[NSScreen frame] implementation is overriden with
  // the headless screen aware version so vertical coordinates conversion works
  // correctly.
  ns_view.frame = gfx::ScreenRectToNSRect(bounds);

  // Render widget host view is not ready at this point, so post a task to set
  // bounds at later time.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> content_web_contents,
             const gfx::Rect& bounds) {
            if (content_web_contents) {
              content::RenderWidgetHostView* host_view =
                  content_web_contents->GetRenderWidgetHostView();
              if (host_view) {
                host_view->SetWindowFrameInScreen(bounds);
              }
            }
          },
          content_web_contents->GetWeakPtr(), bounds));
}

ui::Compositor* HeadlessBrowserImpl::PlatformGetCompositor(
    HeadlessWebContentsImpl* web_contents) {
  // TODO(eseckler): Support BeginFrameControl on Mac.
  return nullptr;
}

device::GeolocationSystemPermissionManager*
HeadlessBrowserImpl::GetGeolocationSystemPermissionManager() {
  return geolocation_system_permission_manager_.get();
}

void HeadlessBrowserImpl::SetGeolocationSystemPermissionManagerForTesting(
    std::unique_ptr<device::GeolocationSystemPermissionManager>
        fake_geolocation_system_permission_manager) {
  geolocation_system_permission_manager_ =
      std::move(fake_geolocation_system_permission_manager);
}

}  // namespace headless
