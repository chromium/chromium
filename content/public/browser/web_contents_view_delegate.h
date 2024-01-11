// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "build/build_config.h"

#if defined(__OBJC__)
#if BUILDFLAG(IS_MAC)
#import <Cocoa/Cocoa.h>
#endif
#endif

#include <optional>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
#if BUILDFLAG(IS_MAC)
@protocol RenderWidgetHostViewMacDelegate;
#endif
#endif

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
class WebDragDestDelegate;
struct ContextMenuParams;
struct DropData;

// This interface allows a client to extend the functionality of the
// WebContentsView implementation.
class CONTENT_EXPORT WebContentsViewDelegate {
 public:
  // Callback used with OnPerformingDrop() method that is called once
  // OnPerformingDrop() completes. Returns an updated DropData or nothing if
  // the drop operation should be aborted.
  using DropCompletionCallback =
      base::OnceCallback<void(std::optional<DropData>)>;

  virtual ~WebContentsViewDelegate();

  // Returns the native window containing the WebContents, or nullptr if the
  // WebContents is not in any window.
  virtual gfx::NativeWindow GetNativeWindow();

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate();

  // Shows a context menu.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  virtual void ShowContextMenu(RenderFrameHost& render_frame_host,
                               const ContextMenuParams& params);

  // Dismiss the context menu if one exists.
  virtual void DismissContextMenu();

  // Tests can use ExecuteCommandForTesting to simulate executing a context menu
  // item (after first opening the context menu using the ShowContextMenu
  // method).
  virtual void ExecuteCommandForTesting(int command_id, int event_flags);

  // Store the current focused view and start tracking it.
  virtual void StoreFocus();

  // Restore focus to stored view if possible, return true if successful.
  virtual bool RestoreFocus();

  // Clears any stored focus.
  virtual void ResetStoredFocus();

  // Allows the delegate to intercept a request to focus the WebContents,
  // and focus something else instead. Returns true when intercepted.
  virtual bool Focus();

  // Advance focus to the view that follows or precedes the WebContents.
  virtual bool TakeFocus(bool reverse);

  // Returns a newly-created, autoreleased delegate for the
  // RenderWidgetHostViewMac, to handle events on the responder chain.
#if defined(__OBJC__)
#if BUILDFLAG(IS_MAC)
  virtual NSObject<RenderWidgetHostViewMacDelegate>* GetDelegateForHost(
      RenderWidgetHost* render_widget_host,
      bool is_popup);
#endif
#else
  virtual void* GetDelegateForHost(RenderWidgetHost* render_widget_host,
                                   bool is_popup);
#endif

  // Performs the actions needed for a drop and then calls the completion
  // callback once done.
  virtual void OnPerformingDrop(const DropData& drop_data,
                                DropCompletionCallback callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
