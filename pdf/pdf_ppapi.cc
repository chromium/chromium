// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ppapi.h"

#include <memory>

#include "pdf/out_of_process_instance.h"
#include "ppapi/c/ppp.h"
#include "ppapi/cpp/private/internal_module.h"
#include "ppapi/cpp/private/pdf.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

namespace {

bool g_sdk_initialized_via_pepper = false;

class PDFModule : public pp::Module {
 public:
  PDFModule();
  ~PDFModule() override;

  // pp::Module implementation.
  bool Init() override;
  pp::Instance* CreateInstance(PP_Instance instance) override;
};

PDFModule::PDFModule() = default;

PDFModule::~PDFModule() {
  if (g_sdk_initialized_via_pepper) {
    ShutdownSDK();
    g_sdk_initialized_via_pepper = false;
  }
}

bool PDFModule::Init() {
  return true;
}

pp::Instance* PDFModule::CreateInstance(PP_Instance instance) {
  if (!g_sdk_initialized_via_pepper) {
    v8::StartupData snapshot;
    pp::PDF::GetV8ExternalSnapshotData(pp::InstanceHandle(instance),
                                       &snapshot.data, &snapshot.raw_size);
    if (snapshot.data) {
      v8::V8::SetSnapshotDataBlob(&snapshot);
    }

    InitializeSDK(/*enable_v8=*/true);
    g_sdk_initialized_via_pepper = true;
  }

  return new OutOfProcessInstance(instance);
}

}  // namespace

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  auto module = std::make_unique<PDFModule>();
  if (!module->InternalInit(module_id, get_browser_interface))
    return PP_ERROR_FAILED;

  pp::InternalSetModuleSingleton(module.release());
  return PP_OK;
}

void PPP_ShutdownModule() {
  delete pp::Module::Get();
  pp::InternalSetModuleSingleton(nullptr);
}

const void* PPP_GetInterface(const char* interface_name) {
  auto* module = pp::Module::Get();
  return module ? module->GetPluginInterface(interface_name) : nullptr;
}

bool IsSDKInitializedViaPepper() {
  return g_sdk_initialized_via_pepper;
}

}  // namespace chrome_pdf
