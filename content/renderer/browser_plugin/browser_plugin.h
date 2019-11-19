// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/blink/public/web/web_plugin.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/frame_visual_properties.h"
#include "content/public/common/screen_info.h"
#include "content/renderer/child_frame_compositor.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/web/web_drag_status.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_node.h"

namespace cc {
class Layer;
class RenderFrameMetadata;
}

namespace content {

class BrowserPluginDelegate;
class BrowserPluginManager;
class ChildFrameCompositingHelper;

class CONTENT_EXPORT BrowserPlugin : public blink::WebPlugin,
                                     public ChildFrameCompositor,
                                     public MouseLockDispatcher::LockTarget {
 public:
  static BrowserPlugin* GetFromNode(const blink::WebNode& node);

  int render_frame_routing_id() const { return render_frame_routing_id_; }
  int browser_plugin_instance_id() const { return browser_plugin_instance_id_; }
  bool attached() const { return attached_; }

  bool OnMessageReceived(const IPC::Message& msg);

  // Update Browser Plugin's DOM Node attribute |attribute_name| with the value
  // |attribute_value|.
  void UpdateDOMAttribute(const std::string& attribute_name,
                          const base::string16& attribute_value);

  // Returns whether the guest process has crashed.
  bool guest_crashed() const { return guest_crashed_; }

  // Informs the guest of an updated focus state.
  void UpdateGuestFocusState(blink::WebFocusType focus_type);

  void ScreenInfoChanged(const ScreenInfo& screen_info);

  void OnZoomLevelChanged(double zoom_level);

  void UpdateCaptureSequenceNumber(uint32_t capture_sequence_number);

  // Indicates whether the guest should be focused.
  bool ShouldGuestBeFocused() const;

  // Provided that a guest instance ID has been allocated, this method attaches
  // this BrowserPlugin instance to that guest.
  void Attach();

  // This method detaches this BrowserPlugin instance from the guest that it's
  // currently attached to, if any.
  void Detach();

  // Returns the last allocated LocalSurfaceIdAllocation.
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation() const;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  void SynchronizeVisualProperties();

  // Returns whether a message should be forwarded to BrowserPlugin.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* Container() const override;
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  bool SupportsKeyboardFocus() const override;
  bool SupportsEditCommands() const override;
  bool SupportsInputMethod() const override;
  bool CanProcessDrag() const override;
  void UpdateAllLifecyclePhases(
      blink::WebWidget::LifecycleUpdateReason) override {}
  void Paint(cc::PaintCanvas* canvas, const blink::WebRect& rect) override {}
  void UpdateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::WebFocusType focus_type) override;
  void UpdateVisibility(bool visible) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;
  bool HandleDragStatusUpdate(blink::WebDragStatus drag_status,
                              const blink::WebDragData& drag_data,
                              blink::WebDragOperationsMask mask,
                              const blink::WebFloatPoint& position,
                              const blink::WebFloatPoint& screen) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;
  bool ExecuteEditCommand(const blink::WebString& name) override;
  bool ExecuteEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  bool SetComposition(
      const blink::WebString& text,
      const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
      const blink::WebRange& replacementRange,
      int selectionStart,
      int selectionEnd) override;
  bool CommitText(const blink::WebString& text,
                  const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
                  const blink::WebRange& replacementRange,
                  int relative_cursor_pos) override;
  bool FinishComposingText(
      blink::WebInputMethodController::ConfirmCompositionBehavior
          selection_behavior) override;

  void ExtendSelectionAndDelete(int before, int after) override;

  // MouseLockDispatcher::LockTarget implementation.
  void OnLockMouseACK(bool succeeded) override;
  void OnMouseLockLost() override;
  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override;

 private:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManager;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Once a BrowserPlugin
  // does an initial navigation or is attached to a newly created guest, it
  // acquires a browser_plugin_instance_id as well. The guest instance ID
  // uniquely identifies a guest WebContents that's hosted by this
  // BrowserPlugin.
  BrowserPlugin(RenderFrame* render_frame,
                const base::WeakPtr<BrowserPluginDelegate>& delegate);

  ~BrowserPlugin() override;

  const gfx::Rect& screen_space_rect() const {
    return pending_visual_properties_.screen_space_rect;
  }

  const ScreenInfo& screen_info() const {
    return pending_visual_properties_.screen_info;
  }

  void UpdateInternalInstanceId();

  // IPC message handlers.
  // Please keep in alphabetical order.
  void OnAdvanceFocus(int instance_id, bool reverse);
  void OnAttachACK(int browser_plugin_instance_id);
  void OnGuestGone(int instance_id);
  void OnGuestReady(int instance_id, const viz::FrameSinkId& frame_sink_id);
  void OnDidUpdateVisualProperties(int browser_plugin_instance_id,
                                   const cc::RenderFrameMetadata& metadata);
  void OnEnableAutoResize(int browser_plugin_instance_id,
                          const gfx::Size& min_size,
                          const gfx::Size& max_size);
  void OnDisableAutoResize(int browser_plugin_instance_id);
  void OnSetContentsOpaque(int instance_id, bool opaque);
  void OnSetCursor(int instance_id, const WebCursor& cursor);
  void OnSetMouseLock(int instance_id, bool enable);
  void OnShouldAcceptTouchEvents(int instance_id, bool accept);

  // ChildFrameCompositor:
  cc::Layer* GetLayer() override;
  void SetLayer(scoped_refptr<cc::Layer> layer,
                bool prevent_contents_opaque_changes,
                bool is_surface_layer) override;
  SkBitmap* GetSadPageBitmap() override;

  // This indicates whether this BrowserPlugin has been attached to a
  // WebContents and is ready to receive IPCs.
  bool attached_;
  // We cache the |render_frame_routing_id| because we need it on destruction.
  // If the RenderFrame is destroyed before the BrowserPlugin is destroyed
  // then we will attempt to access a nullptr.
  const int render_frame_routing_id_;
  blink::WebPluginContainer* container_;
  // The plugin's rect in css pixels.
  bool guest_crashed_;
  bool plugin_focused_;
  // Tracks the visibility of the browser plugin regardless of the whole
  // embedder RenderView's visibility.
  bool visible_;

  WebCursor cursor_;

  bool mouse_locked_;

  // This indicates that the BrowserPlugin has a geometry.
  bool ready_;

  // Used for HW compositing.
  std::unique_ptr<ChildFrameCompositingHelper> compositing_helper_;

  // URL for the embedder frame.
  int browser_plugin_instance_id_;

  std::vector<EditCommand> edit_commands_;

  viz::FrameSinkId frame_sink_id_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;

  // The last ResizeParams sent to the browser process, if any.
  base::Optional<FrameVisualProperties> sent_visual_properties_;

  // The current set of ResizeParams. This may or may not match
  // |sent_visual_properties_|.
  FrameVisualProperties pending_visual_properties_;

  // We call lifetime managing methods on |delegate_|, but we do not directly
  // own this. The delegate destroys itself.
  base::WeakPtr<BrowserPluginDelegate> delegate_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Pointer to the RenderWidget that embeds this plugin.
  base::WeakPtr<RenderWidget> embedding_render_widget_;

  // The layer used to embed the out-of-process content.
  scoped_refptr<cc::Layer> embedded_layer_;

  // Weak factory used in v8 |MakeWeak| callback, since the v8 callback might
  // get called after BrowserPlugin has been destroyed.
  base::WeakPtrFactory<BrowserPlugin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
