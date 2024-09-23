// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_web_contents_view_delegate.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"

enum {
  ShellContextMenuItemCutTag = 0,
  ShellContextMenuItemCopyTag,
  ShellContextMenuItemCopyLinkTag,
  ShellContextMenuItemPasteTag,
  ShellContextMenuItemDeleteTag,
  ShellContextMenuItemOpenLinkTag
};

// A hidden button used only for creating context menus. The only way to
// programmatically trigger a context menu on iOS is to trigger the primary
// action of a button that shows a context menu as its primary action.
@interface ContextMenuHiddenButton : UIButton

// The frame determines the position at which the context menu is shown.
+ (instancetype)buttonWithFrame:(CGRect)frame
              contextMenuParams:(content::ContextMenuParams)params
                 forWebContents:(content::WebContents*)webContents;
@end

@implementation ContextMenuHiddenButton {
  content::ContextMenuParams _params;
  base::WeakPtr<content::WebContents> _webContents;
}

+ (instancetype)buttonWithFrame:(CGRect)frame
              contextMenuParams:(content::ContextMenuParams)params
                 forWebContents:(content::WebContents*)webContents {
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
  return button;
}

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  UIContextMenuConfiguration* config = [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu* _Nullable(
                       NSArray<UIMenuElement*>* _Nonnull suggestedActions) {
                     return [self buildContextMenuItems];
                   }];
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

- (UIAction*)makeMenuItem:(NSString*)title menuTag:(NSInteger)tag {
  auto menuActionHandler = ^(UIAction* action) {
    switch (tag) {
      case ShellContextMenuItemCutTag:
        self->_webContents->Cut();
        break;
      case ShellContextMenuItemCopyTag:
        self->_webContents->Copy();
        break;
      case ShellContextMenuItemCopyLinkTag: {
        UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
        pasteboard.string = [NSString
            stringWithUTF8String:self->_params.link_url.spec().c_str()];
        break;
      }
      case ShellContextMenuItemPasteTag:
        self->_webContents->Paste();
        break;
      case ShellContextMenuItemDeleteTag:
        self->_webContents->Delete();
        break;
      case ShellContextMenuItemOpenLinkTag: {
        content::NavigationController::LoadURLParams params(
            self->_params.link_url);
        self->_webContents->GetController().LoadURLWithParams(params);
        break;
      }
    }
  };

  UIAction* menu = [UIAction actionWithTitle:title
                                       image:nil
                                  identifier:nil
                                     handler:menuActionHandler];
  return menu;
}

- (UIMenu*)buildContextMenuItems {
  bool hasLink = !_params.unfiltered_link_url.is_empty();
  bool hasSelection = !_params.selection_text.empty();
  bool isEditable = _params.is_editable;

  NSMutableArray* menuItems = [[NSMutableArray alloc] init];
  if (hasLink) {
    [menuItems addObject:[self makeMenuItem:@"Go to the Link"
                                    menuTag:ShellContextMenuItemOpenLinkTag]];
    [menuItems addObject:[self makeMenuItem:@"Copy Link"
                                    menuTag:ShellContextMenuItemCopyLinkTag]];
  }

  if (isEditable) {
    if (_params.edit_flags & blink::ContextMenuDataEditFlags::kCanCut) {
      [menuItems addObject:[self makeMenuItem:@"Cut"
                                      menuTag:ShellContextMenuItemCutTag]];
    }

    if (_params.edit_flags & blink::ContextMenuDataEditFlags::kCanCopy) {
      [menuItems addObject:[self makeMenuItem:@"Copy"
                                      menuTag:ShellContextMenuItemCopyTag]];
    }

    if (_params.edit_flags & blink::ContextMenuDataEditFlags::kCanPaste) {
      [menuItems addObject:[self makeMenuItem:@"Paste"
                                      menuTag:ShellContextMenuItemPasteTag]];
    }

    if (_params.edit_flags & blink::ContextMenuDataEditFlags::kCanDelete) {
      [menuItems addObject:[self makeMenuItem:@"Delete"
                                      menuTag:ShellContextMenuItemDeleteTag]];
    }
  } else if (hasSelection) {
    [menuItems addObject:[self makeMenuItem:@"Copy"
                                    menuTag:ShellContextMenuItemCopyTag]];
  }

  NSString* title =
      hasLink ? [NSString
                    stringWithUTF8String:self->_params.link_url.spec().c_str()]
              : @"";
  return [UIMenu menuWithTitle:title children:menuItems];
}

@end

namespace content {

namespace {

gfx::NativeView GetContentNativeView(WebContents* web_contents) {
  RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return gfx::NativeView();
  }
  return rwhv->GetNativeView();
}

}  // namespace

class ShellWebContentsUIButtonHolder {
 public:
  UIButton* __strong button_;
};

std::unique_ptr<WebContentsViewDelegate> CreateShellWebContentsViewDelegate(
    WebContents* web_contents) {
  return std::make_unique<ShellWebContentsViewDelegate>(web_contents);
}

ShellWebContentsViewDelegate::ShellWebContentsViewDelegate(
    WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);  // Avoids 'unused private field' build error.
  hidden_button_ = std::make_unique<ShellWebContentsUIButtonHolder>();
}

ShellWebContentsViewDelegate::~ShellWebContentsViewDelegate() {}

void ShellWebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {
  if (switches::IsRunWebTestsSwitchPresent()) {
    return;
  }

  UIView* view = base::apple::ObjCCastStrict<UIView>(
      GetContentNativeView(web_contents_).Get());
  CGRect frame = CGRectMake(params.x, params.y, 0, 0);

  [hidden_button_->button_ removeFromSuperview];
  hidden_button_->button_ =
      [ContextMenuHiddenButton buttonWithFrame:frame
                             contextMenuParams:params
                                forWebContents:web_contents_];
  [view addSubview:hidden_button_->button_];
  [hidden_button_->button_ performPrimaryAction];
}

}  // namespace content
