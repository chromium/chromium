// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/ppapi_unittest.h"

#include <stdint.h>

#include "content/public/common/content_plugin_info.h"
#include "content/public/renderer/ppapi_gfx_conversion.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace content {

namespace {

PpapiUnittest* current_unittest = nullptr;

const void* MockGetInterface(const char* interface_name) {
  return current_unittest->GetMockInterface(interface_name);
}

int MockInitializeModule(PP_Module, PPB_GetInterface) {
  return PP_OK;
}

// PPP_Instance implementation ------------------------------------------------

PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                           uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  return PP_TRUE;
}

void Instance_DidDestroy(PP_Instance instance) {
}

void Instance_DidChangeView(PP_Instance pp_instance, PP_Resource view) {
}

void Instance_DidChangeFocus(PP_Instance pp_instance, PP_Bool has_focus) {
}

PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                    PP_Resource pp_url_loader) {
  return PP_FALSE;
}

static PPP_Instance mock_instance_interface = {
  &Instance_DidCreate,
  &Instance_DidDestroy,
  &Instance_DidChangeView,
  &Instance_DidChangeFocus,
  &Instance_HandleDocumentLoad
};

}  // namespace

// PpapiUnittest --------------------------------------------------------------

PpapiUnittest::PpapiUnittest() {
  DCHECK(!current_unittest);
  current_unittest = this;
}

PpapiUnittest::~PpapiUnittest() {
  DCHECK(current_unittest == this);
  current_unittest = nullptr;
}

void PpapiUnittest::SetUp() {
  // Initialize the mock module.
  ppapi::PpapiPermissions perms;
  module_ = new PluginModule("Mock plugin", "1.0", base::FilePath(),
                             perms);
  ppapi::PpapiGlobals::Get()->ResetMainThreadMessageLoopForTesting();
  ContentPluginInfo::EntryPoints entry_points;
  entry_points.get_interface = &MockGetInterface;
  entry_points.initialize_module = &MockInitializeModule;
  ASSERT_TRUE(module_->InitAsInternalPlugin(entry_points));

  // Initialize renderer ppapi host.
  CHECK(RendererPpapiHostImpl::CreateOnModuleForInProcess(module(), perms));
  CHECK(module_->renderer_ppapi_host());

  // Initialize the mock instance.
  instance_ = PepperPluginInstanceImpl::Create(
      nullptr, module(), nullptr, GURL(),
      UnitTestTestSuite::MainThreadIsolateForUnitTestSuite());
}

void PpapiUnittest::TearDown() {
  instance_ = nullptr;
  module_ = nullptr;
  PluginModule::ResetHostGlobalsForTest();
}

const void* PpapiUnittest::GetMockInterface(const char* interface_name) const {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE_1_0) == 0)
    return &mock_instance_interface;
  return nullptr;
}

void PpapiUnittest::ShutdownModule() {
  DCHECK(instance_->HasOneRef());
  instance_ = nullptr;
  DCHECK(module_->HasOneRef());
  module_ = nullptr;
}

void PpapiUnittest::SetViewSize(int width, int height) const {
  instance_->view_data_.rect = PP_FromGfxRect(gfx::Rect(0, 0, width, height));
  instance_->view_data_.clip_rect = instance_->view_data_.rect;
}

}  // namespace content
