// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/c/pp_var.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
struct WebPluginParams;
struct WebPrintParams;
}

namespace content {

class PepperPluginInstanceImpl;
class PluginModule;
class RenderFrameImpl;

class PepperWebPluginImpl : public blink::WebPlugin {
 public:
  PepperWebPluginImpl(PluginModule* module,
                      const blink::WebPluginParams& params,
                      RenderFrameImpl* render_frame);

  PepperPluginInstanceImpl* instance() { return instance_.get(); }

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* Container() const override;
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  bool SupportsKeyboardFocus() const override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason) override {}
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visible) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError&) override;
  bool HasSelection() const override;
  blink::WebString SelectionAsText() const override;
  blink::WebString SelectionAsMarkup() const override;
  bool CanEditText() const override;
  bool HasEditableText() const override;
  bool CanUndo() const override;
  bool CanRedo() const override;
  bool ExecuteEditCommand(const blink::WebString& name) override;
  bool ExecuteEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  blink::WebURL LinkAtPosition(const gfx::Point& position) const override;
  bool GetPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* preset_options) override;
  bool IsPdfPlugin() override;
  bool StartFind(const blink::WebString& search_text,
                 bool case_sensitive,
                 int identifier) override;
  void SelectFindResult(bool forward, int identifier) override;
  void StopFind() override;
  bool SupportsPaginatedPrint() override;

  int PrintBegin(const blink::WebPrintParams& print_params) override;
  void PrintPage(int page_number, cc::PaintCanvas* canvas) override;
  void PrintEnd() override;

  bool CanRotateView() override;
  void RotateView(RotationType type) override;
  bool IsPlaceholder() override;
  void DidLoseMouseLock() override;
  void DidReceiveMouseLockResult(bool success) override;

  bool CanComposeInline() override;
  bool ShouldDispatchImeEventsToPlugin() override;
  blink::WebTextInputType GetPluginTextInputType() override;
  gfx::Rect GetPluginCaretBounds() override;
  void ImeSetCompositionForPlugin(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end) override;
  void ImeCommitTextForPlugin(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int relative_cursor_pos) override;
  void ImeFinishComposingTextForPlugin(bool keep_selection) override;

 private:
  friend class base::DeleteHelper<PepperWebPluginImpl>;

  ~PepperWebPluginImpl() override;

  // Cleared upon successful initialization.
  struct InitData;
  std::unique_ptr<InitData> init_data_;

  // True if the instance represents the entire document in a frame instead of
  // being an embedded resource.
  const bool full_frame_;

  scoped_refptr<PepperPluginInstanceImpl> instance_;
  gfx::Rect plugin_rect_;
  PP_Var instance_object_;
  blink::WebPluginContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(PepperWebPluginImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
