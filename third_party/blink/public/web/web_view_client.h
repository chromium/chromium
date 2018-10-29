/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_VIEW_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_VIEW_CLIENT_H_

#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {

class WebDateTimeChooserCompletion;
class WebFileChooserCompletion;
class WebNode;
class WebURL;
class WebURLRequest;
class WebView;
class WebWidget;
enum class WebSandboxFlags;
struct WebDateTimeChooserParams;
struct WebRect;
struct WebSize;
struct WebWindowFeatures;

class WebViewClient {
 public:
  virtual ~WebViewClient() = default;
  // Factory methods -----------------------------------------------------

  // Create a new related WebView.  This method must clone its session storage
  // so any subsequent calls to createSessionStorageNamespace conform to the
  // WebStorage specification.
  // The request parameter is only for the client to check if the request
  // could be fulfilled.  The client should not load the request.
  // The policy parameter indicates how the new view will be displayed in
  // WebWidgetClient::show.
  virtual WebView* CreateView(
      WebLocalFrame* creator,
      const WebURLRequest& request,
      const WebWindowFeatures& features,
      const WebString& name,
      WebNavigationPolicy policy,
      bool suppress_opener,
      WebSandboxFlags,
      const SessionStorageNamespaceId& session_storage_namespace_id) {
    return nullptr;
  }

  // Create a new popup WebWidget.
  virtual WebWidget* CreatePopup(WebLocalFrame*) { return nullptr; }

  // Returns the session storage namespace id associated with this WebView.
  virtual base::StringPiece GetSessionStorageNamespaceId() {
    return base::StringPiece();
  }

  // Misc ----------------------------------------------------------------

  // Called when script in the page calls window.print().  If frame is
  // non-null, then it selects a particular frame, including its
  // children, to print.  Otherwise, the main frame and its children
  // should be printed.
  virtual void PrintPage(WebLocalFrame*) {}

  // This method enumerates all the files in the path. It returns immediately
  // and asynchronously invokes the WebFileChooserCompletion with all the
  // files in the directory. Returns false if the WebFileChooserCompletion
  // will never be called.
  virtual bool EnumerateChosenDirectory(const WebString& path,
                                        WebFileChooserCompletion*) {
    return false;
  }

  // Called when PageImportanceSignals for the WebView is updated.
  virtual void PageImportanceSignalsChanged() {}

  // Called to get the position of the root window containing the widget
  // in screen coordinates.
  virtual WebRect RootWindowRect() { return WebRect(); }

  // Dialogs -------------------------------------------------------------

  // Ask users to choose date/time for the specified parameters. When a user
  // chooses a value, an implementation of this function should call
  // WebDateTimeChooserCompletion::didChooseValue or didCancelChooser. If the
  // implementation opened date/time chooser UI successfully, it should return
  // true. This function is used only if ExternalDateTimeChooser is used.
  virtual bool OpenDateTimeChooser(const WebDateTimeChooserParams&,
                                   WebDateTimeChooserCompletion*) {
    return false;
  }

  // UI ------------------------------------------------------------------

  // Called when hovering over an anchor with the given URL.
  virtual void SetMouseOverURL(const WebURL&) {}

  // Called when keyboard focus switches to an anchor with the given URL.
  virtual void SetKeyboardFocusURL(const WebURL&) {}

  // Called to determine if drag-n-drop operations may initiate a page
  // navigation.
  virtual bool AcceptsLoadDrops() { return true; }

  // Take focus away from the WebView by focusing an adjacent UI element
  // in the containing window.
  virtual void FocusNext() {}
  virtual void FocusPrevious() {}

  // Called when a new node gets focused. |fromNode| is the previously focused
  // node, |toNode| is the newly focused node. Either can be null.
  virtual void FocusedNodeChanged(const WebNode& from_node,
                                  const WebNode& to_node) {}

  // Called to check if layout update should be processed.
  virtual bool CanUpdateLayout() { return false; }

  // Indicates two things:
  //   1) This view may have a new layout now.
  //   2) Layout is up-to-date.
  // After calling WebWidget::updateAllLifecyclePhases(), expect to get this
  // notification unless the view did not need a layout.
  virtual void DidUpdateMainFrameLayout() {}

  // Return true to swallow the input event if the embedder will start a
  // disambiguation popup
  virtual bool DidTapMultipleTargets(const WebSize& visual_viewport_offset,
                                     const WebRect& touch_rect,
                                     const WebVector<WebRect>& target_rects) {
    return false;
  }

  // Returns comma separated list of accept languages.
  virtual WebString AcceptLanguages() { return WebString(); }

  // Called when the View has changed size as a result of an auto-resize.
  virtual void DidAutoResize(const WebSize& new_size) {}

  // Called when the View acquires focus.
  virtual void DidFocus(WebLocalFrame* calling_frame) {}

  // Returns information about the screen where this view's widgets are being
  // displayed.
  virtual WebScreenInfo GetScreenInfo() = 0;

  // Session history -----------------------------------------------------

  // Tells the embedder to navigate back or forward in session history by
  // the given offset (relative to the current position in session
  // history). |has_user_gesture| tells whether or not this is the consequence
  // of a user action.
  virtual void NavigateBackForwardSoon(int offset, bool has_user_gesture) {}

  // Returns the number of history items before/after the current
  // history item.
  virtual int HistoryBackListCount() { return 0; }
  virtual int HistoryForwardListCount() { return 0; }

  // Developer tools -----------------------------------------------------

  // Called to notify the client that the inspector's settings were
  // changed and should be saved.  See WebView::inspectorSettings.
  virtual void DidUpdateInspectorSettings() {}

  virtual void DidUpdateInspectorSetting(const WebString& key,
                                         const WebString& value) {}

  // Zoom ----------------------------------------------------------------

  // Informs the browser that the zoom levels for this frame have changed from
  // the default values.
  virtual void ZoomLimitsChanged(double minimum_level, double maximum_level) {}

  // Informs the browser that the page scale has changed.
  virtual void PageScaleFactorChanged() {}

  // Gestures -------------------------------------------------------------

  virtual bool CanHandleGestureEvent() { return false; }

  virtual WebWidgetClient* WidgetClient() = 0;
};

}  // namespace blink

#endif
