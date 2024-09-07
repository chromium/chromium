// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_PLUGIN_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_PLUGIN_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {
class WebGraphicsContext3DProvider;
struct WebPluginParams;
}  // namespace blink

namespace cc {
class CrossThreadSharedBitmap;
}

namespace gpu {

class ClientSharedImage;
class ClientSharedImageInterface;

namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
struct TransferableResource;
}

namespace content {
class TestRunner;

// A fake implementation of blink::WebPlugin for testing purposes.
//
// It uses GL to paint a scene consisiting of a primitive over a background. The
// primitive and background can be customized using the following plugin
// parameters.
// primitive: none (default), triangle.
// background-color: black (default), red, green, blue.
// primitive-color: black (default), red, green, blue.
// opacity: [0.0 - 1.0]. Default is 1.0.
//
// Whether the plugin accepts touch events or not can be customized using the
// 'accepts-touch' plugin parameter (defaults to false).
class TestPlugin : public blink::WebPlugin, public cc::TextureLayerClient {
 public:
  static TestPlugin* Create(const blink::WebPluginParams& params,
                            TestRunner* test_runner,
                            blink::WebLocalFrame* frame);

  TestPlugin(const TestPlugin&) = delete;
  TestPlugin& operator=(const TestPlugin&) = delete;

  ~TestPlugin() override;

  static const blink::WebString& MimeType();
  static const blink::WebString& CanCreateWithoutRendererMimeType();
  static const blink::WebString& PluginPersistsMimeType();
  static bool IsSupportedMimeType(const blink::WebString& mime_type);

  // WebPlugin methods:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  bool CanProcessDrag() const override;
  bool SupportsKeyboardFocus() const override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason) override {}
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override {}
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focus, blink::mojom::FocusType focus_type) override {}
  void UpdateVisibility(bool visibility) override {}
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  bool HandleDragStatusUpdate(blink::WebDragStatus drag_status,
                              const blink::WebDragData& data,
                              blink::DragOperationsMask mask,
                              const gfx::PointF& position,
                              const gfx::PointF& screen_position) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override {}
  void DidReceiveData(base::span<const char> data) override {}
  void DidFinishLoading() override {}
  void DidFailLoading(const blink::WebURLError& error) override {}
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*) override;

  // cc::TextureLayerClient methods:
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      viz::ReleaseCallback* release_callback) override;

 private:
  TestPlugin(const blink::WebPluginParams& params,
             TestRunner* test_runner,
             blink::WebLocalFrame* frame);

  enum Primitive { PrimitiveNone, PrimitiveTriangle };

  struct Scene {
    Primitive primitive;
    uint8_t background_color[3];
    uint8_t primitive_color[3];
    float opacity;

    GLuint vbo;
    GLuint program;
    int color_location;
    int position_location;

    Scene()
        : primitive(PrimitiveNone),
          opacity(1.0f)  // Fully opaque.
          ,
          vbo(0),
          program(0),
          color_location(-1),
          position_location(-1) {
      background_color[0] = background_color[1] = background_color[2] = 0;
      primitive_color[0] = primitive_color[1] = primitive_color[2] = 0;
    }
  };

  using ContextProviderRef = base::RefCountedData<
      std::unique_ptr<blink::WebGraphicsContext3DProvider>>;

  // Functions for parsing plugin parameters.
  Primitive ParsePrimitive(const blink::WebString& string);
  void ParseColor(const blink::WebString& string, uint8_t color[3]);
  float ParseOpacity(const blink::WebString& string);
  bool ParseBoolean(const blink::WebString& string);

  // Functions for loading and drawing scene in GL.
  bool InitScene();
  void DrawSceneGL();
  void DestroyScene();
  bool InitProgram();
  bool InitPrimitive();
  void DrawPrimitive();
  GLuint LoadShader(GLenum type, const std::string& source);
  GLuint LoadProgram(const std::string& vertex_source,
                     const std::string& fragment_source);

  // Functions for drawing scene in Software.
  void DrawSceneSoftware(void* memory);
  static void ReleaseSharedMemory(
      scoped_refptr<cc::CrossThreadSharedBitmap> shared_bitmap,
      cc::SharedBitmapIdRegistration registration,
      const gpu::SyncToken& sync_token,
      bool lost);
  static void ReleaseSharedImage(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      bool lost);

  raw_ptr<TestRunner> test_runner_;
  raw_ptr<blink::WebPluginContainer> container_;
  raw_ptr<blink::WebLocalFrame> web_local_frame_;

  gfx::Rect rect_;
  scoped_refptr<ContextProviderRef> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken sync_token_;
  scoped_refptr<cc::CrossThreadSharedBitmap> shared_bitmap_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;
  bool content_changed_ = false;
  GLuint framebuffer_ = 0;
  Scene scene_;
  scoped_refptr<cc::TextureLayer> layer_;

  v8::Persistent<v8::Object> scriptable_object_;

  blink::WebPluginContainer::TouchEventRequestType touch_event_request_ =
      blink::WebPluginContainer::kTouchEventRequestTypeNone;
  // Requests touch events from the WebPluginContainerImpl multiple times to
  // tickle webkit.org/b/108381
  bool re_request_touch_events_ = false;
  bool print_event_details_ = false;
  bool print_user_gesture_status_ = false;
  bool can_process_drag_ = false;
  bool supports_keyboard_focus_ = false;

  bool is_persistent_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_PLUGIN_H_
