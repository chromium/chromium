// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {
class WebGraphicsContext3DProvider;
struct WebPluginParams;
}

namespace cc {
class CrossThreadSharedBitmap;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace viz {
struct TransferableResource;
}

namespace test_runner {

class WebTestDelegate;

// A fake implemention of blink::WebPlugin for testing purposes.
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
                            WebTestDelegate* delegate,
                            blink::WebLocalFrame* frame);
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
  void UpdateAllLifecyclePhases(
      blink::WebWidget::LifecycleUpdateReason) override {}
  void Paint(cc::PaintCanvas* canvas, const blink::WebRect& rect) override {}
  void UpdateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focus, blink::WebFocusType focus_type) override {}
  void UpdateVisibility(bool visibility) override {}
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      blink::WebCursorInfo& info) override;
  bool HandleDragStatusUpdate(
      blink::WebDragStatus drag_status,
      const blink::WebDragData& data,
      blink::WebDragOperationsMask mask,
      const blink::WebFloatPoint& position,
      const blink::WebFloatPoint& screen_position) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override {}
  void DidReceiveData(const char* data, size_t data_length) override {}
  void DidFinishLoading() override {}
  void DidFailLoading(const blink::WebURLError& error) override {}
  bool IsPlaceholder() override;

  // cc::TextureLayerClient methods:
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override;

 private:
  TestPlugin(const blink::WebPluginParams& params,
             WebTestDelegate* delegate,
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
      scoped_refptr<ContextProviderRef> context_provider,
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token,
      bool lost);

  WebTestDelegate* delegate_;
  blink::WebPluginContainer* container_;
  blink::WebLocalFrame* web_local_frame_;

  blink::WebRect rect_;
  scoped_refptr<ContextProviderRef> context_provider_;
  gpu::gles2::GLES2Interface* gl_;
  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;
  scoped_refptr<cc::CrossThreadSharedBitmap> shared_bitmap_;
  bool content_changed_;
  GLuint framebuffer_;
  Scene scene_;
  scoped_refptr<cc::TextureLayer> layer_;

  blink::WebPluginContainer::TouchEventRequestType touch_event_request_;
  // Requests touch events from the WebPluginContainerImpl multiple times to
  // tickle webkit.org/b/108381
  bool re_request_touch_events_;
  bool print_event_details_;
  bool print_user_gesture_status_;
  bool can_process_drag_;
  bool supports_keyboard_focus_;

  bool is_persistent_;
  bool can_create_without_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TestPlugin);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_
