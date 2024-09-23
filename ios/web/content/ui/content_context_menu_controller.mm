// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/ui/content_context_menu_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "content/public/browser/context_menu_params.h"
#import "content/public/browser/render_widget_host_view.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/web_state/content_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state_delegate.h"
#import "services/network/public/mojom/referrer_policy.mojom.h"

// A hidden button used only for creating context menus. The only way to
// programmatically trigger a context menu on iOS is to trigger the primary
// action of a button that shows a context menu as its primary action.
@interface ContextMenuHiddenButton : UIButton

// The frame determines the position at which the context menu is shown.
+ (instancetype)buttonWithFrame:(CGRect)frame
              contextMenuParams:(content::ContextMenuParams)params
                 forWebContents:(content::WebContents*)webContents
                        forView:(UIView*)view;
@end

@implementation ContextMenuHiddenButton {
  content::ContextMenuParams _params;
  base::WeakPtr<content::WebContents> _webContents;
  UIView* _view;
}

+ (instancetype)buttonWithFrame:(CGRect)frame
              contextMenuParams:(content::ContextMenuParams)params
                 forWebContents:(content::WebContents*)webContents
                        forView:(UIView*)view {
  ContextMenuHiddenButton* button =
      [ContextMenuHiddenButton buttonWithType:UIButtonTypeSystem];
  button.hidden = YES;
  button.userInteractionEnabled = NO;
  button.contextMenuInteractionEnabled = YES;
  button.showsMenuAsPrimaryAction = YES;
  button.frame = frame;
  button.layer.zPosition = CGFLOAT_MIN;
  button->_params = params;
  button->_webContents = webContents->GetWeakPtr();
  button->_view = view;
  return button;
}

#pragma mark - Private

- (NSString*)convertToNSString:(const std::u16string&)string {
  return [[NSString alloc] initWithBytes:string.data()
                                  length:string.size() * sizeof(char16_t)
                                encoding:NSUTF16LittleEndianStringEncoding];
}

- (web::ReferrerPolicy)convertToWebReferrerPolicy:
    (network::mojom::ReferrerPolicy)policy {
  switch (policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return web::ReferrerPolicy::ReferrerPolicyAlways;
    case network::mojom::ReferrerPolicy::kDefault:
      return web::ReferrerPolicy::ReferrerPolicyDefault;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return web::ReferrerPolicy::ReferrerPolicyNoReferrerWhenDowngrade;
    case network::mojom::ReferrerPolicy::kNever:
      return web::ReferrerPolicy::ReferrerPolicyNever;
    case network::mojom::ReferrerPolicy::kOrigin:
      return web::ReferrerPolicy::ReferrerPolicyOrigin;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return web::ReferrerPolicy::ReferrerPolicyOriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return web::ReferrerPolicy::ReferrerPolicyStrictOriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return web::ReferrerPolicy::ReferrerPolicySameOrigin;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return web::ReferrerPolicy::ReferrerPolicyStrictOrigin;
  }
  NOTREACHED_IN_MIGRATION();
  return web::ReferrerPolicy::ReferrerPolicyDefault;
}

- (web::ContextMenuParams)webContextMenuParams {
  // TODO(crbug.com/333767962): The 'title_attribute' is intentionally not set
  // to match the Chrome on WebKit behavior that the link URL is displayed at
  // the top of the context menu.
  web::ContextMenuParams web_params;
  web_params.is_main_frame = !_params.is_subframe;
  web_params.link_url = _params.link_url;
  web_params.referrer_policy =
      [self convertToWebReferrerPolicy:_params.referrer_policy];
  web_params.src_url = _params.src_url;
  web_params.view = _view;
  web_params.location.x = _params.x;
  web_params.location.y = _params.y;
  web_params.text = [self convertToNSString:_params.link_text];
  web_params.alt_text = [self convertToNSString:_params.alt_text];
  return web_params;
}

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  web::ContentWebState* web_state =
      static_cast<web::ContentWebState*>(_webContents->GetDelegate());
  DCHECK(web_state);

  __block UIContextMenuConfiguration* config = nil;
  if (web_state && web_state->GetDelegate()) {
    web_state->GetDelegate()->ContextMenuConfiguration(
        web_state, [self webContextMenuParams],
        ^(UIContextMenuConfiguration* conf) {
          config = conf;
        });
  }

  [super contextMenuInteraction:interaction
      configurationForMenuAtLocation:location];
  return config;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  [super contextMenuInteraction:interaction
        willEndForConfiguration:configuration
                       animator:animator];
  if (_webContents) {
    _webContents->NotifyContextMenuClosed(_params.link_followed);
  }
}

@end

namespace {

gfx::NativeView GetContentNativeView(content::WebContents* web_contents) {
  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return gfx::NativeView();
  }
  return rwhv->GetNativeView();
}

}  // namespace

class IOSWebContentsUIButtonHolder {
 public:
  UIButton* __strong button_;
};

ContentContextMenuController::ContentContextMenuController() {
  hidden_button_ = std::make_unique<IOSWebContentsUIButtonHolder>();
}

ContentContextMenuController::~ContentContextMenuController() = default;

void ContentContextMenuController::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);

  UIView* view = base::apple::ObjCCastStrict<UIView>(
      GetContentNativeView(web_contents).Get());
  CGRect frame = CGRectMake(params.x, params.y, 0, 0);

  [hidden_button_->button_ removeFromSuperview];
  hidden_button_->button_ =
      [ContextMenuHiddenButton buttonWithFrame:frame
                             contextMenuParams:params
                                forWebContents:web_contents
                                       forView:view];
  [view addSubview:hidden_button_->button_];
  [hidden_button_->button_ performPrimaryAction];
}
