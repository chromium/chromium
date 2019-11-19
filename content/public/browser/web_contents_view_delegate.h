// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
@protocol RenderWidgetHostViewMacDelegate;
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
  enum class DropCompletionResult {
    // The drag and drop operation can continue normally.
    kContinue,

    // The drag and drop should be aborted.  For example, the data in the
    // drop does not comply with enterprise policies.
    kAbort,
  };

  // Callback used with OnPerformDrop() method that is called once
  // OnPerformDrop() completes.
  using DropCompletionCallback = base::OnceCallback<void(DropCompletionResult)>;

  virtual ~WebContentsViewDelegate();

  // Returns the native window containing the WebContents, or nullptr if the
  // WebContents is not in any window.
  virtual gfx::NativeWindow GetNativeWindow();

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate();

  // Shows a context menu.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params);

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

  // Returns a newly-created delegate for the RenderWidgetHostViewMac, to handle
  // events on the responder chain.
#if defined(__OBJC__)
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
  CreateRenderWidgetHostViewDelegate(RenderWidgetHost* render_widget_host,
                                     bool is_popup);
#else
  virtual void* CreateRenderWidgetHostViewDelegate(
      RenderWidgetHost* render_widget_host,
      bool is_popup);
#endif

  // Performs the actions needed for a drop and then calls the completion
  // callback once done.
  virtual void OnPerformDrop(const DropData& drop_data,
                             DropCompletionCallback callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
