// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
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

  PepperWebPluginImpl(const PepperWebPluginImpl&) = delete;
  PepperWebPluginImpl& operator=(const PepperWebPluginImpl&) = delete;

  PepperPluginInstanceImpl* instance() { return instance_.get(); }

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* Container() const override;
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
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
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError&) override;
  bool HasSelection() const override;
  blink::WebString SelectionAsText() const override;
  blink::WebString SelectionAsMarkup() const override;
  bool SupportsPaginatedPrint() override;

  int PrintBegin(const blink::WebPrintParams& print_params) override;
  void PrintPage(int page_number, cc::PaintCanvas* canvas) override;
  void PrintEnd() override;

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
  raw_ptr<blink::WebPluginContainer> container_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
