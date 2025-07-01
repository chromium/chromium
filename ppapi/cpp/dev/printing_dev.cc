// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/printing_dev.h"

#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPPrintingInterface[] = PPP_PRINTING_DEV_INTERFACE;

template <> const char* interface_name<PPB_Printing_Dev_0_7>() {
  return PPB_PRINTING_DEV_INTERFACE_0_7;
}

uint32_t QuerySupportedFormats(PP_Instance instance) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->QuerySupportedPrintOutputFormats();
}

int32_t Begin(PP_Instance instance,
              const struct PP_PrintSettings_Dev* print_settings) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->PrintBegin(*print_settings);
}

PP_Resource PrintPages(PP_Instance instance,
                       const struct PP_PrintPageNumberRange_Dev* page_ranges,
                       uint32_t page_range_count) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->PrintPages(
      page_ranges, page_range_count).detach();
}

void End(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (object)
    static_cast<Printing_Dev*>(object)->PrintEnd();
}

PP_Bool IsScalingDisabled(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return PP_FALSE;
  bool return_value =
      static_cast<Printing_Dev*>(object)->IsPrintScalingDisabled();
  return PP_FromBool(return_value);
}

const PPP_Printing_Dev ppp_printing = {
  &QuerySupportedFormats,
  &Begin,
  &PrintPages,
  &End,
  &IsScalingDisabled
};

}  // namespace

Printing_Dev::Printing_Dev(Instance* instance)
    : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPPrintingInterface, &ppp_printing);
  instance->AddPerInstanceObject(
      kPPPPrintingInterface, this);
  if (has_interface<PPB_Printing_Dev_0_7>()) {
    PassRefFromConstructor(get_interface<PPB_Printing_Dev_0_7>()->Create(
        associated_instance_.pp_instance()));
  }
}

Printing_Dev::~Printing_Dev() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPPrintingInterface, this);
}

// static
bool Printing_Dev::IsAvailable() {
  return has_interface<PPB_Printing_Dev_0_7>();
}

int32_t Printing_Dev::GetDefaultPrintSettings(
    const CompletionCallbackWithOutput<PP_PrintSettings_Dev>& callback) const {
  if (has_interface<PPB_Printing_Dev_0_7>()) {
    return get_interface<PPB_Printing_Dev_0_7>()->GetDefaultPrintSettings(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
