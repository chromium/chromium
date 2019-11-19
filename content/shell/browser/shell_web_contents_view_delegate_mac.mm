// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_web_contents_view_delegate.h"

#import  <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/shell/browser/renderer_host/shell_render_widget_host_view_mac_delegate.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"

using blink::ContextMenuDataEditFlags;

enum {
  ShellContextMenuItemCutTag = 0,
  ShellContextMenuItemCopyTag,
  ShellContextMenuItemPasteTag,
  ShellContextMenuItemDeleteTag,
  ShellContextMenuItemOpenLinkTag,
  ShellContextMenuItemBackTag,
  ShellContextMenuItemForwardTag,
  ShellContextMenuItemReloadTag,
  ShellContextMenuItemInspectTag
};

@interface ShellContextMenuDelegate : NSObject<NSMenuDelegate> {
 @private
  content::ShellWebContentsViewDelegate* delegate_;
}
@end

@implementation ShellContextMenuDelegate
- (id)initWithDelegate:(content::ShellWebContentsViewDelegate*) delegate {
  if ((self = [super init])) {
    delegate_ = delegate;
  }
  return self;
}

- (void)itemSelected:(id)sender {
  NSInteger tag = [sender tag];
  delegate_->ActionPerformed(tag);
}
@end

namespace {

NSMenuItem* MakeContextMenuItem(NSString* title,
                                NSInteger tag,
                                NSMenu* menu,
                                BOOL enabled,
                                ShellContextMenuDelegate* delegate) {
  NSMenuItem* menu_item =
      [[NSMenuItem alloc] initWithTitle:title
                                 action:@selector(itemSelected:)
                          keyEquivalent:@""];
  [menu_item setTarget:delegate];
  [menu_item setTag:tag];
  [menu_item setEnabled:enabled];
  [menu addItem:menu_item];

  return menu_item;
}

}  // namespace

namespace content {

WebContentsViewDelegate* CreateShellWebContentsViewDelegate(
  WebContents* web_contents) {
  return new ShellWebContentsViewDelegate(web_contents);
}

ShellWebContentsViewDelegate::ShellWebContentsViewDelegate(
    WebContents* web_contents)
    : web_contents_(web_contents) {
}

ShellWebContentsViewDelegate::~ShellWebContentsViewDelegate() {
}

void ShellWebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  if (switches::IsRunWebTestsSwitchPresent())
    return;

  params_ = params;
  bool has_link = !params_.unfiltered_link_url.is_empty();
  bool has_selection = ! params_.selection_text.empty();

  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  ShellContextMenuDelegate* delegate =
      [[ShellContextMenuDelegate alloc] initWithDelegate:this];
  [menu setDelegate:delegate];
  [menu setAutoenablesItems:NO];

  if (params.media_type == blink::ContextMenuDataMediaType::kNone &&
      !has_link && !has_selection && !params_.is_editable) {
    BOOL back_menu_enabled =
        web_contents_->GetController().CanGoBack() ? YES : NO;
    MakeContextMenuItem(@"Back",
                        ShellContextMenuItemBackTag,
                        menu,
                        back_menu_enabled,
                        delegate);

    BOOL forward_menu_enabled =
        web_contents_->GetController().CanGoForward() ? YES : NO;
    MakeContextMenuItem(@"Forward",
                        ShellContextMenuItemForwardTag,
                        menu,
                        forward_menu_enabled,
                        delegate);

    MakeContextMenuItem(@"Reload",
                        ShellContextMenuItemReloadTag,
                        menu,
                        YES,
                        delegate);

    NSMenuItem* separator = [NSMenuItem separatorItem];
    [menu addItem:separator];
  }

  if (has_link) {
    MakeContextMenuItem(@"Open In New Window",
                        ShellContextMenuItemOpenLinkTag,
                        menu,
                        YES,
                        delegate);

    NSMenuItem* separator = [NSMenuItem separatorItem];
    [menu addItem:separator];
  }

  if (params_.is_editable) {
    BOOL cut_menu_enabled =
        (params_.edit_flags & ContextMenuDataEditFlags::kCanCut) ? YES : NO;
    MakeContextMenuItem(@"Cut",
                        ShellContextMenuItemCutTag,
                        menu,
                        cut_menu_enabled,
                        delegate);

    BOOL copy_menu_enabled =
        (params_.edit_flags & ContextMenuDataEditFlags::kCanCopy) ? YES : NO;
    MakeContextMenuItem(@"Copy",
                        ShellContextMenuItemCopyTag,
                        menu,
                        copy_menu_enabled,
                        delegate);

    BOOL paste_menu_enabled =
        (params_.edit_flags & ContextMenuDataEditFlags::kCanPaste) ? YES : NO;
    MakeContextMenuItem(@"Paste",
                        ShellContextMenuItemPasteTag,
                        menu,
                        paste_menu_enabled,
                        delegate);

    BOOL delete_menu_enabled =
        (params_.edit_flags & ContextMenuDataEditFlags::kCanDelete) ? YES : NO;
    MakeContextMenuItem(@"Delete",
                        ShellContextMenuItemDeleteTag,
                        menu,
                        delete_menu_enabled,
                        delegate);

    NSMenuItem* separator = [NSMenuItem separatorItem];
    [menu addItem:separator];
  } else if (has_selection) {
    MakeContextMenuItem(@"Copy",
                        ShellContextMenuItemCopyTag,
                        menu,
                        YES,
                        delegate);

    NSMenuItem* separator = [NSMenuItem separatorItem];
    [menu addItem:separator];
  }

  MakeContextMenuItem(@"Inspect",
                      ShellContextMenuItemInspectTag,
                      menu,
                      YES,
                      delegate);

  NSView* parent_view = web_contents_->GetContentNativeView().GetNativeNSView();
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                           location:position
                                      modifierFlags:NSRightMouseDownMask
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  [NSMenu popUpContextMenu:menu
                 withEvent:clickEvent
                   forView:parent_view];
}

void ShellWebContentsViewDelegate::ActionPerformed(int tag) {
  switch (tag) {
    case ShellContextMenuItemCutTag:
      web_contents_->Cut();
      break;
    case ShellContextMenuItemCopyTag:
      web_contents_->Copy();
      break;
    case ShellContextMenuItemPasteTag:
      web_contents_->Paste();
      break;
    case ShellContextMenuItemDeleteTag:
      web_contents_->Delete();
      break;
    case ShellContextMenuItemOpenLinkTag: {
      ShellBrowserContext* browser_context =
          ShellContentBrowserClient::Get()->browser_context();
      Shell::CreateNewWindow(browser_context, params_.link_url, nullptr,
                             gfx::Size());
      break;
    }
    case ShellContextMenuItemBackTag:
      web_contents_->GetController().GoToOffset(-1);
      web_contents_->Focus();
      break;
    case ShellContextMenuItemForwardTag:
      web_contents_->GetController().GoToOffset(1);
      web_contents_->Focus();
      break;
    case ShellContextMenuItemReloadTag: {
      web_contents_->GetController().Reload(ReloadType::NORMAL, false);
      web_contents_->Focus();
      break;
    }
    case ShellContextMenuItemInspectTag: {
      ShellDevToolsFrontend::Show(web_contents_);
      break;
    }
  }
}

NSObject<RenderWidgetHostViewMacDelegate>*
ShellWebContentsViewDelegate::CreateRenderWidgetHostViewDelegate(
    content::RenderWidgetHost* render_widget_host,
    bool is_popup) {
  return [[ShellRenderWidgetHostViewMacDelegate alloc] init];
}

}  // namespace content
