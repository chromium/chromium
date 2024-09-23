// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/pepper_webplugin_impl.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_plugin_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/test/test_content_client.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {
namespace {

class PepperWebPluginImplBrowserTest : public RenderViewTest {
 public:
  PepperWebPluginImplBrowserTest()
      : pp_module_(0), pp_instance_(0), graphics2d_(0) {}

  void SetUp() override {
    current_test_ = this;
    RenderViewTest::SetUp();
  }
  void TearDown() override {
    RenderViewTest::TearDown();
    current_test_ = nullptr;
  }
  ContentClient* CreateContentClient() override {
    return new MockContentClient;
  }
  ContentRendererClient* CreateContentRendererClient() override {
    return new MockContentRendererClient;
  }

 protected:
  // PPP implementation
  static const void* GetInterface(const char* name) {
    static PPP_Instance ppp_instance = {
        &PepperWebPluginImplBrowserTest::DidCreate,
        &PepperWebPluginImplBrowserTest::DidDestroy,
        &PepperWebPluginImplBrowserTest::DidChangeView,
        &PepperWebPluginImplBrowserTest::DidChangeFocus,
        &PepperWebPluginImplBrowserTest::HandleDocumentLoad};
    if (!strcmp(name, PPP_INSTANCE_INTERFACE))
      return &ppp_instance;
    return nullptr;
  }
  static int InitializeModule(PP_Module module,
                              PPB_GetInterface get_interface) {
    EXPECT_EQ(0, current_test_->pp_module_);
    current_test_->pp_module_ = module;
    ppb_core_ = static_cast<const PPB_Core*>(get_interface(PPB_CORE_INTERFACE));
    ppb_graphics2d_ = static_cast<const PPB_Graphics2D*>(
        get_interface(PPB_GRAPHICS_2D_INTERFACE));
    ppb_image_data_ = static_cast<const PPB_ImageData*>(
        get_interface(PPB_IMAGEDATA_INTERFACE));
    ppb_instance_ =
        static_cast<const PPB_Instance*>(get_interface(PPB_INSTANCE_INTERFACE));
    return PP_OK;
  }
  static void ShutdownModule() {
    EXPECT_NE(0, current_test_->pp_module_);
    current_test_->pp_module_ = 0;
  }

  static void DummyCallback(void*, int32_t) {}

  void PaintSomething() {
    PP_Size size = {2, 1};
    PP_Resource image = ppb_image_data_->Create(
        pp_instance_, ppb_image_data_->GetNativeImageDataFormat(), &size,
        PP_TRUE);
    int32_t* pixels = static_cast<int32_t*>(ppb_image_data_->Map(image));
    pixels[0] = 0xff000000;
    pixels[1] = 0xffffffff;
    ppb_image_data_->Unmap(image);
    ppb_graphics2d_->ReplaceContents(graphics2d_, image);
    PP_CompletionCallback callback = {
        &PepperWebPluginImplBrowserTest::DummyCallback, nullptr, 0};
    ppb_graphics2d_->Flush(graphics2d_, callback);
    ppb_core_->ReleaseResource(image);
  }

  // PPP_Instance implementation
  static PP_Bool DidCreate(PP_Instance instance,
                           uint32_t,
                           const char* [],
                           const char* []) {
    EXPECT_EQ(0, current_test_->pp_instance_);
    current_test_->pp_instance_ = instance;
    PP_Size size = {2, 1};
    current_test_->graphics2d_ =
        ppb_graphics2d_->Create(instance, &size, PP_TRUE);
    ppb_instance_->BindGraphics(instance, current_test_->graphics2d_);
    return PP_TRUE;
  }
  static void DidDestroy(PP_Instance instance) {
    EXPECT_NE(0, current_test_->pp_instance_);
    current_test_->PaintSomething();
    ppb_core_->ReleaseResource(current_test_->graphics2d_);
    current_test_->pp_instance_ = 0;
  }
  static void DidChangeView(PP_Instance, PP_Resource) {}
  static void DidChangeFocus(PP_Instance, PP_Bool) {}
  static PP_Bool HandleDocumentLoad(PP_Instance, PP_Resource) {
    return PP_FALSE;
  }

  static ContentPluginInfo GetPluginInfo() {
    ContentPluginInfo info;
    info.is_internal = true;
    info.path = base::FilePath(FILE_PATH_LITERAL("internal-always-throttle"));
    info.name = "Always Throttle";
    info.mime_types.push_back(
        WebPluginMimeType("test/always-throttle", "", ""));
    info.internal_entry_points.get_interface =
        &PepperWebPluginImplBrowserTest::GetInterface;
    info.internal_entry_points.initialize_module =
        &PepperWebPluginImplBrowserTest::InitializeModule;
    info.internal_entry_points.shutdown_module =
        &PepperWebPluginImplBrowserTest::ShutdownModule;
    return info;
  }

  class MockContentClient : public TestContentClient {
   public:
    void AddPlugins(std::vector<ContentPluginInfo>* plugins) override {
      plugins->push_back(GetPluginInfo());
    }
  };
  class MockContentRendererClient : public ContentRendererClient {
   public:
    bool OverrideCreatePlugin(RenderFrame* render_frame,
                              const blink::WebPluginParams& params,
                              blink::WebPlugin** plugin) override {
      *plugin =
          render_frame->CreatePlugin(GetPluginInfo().ToWebPluginInfo(), params);
      return *plugin;
    }

    bool IsOriginIsolatedPepperPlugin(const base::FilePath& ignored) override {
      return false;
    }
  };

  PP_Module pp_module_;
  PP_Instance pp_instance_;
  PP_Resource graphics2d_;
  static PepperWebPluginImplBrowserTest* current_test_;
  static const PPB_Core* ppb_core_;
  static const PPB_Graphics2D* ppb_graphics2d_;
  static const PPB_ImageData* ppb_image_data_;
  static const PPB_Instance* ppb_instance_;
};
PepperWebPluginImplBrowserTest* PepperWebPluginImplBrowserTest::current_test_;
const PPB_Core* PepperWebPluginImplBrowserTest::ppb_core_;
const PPB_Graphics2D* PepperWebPluginImplBrowserTest::ppb_graphics2d_;
const PPB_ImageData* PepperWebPluginImplBrowserTest::ppb_image_data_;
const PPB_Instance* PepperWebPluginImplBrowserTest::ppb_instance_;

// This test simulates the behavior of a plugin that emits new frames during
// destruction. The throttler shouldn't engage and create a placeholder for
// a to-be destroyed plugin in such case. See crbug.com/483068
TEST_F(PepperWebPluginImplBrowserTest, NotEngageThrottleDuringDestroy) {
  LoadHTML("<!DOCTYPE html><object type='test/always-throttle'></object>");
  EXPECT_NE(0, pp_instance_);
  LoadHTML("");
  EXPECT_EQ(0, pp_instance_);
}

}  // unnamed namespace

}  // namespace content
