// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/sys_string_conversions.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/app_shim_remote_cocoa/web_menu_runner_mac.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/mac/attributed_string_type_converters.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/attributed_string.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

// The interface class used to override the implementation of some of
// RenderWidgetHostViewCocoa methods for tests.
@interface RenderWidgetHostViewCocoaSwizzler : NSObject
- (void)showDefinitionForAttributedString:(NSAttributedString*)attrString
                                  atPoint:(NSPoint)textBaselineOrigin;
@end

namespace content {

using base::apple::ScopedObjCClassSwizzler;

// static
constexpr char
    RenderWidgetHostViewCocoaObserver::kShowDefinitionForAttributedString[];

// static
std::map<std::string, std::unique_ptr<base::apple::ScopedObjCClassSwizzler>>
    RenderWidgetHostViewCocoaObserver::rwhvcocoa_swizzlers_;

// static
std::map<WebContents*, RenderWidgetHostViewCocoaObserver*>
    RenderWidgetHostViewCocoaObserver::observers_;

namespace {

content::RenderWidgetHostViewMac* GetRenderWidgetHostViewMac(
    WebContents* contents) {
  auto* rwhv_base = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());
  if (rwhv_base && !rwhv_base->IsRenderWidgetHostViewChildFrame()) {
    return static_cast<RenderWidgetHostViewMac*>(rwhv_base);
  }
  return nil;
}

RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa(WebContents* contents) {
  content::RenderWidgetHostViewMac* rwhv_mac =
      GetRenderWidgetHostViewMac(contents);
  if (!rwhv_mac) {
    return nil;
  }
  return rwhv_mac->GetInProcessNSView();
}

content::RenderWidgetHostViewMac* GetRenderWidgetHostViewMac(NSObject* object) {
  for (auto* contents : WebContentsImpl::GetAllWebContents()) {
    content::RenderWidgetHostViewMac* rwhv_mac =
        GetRenderWidgetHostViewMac(contents);
    if (rwhv_mac && rwhv_mac->GetInProcessNSView() == object) {
      return rwhv_mac;
    }
  }
  return nullptr;
}

}  // namespace

base::apple::ScopedObjCClassSwizzler*
RenderWidgetHostViewCocoaObserver::GetSwizzler(const std::string& method_name) {
  return rwhvcocoa_swizzlers_.count(method_name)
             ? rwhvcocoa_swizzlers_.at(method_name).get()
             : nullptr;
}

// static
RenderWidgetHostViewCocoaObserver*
RenderWidgetHostViewCocoaObserver::GetObserver(WebContents* web_contents) {
  return observers_.count(web_contents) ? observers_.at(web_contents) : nullptr;
}

RenderWidgetHostViewCocoaObserver::RenderWidgetHostViewCocoaObserver(
    WebContents* web_contents)
    : web_contents_(web_contents) {
  if (rwhvcocoa_swizzlers_.empty()) {
    SetUpSwizzlers();
  }

  MenuWasRunCallback callback = base::BindRepeating(
      [](RenderWidgetHostViewCocoaObserver* observer, NSView* view,
         NSRect bounds, int index) {
        RenderWidgetHostViewCocoa* rwhv_cocoa =
            base::apple::ObjCCast<RenderWidgetHostViewCocoa>(view);
        gfx::Rect rect = [rwhv_cocoa flipNSRectToRect:bounds];
        observer->DidAttemptToShowPopup(rect, index);
      },
      this);
  [WebMenuRunner registerForTestingMenuRunCallback:callback
                                           forView:GetRenderWidgetHostViewCocoa(
                                                       web_contents)];

  DCHECK(!observers_.count(web_contents));
  observers_[web_contents] = this;
}

RenderWidgetHostViewCocoaObserver::~RenderWidgetHostViewCocoaObserver() {
  [WebMenuRunner
      unregisterForTestingMenuRunCallbackForView:GetRenderWidgetHostViewCocoa(
                                                     web_contents_)];

  observers_.erase(web_contents_);

  if (observers_.empty()) {
    rwhvcocoa_swizzlers_.clear();
  }
}

void RenderWidgetHostViewCocoaObserver::SetUpSwizzlers() {
  if (!rwhvcocoa_swizzlers_.empty()) {
    return;
  }

  // [RenderWidgetHostViewCocoa showDefinitionForAttributedString:atPoint].
  rwhvcocoa_swizzlers_[kShowDefinitionForAttributedString] =
      std::make_unique<ScopedObjCClassSwizzler>(
          GetRenderWidgetHostViewCocoaClassForTesting(),
          [RenderWidgetHostViewCocoaSwizzler class],
          NSSelectorFromString(@(kShowDefinitionForAttributedString)));
}

void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds) {
  NSRect new_bounds = bounds.ToCGRect();
  if (NSScreen.screens.count > 0) {
    new_bounds.origin.y = NSScreen.screens.firstObject.frame.size.height -
                          new_bounds.origin.y - new_bounds.size.height;
  }

  [window.GetNativeNSWindow() setFrame:new_bounds display:NO];
}

void GetStringAtPointForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Point& point,
    base::OnceCallback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      rwh, point,
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&, const gfx::Point&)>
                 callback,
             ui::mojom::AttributedStringPtr attributed_string,
             const gfx::Point& baseline_point) {
            std::string string =
                attributed_string
                    ? base::SysCFStringRefToUTF8(CFAttributedStringGetString(
                          attributed_string.To<CFAttributedStringRef>()))
                    : std::string();
            std::move(callback).Run(string, baseline_point);
          },
          std::move(result_callback)));
}

void GetStringFromRangeForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Range& range,
    base::OnceCallback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringFromRange(
      rwh, range,
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&, const gfx::Point&)>
                 callback,
             ui::mojom::AttributedStringPtr attributed_string,
             const gfx::Point& baseline_point) {
            std::string string =
                attributed_string
                    ? base::SysCFStringRefToUTF8(CFAttributedStringGetString(
                          attributed_string.To<CFAttributedStringRef>()))
                    : std::string();
            std::move(callback).Run(string, baseline_point);
          },
          std::move(result_callback)));
}

}  // namespace content

@implementation RenderWidgetHostViewCocoaSwizzler

- (void)showDefinitionForAttributedString:(NSAttributedString*)attrString
                                  atPoint:(NSPoint)textBaselineOrigin {
  content::RenderWidgetHostViewCocoaObserver::GetSwizzler(
      content::RenderWidgetHostViewCocoaObserver::
          kShowDefinitionForAttributedString)
      ->InvokeOriginal<void, NSAttributedString*, NSPoint>(
          self, _cmd, attrString, textBaselineOrigin);

  auto* rwhv_mac = content::GetRenderWidgetHostViewMac(self);

  auto* observer = content::RenderWidgetHostViewCocoaObserver::GetObserver(
      rwhv_mac->GetWebContents());

  if (!observer) {
    return;
  }
  observer->OnShowDefinitionForAttributedString(
      base::SysNSStringToUTF8(attrString.string));
}

@end
