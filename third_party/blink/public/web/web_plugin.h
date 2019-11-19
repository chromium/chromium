/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PLUGIN_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PLUGIN_H_

#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_drag_status.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "v8/include/v8.h"

namespace blink {

class WebCoalescedInputEvent;
class WebDragData;
class WebPluginContainer;
class WebURLResponse;
struct WebImeTextSpan;
struct WebCursorInfo;
struct WebPrintParams;
struct WebPrintPresetOptions;
struct WebPoint;
struct WebFloatPoint;
struct WebRect;
struct WebURLError;
template <typename T>
class WebVector;

class WebPlugin {
 public:
  // Initializes the plugin using |container| to communicate with the renderer
  // code. |container| must own this plugin. |container| must not be nullptr.
  //
  // Returns true if a plugin (not necessarily this one) has been successfully
  // initialized into |container|.
  //
  // NOTE: This method is subtle. This plugin may be marked for deletion via
  // destroy() during initialization. When this occurs, container() will
  // return nullptr. Because deletions during initialize() must be
  // asynchronous, this object is still alive immediately after initialize().
  //   1) If container() == nullptr and this method returns true, this plugin
  //      has been replaced by another during initialization. This new plugin
  //      may be accessed via container->plugin().
  //   2) If container() == nullptr and this method returns false, this plugin
  //      and the container have both been marked for deletion.
  virtual bool Initialize(WebPluginContainer*) = 0;

  // Plugins must arrange for themselves to be deleted sometime during or after
  // this method is called. This method is only called by the owning
  // WebPluginContainer.
  // The exception is if the plugin has been detached by a WebPluginContainer,
  // i.e. been replaced by another plugin. Then it must be destroyed separately.
  // Once this method has been called, container() must return nullptr.
  virtual void Destroy() = 0;

  // Returns the container that this plugin has been initialized with.
  // Must return nullptr if this plugin is scheduled for deletion.
  //
  // NOTE: This container doesn't necessarily own this plugin. For example,
  // if the container has been assigned a new plugin, then the container will
  // own the new plugin, not this old plugin.
  virtual WebPluginContainer* Container() const { return nullptr; }

  virtual v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*) {
    return v8::Local<v8::Object>();
  }

  virtual bool SupportsKeyboardFocus() const { return false; }
  virtual bool SupportsEditCommands() const { return false; }
  // Returns true if this plugin supports input method, which implements
  // setComposition(), commitText() and finishComposingText() below.
  virtual bool SupportsInputMethod() const { return false; }

  virtual bool CanProcessDrag() const { return false; }

  virtual void UpdateAllLifecyclePhases(WebWidget::LifecycleUpdateReason) = 0;
  virtual void Paint(cc::PaintCanvas*, const WebRect&) = 0;

  // Coordinates are relative to the containing window.
  virtual void UpdateGeometry(const WebRect& window_rect,
                              const WebRect& clip_rect,
                              const WebRect& unobscured_rect,
                              bool is_visible) = 0;

  virtual void UpdateFocus(bool focused, WebFocusType) = 0;

  virtual void UpdateVisibility(bool) = 0;

