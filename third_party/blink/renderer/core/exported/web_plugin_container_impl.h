/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PLUGIN_CONTAINER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PLUGIN_CONTAINER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cc {
class Layer;
}

namespace blink {

class Event;
class GestureEvent;
class HTMLFrameOwnerElement;
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class LocalFrameView;
class MouseEvent;
class ResourceError;
class ResourceResponse;
class TouchEvent;
class WebKeyboardEvent;
class WebPlugin;
class WheelEvent;
struct WebPrintParams;
struct WebPrintPresetOptions;

class CORE_EXPORT WebPluginContainerImpl final
    : public GarbageCollected<WebPluginContainerImpl>,
      public EmbeddedContentView,
      public WebPluginContainer,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(WebPluginContainerImpl);
  USING_PRE_FINALIZER(WebPluginContainerImpl, PreFinalize);

 public:
  // Check if plugins support a given command |name|.
  static bool SupportsCommand(const WebString& name);

  WebPluginContainerImpl(HTMLPlugInElement&, WebPlugin*);
  ~WebPluginContainerImpl() override;

  // EmbeddedContentView methods
  bool IsPluginView() const override { return true; }
  LocalFrameView* ParentFrameView() const override;
  LayoutEmbeddedContent* GetLayoutEmbeddedContent() const override;
  void AttachToLayout() override;
  void DetachFromLayout() override;
  // |paint_offset| is used to to paint the contents at the correct location.
  // It should be issued as a transform operation before painting the contents.
  void Paint(GraphicsContext&,
             const GlobalPaintFlags,
             const CullRect&,
             const IntSize& paint_offset = IntSize()) const override;
  void UpdateGeometry() override;
  void Show() override;
  void Hide() override;

  cc::Layer* CcLayer() const;
  bool PreventContentsOpaqueChangesToCcLayer() const;
  v8::Local<v8::Object> ScriptableObject(v8::Isolate*);
  bool SupportsKeyboardFocus() const;
  bool SupportsInputMethod() const;
  bool CanProcessDrag() const;
  bool WantsWheelEvents() const;
  void UpdateAllLifecyclePhases();
  void InvalidateRect(const IntRect&);
  void SetFocused(bool, WebFocusType);
  void HandleEvent(Event&);
  bool IsErrorplaceholder();
  void EventListenersRemoved();
  void InvalidatePaint() {}

  // WebPluginContainer methods
  WebElement GetElement() override;
  WebDocument GetDocument() override;
  void DispatchProgressEvent(const WebString& type,
                             bool length_computable,
                             uint64_t loaded,
                             uint64_t total,
                             const WebString& url) override;
  void EnqueueMessageEvent(const WebDOMMessageEvent&) override;
  void Invalidate() override;
  void InvalidateRect(const WebRect&) override;
  void ScrollRect(const WebRect&) override;
  void ScheduleAnimation() override;
  void ReportGeometry() override;
  v8::Local<v8::Object> V8ObjectForElement() override;
  WebString ExecuteScriptURL(const WebURL&, bool popups_allowed) override;
  void LoadFrameRequest(const WebURLRequest&, const WebString& target) override;
  bool IsRectTopmost(const WebRect&) override;
  void RequestTouchEventType(TouchEventRequestType) override;
  void SetWantsWheelEvents(bool) override;
  WebPoint RootFrameToLocalPoint(const WebPoint&) override;
  WebPoint LocalToRootFramePoint(const WebPoint&) override;

  // Non-Oilpan, this cannot be null. With Oilpan, it will be
  // null when in a disposed state, pending finalization during the next GC.
  WebPlugin* Plugin() override { return web_plugin_; }
  void SetPlugin(WebPlugin*) override;

  void UsePluginAsFindHandler() override;
  void ReportFindInPageMatchCount(int identifier,
                                  int total,
                                  bool final_update) override;
  void ReportFindInPageSelection(int identifier, int index) override;

  float DeviceScaleFactor() override;
  float PageScaleFactor() override;
  float PageZoomFactor() override;

  void SetCcLayer(cc::Layer*, bool prevent_contents_opaque_changes) override;

  void RequestFullscreen() override;
  bool IsFullscreenElement() const override;
  void CancelFullscreen() override;

  // Printing interface. The plugin can support custom printing
  // (which means it controls the layout, number of pages etc).
  // Whether the plugin supports its own paginated print. The other print
  // interface methods are called only if this method returns true.
  bool SupportsPaginatedPrint() const;
  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  bool GetPrintPresetOptionsFromDocument(WebPrintPresetOptions*) const;
  // Sets up printing at the specified WebPrintParams. Returns the number of
  // pages to be printed at these settings.
  int PrintBegin(const WebPrintParams&) const;
  // Prints the page specified by pageNumber (0-based index) into the supplied
  // canvas.
  void PrintPage(int page_number, GraphicsContext&);
  // Ends the print operation.
  void PrintEnd();

  // Copy the selected text.
  void Copy();

  // Pass the edit command to the plugin.
  bool ExecuteEditCommand(const WebString& name);
  bool ExecuteEditCommand(const WebString& name, const WebString& value);

  // Resource load events for the plugin's source data:
  void DidReceiveResponse(const ResourceResponse&);
  void DidReceiveData(const char* data, size_t data_length);
  void DidFinishLoading();
  void DidFailLoading(const ResourceError&);

  void Trace(blink::Visitor*) override;
  // USING_PRE_FINALIZER does not allow for virtual dispatch from the finalizer
  // method. Here we call Dispose() which does the correct virtual dispatch.
  void PreFinalize() { Dispose(); }
  void Dispose() override;
  void SetFrameRect(const IntRect&) override;
  void PropagateFrameRects() override { ReportGeometry(); }

 protected:
  void ParentVisibleChanged() override;

 private:
  // Sets |windowRect| to the content rect of the plugin in screen space.
  // Sets |clippedAbsoluteRect| to the visible rect for the plugin, clipped to
  // the visible screen of the root frame, in local space of the plugin.
  // Sets |unclippedAbsoluteRect| to the visible rect for the plugin (but
  // without also clipping to the screen), in local space of the plugin.
  void ComputeClipRectsForPlugin(
      const HTMLFrameOwnerElement* plugin_owner_element,
      IntRect& window_rect,
      IntRect& clipped_local_rect,
      IntRect& unclipped_int_local_rect) const;

  WebTouchEvent TransformTouchEvent(const WebInputEvent&);
  WebCoalescedInputEvent TransformCoalescedTouchEvent(
      const WebCoalescedInputEvent&);

  void HandleMouseEvent(MouseEvent&);
  void HandleDragEvent(MouseEvent&);
  void HandleWheelEvent(WheelEvent&);
  void HandleKeyboardEvent(KeyboardEvent&);
  bool HandleCutCopyPasteKeyboardEvent(const WebKeyboardEvent&);
  void HandleTouchEvent(TouchEvent&);
  void HandleGestureEvent(GestureEvent&);

  void SynthesizeMouseEventIfPossible(TouchEvent&);

  void FocusPlugin();

  void CalculateGeometry(IntRect& window_rect,
                         IntRect& clip_rect,
                         IntRect& unobscured_rect);

  friend class WebPluginContainerTest;

  Member<HTMLPlugInElement> element_;
  WebPlugin* web_plugin_;
  cc::Layer* layer_;
  TouchEventRequestType touch_event_request_type_;
  bool prevent_contents_opaque_changes_;
  bool wants_wheel_events_;
};

DEFINE_TYPE_CASTS(WebPluginContainerImpl,
                  EmbeddedContentView,
                  embedded_content_view,
                  embedded_content_view->IsPluginView(),
                  embedded_content_view.IsPluginView());
// Unlike EmbeddedContentView, we need not worry about object type for
// container. WebPluginContainerImpl is the only subclass of WebPluginContainer.
DEFINE_TYPE_CASTS(WebPluginContainerImpl,
                  WebPluginContainer,
                  container,
                  true,
                  true);

}  // namespace blink

#endif