  virtual WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&,
                                               WebCursorInfo&) = 0;

  virtual bool HandleDragStatusUpdate(WebDragStatus,
                                      const WebDragData&,
                                      WebDragOperationsMask,
                                      const WebFloatPoint& position,
                                      const WebFloatPoint& screen_position) {
    return false;
  }

  virtual void DidReceiveResponse(const WebURLResponse&) = 0;
  virtual void DidReceiveData(const char* data, size_t data_length) = 0;
  virtual void DidFinishLoading() = 0;
  virtual void DidFailLoading(const WebURLError&) = 0;

  // Printing interface.
  // Whether the plugin supports its own paginated print. The other print
  // interface methods are called only if this method returns true.
  virtual bool SupportsPaginatedPrint() { return false; }
  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  virtual bool GetPrintPresetOptionsFromDocument(WebPrintPresetOptions*) {
    return false;
  }
  // Returns true if the plugin is a PDF plugin.
  virtual bool IsPdfPlugin() { return false; }

  // Sets up printing with the specified printParams. Returns the number of
  // pages to be printed at these settings.
  virtual int PrintBegin(const WebPrintParams& print_params) { return 0; }

  virtual void PrintPage(int page_number, cc::PaintCanvas* canvas) {}

  // Ends the print operation.
  virtual void PrintEnd() {}

  virtual bool HasSelection() const { return false; }
  virtual WebString SelectionAsText() const { return WebString(); }
  virtual WebString SelectionAsMarkup() const { return WebString(); }

  virtual bool CanEditText() const { return false; }
  virtual bool HasEditableText() const { return false; }

  virtual bool CanUndo() const { return false; }
  virtual bool CanRedo() const { return false; }

  virtual bool ExecuteEditCommand(const WebString& name) { return false; }
  virtual bool ExecuteEditCommand(const WebString& name,
                                  const WebString& value) {
    return false;
  }

  // Sets composition text from input method, and returns true if the
  // composition is set successfully. If |replacementRange| is not null, the
  // text inside |replacementRange| will be replaced by |text|
  virtual bool SetComposition(const WebString& text,
                              const WebVector<WebImeTextSpan>& ime_text_spans,
                              const WebRange& replacement_range,
                              int selection_start,
                              int selection_end) {
    return false;
  }

  // Deletes the ongoing composition if any, inserts the specified text, and
  // moves the caret according to relativeCaretPosition. If |replacementRange|
  // is not null, the text inside |replacementRange| will be replaced by |text|.
  virtual bool CommitText(const WebString& text,
                          const WebVector<WebImeTextSpan>& ime_text_spans,
                          const WebRange& replacement_range,
                          int relative_caret_position) {
    return false;
  }

  // Confirms an ongoing composition; holds or moves selections accroding to
  // selectionBehavior.
  virtual bool FinishComposingText(
      WebInputMethodController::ConfirmCompositionBehavior selection_behavior) {
    return false;
  }

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  virtual void ExtendSelectionAndDelete(int before, int after) {}
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in UTF-16 Code Unit, not in code points
  // or in glyphs.
  virtual void DeleteSurroundingText(int before, int after) {}
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in code points, not in UTF-16 Code Unit
  // or in glyphs. Do nothing if there are one or more invalid surrogate pairs
  // in the requested range.
  virtual void DeleteSurroundingTextInCodePoints(int before, int after) {}
  // If the given position is over a link, returns the absolute url.
  // Otherwise an empty url is returned.
  virtual WebURL LinkAtPosition(const WebPoint& position) const {
    return WebURL();
  }

  // Find interface.
  // Start a new search.  The plugin should search for a little bit at a time so
  // that it doesn't block the thread in case of a large document.  The results,
  // along with the find's identifier, should be sent asynchronously to
  // WebLocalFrameClient's reportFindInPage* methods.
  // Returns true if the search started, or false if the plugin doesn't support
  // search.
  virtual bool StartFind(const WebString& search_text,
                         bool case_sensitive,
                         int identifier) {
    return false;
  }
  // Tells the plugin to jump forward or backward in the list of find results.
  virtual void SelectFindResult(bool forward, int identifier) {}
  // Tells the plugin that the user has stopped the find operation.
  virtual void StopFind() {}

  // View rotation types.
  enum RotationType {
    kRotationType90Clockwise,
    kRotationType90Counterclockwise
  };
  // Whether the plugin can rotate the view of its content.
  virtual bool CanRotateView() { return false; }
  // Rotates the plugin's view of its content.
  virtual void RotateView(RotationType type) {}
  // Check whether a plugin can be interacted with. A positive return value
  // means the plugin has not loaded and hence cannot be interacted with.
  // The plugin could, however, load successfully later.
  virtual bool IsPlaceholder() { return true; }
  // Check whether a plugin failed to load, with there being no possibility of
  // it loading later.
  virtual bool IsErrorPlaceholder() { return false; }

 protected:
  virtual ~WebPlugin() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PLUGIN_H_
