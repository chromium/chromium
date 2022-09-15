// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppp_message_handler.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event_interface_name.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/message_handler.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Console_1_0>() {
  return PPB_CONSOLE_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_Instance_1_0>() {
  return PPB_INSTANCE_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_Messaging_1_0>() {
  return PPB_MESSAGING_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_Messaging_1_2>() {
  return PPB_MESSAGING_INTERFACE_1_2;
}

// PPP_MessageHandler implementation -------------------------------------------
void HandleMessage(PP_Instance pp_instance,
                   void* user_data,
                   const PP_Var* var) {
  MessageHandler* message_handler = static_cast<MessageHandler*>(user_data);
  message_handler->HandleMessage(InstanceHandle(pp_instance), Var(*var));
}

void HandleBlockingMessage(PP_Instance pp_instance,
                           void* user_data,
                           const PP_Var* var,
                           PP_Var* result) {
  MessageHandler* message_handler = static_cast<MessageHandler*>(user_data);
  pp::Var result_var =
      message_handler->HandleBlockingMessage(InstanceHandle(pp_instance),
                                             Var(*var));
  *result = result_var.Detach();
}

void Destroy(PP_Instance pp_instance, void* user_data) {
  MessageHandler* message_handler = static_cast<MessageHandler*>(user_data);
  message_handler->WasUnregistered(InstanceHandle(pp_instance));
}

static PPP_MessageHandler_0_2 message_handler_if = {
  &HandleMessage, &HandleBlockingMessage, &Destroy
};

}  // namespace

Instance::Instance(PP_Instance instance) : pp_instance_(instance) {
}

Instance::~Instance() {
}

bool Instance::Init(uint32_t /*argc*/, const char* /*argn*/[],
                    const char* /*argv*/[]) {
  return true;
}

void Instance::DidChangeView(const View& view) {
  // Call the deprecated version for source backwards-compat.
  DidChangeView(view.GetRect(), view.GetClipRect());
}

void Instance::DidChangeView(const pp::Rect& /*position*/,
                             const pp::Rect& /*clip*/) {
}

void Instance::DidChangeFocus(bool /*has_focus*/) {
}


bool Instance::HandleDocumentLoad(const URLLoader& /*url_loader*/) {
  return false;
}

bool Instance::HandleInputEvent(const InputEvent& /*event*/) {
  return false;
}

void Instance::HandleMessage(const Var& /*message*/) {
  return;
}

bool Instance::BindGraphics(const Graphics2D& graphics) {
  if (!has_interface<PPB_Instance_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_Instance_1_0>()->BindGraphics(
      pp_instance(), graphics.pp_resource()));
}

bool Instance::BindGraphics(const Graphics3D& graphics) {
  if (!has_interface<PPB_Instance_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_Instance_1_0>()->BindGraphics(
      pp_instance(), graphics.pp_resource()));
}

bool Instance::IsFullFrame() {
  if (!has_interface<PPB_Instance_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_Instance_1_0>()->IsFullFrame(
      pp_instance()));
}

int32_t Instance::RequestInputEvents(uint32_t event_classes) {
  if (!has_interface<PPB_InputEvent_1_0>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_InputEvent_1_0>()->RequestInputEvents(pp_instance(),
                                                                 event_classes);
}

int32_t Instance::RequestFilteringInputEvents(uint32_t event_classes) {
  if (!has_interface<PPB_InputEvent_1_0>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_InputEvent_1_0>()->RequestFilteringInputEvents(
      pp_instance(), event_classes);
}

void Instance::ClearInputEventRequest(uint32_t event_classes) {
  if (!has_interface<PPB_InputEvent_1_0>())
    return;
  get_interface<PPB_InputEvent_1_0>()->ClearInputEventRequest(pp_instance(),
                                                          event_classes);
}

void Instance::PostMessage(const Var& message) {
  if (has_interface<PPB_Messaging_1_2>()) {
    get_interface<PPB_Messaging_1_2>()->PostMessage(pp_instance(),
                                                    message.pp_var());
  } else if (has_interface<PPB_Messaging_1_0>()) {
    get_interface<PPB_Messaging_1_0>()->PostMessage(pp_instance(),
                                                    message.pp_var());
  }
}

int32_t Instance::RegisterMessageHandler(MessageHandler* message_handler,
                                         const MessageLoop& message_loop) {
  if (!has_interface<PPB_Messaging_1_2>())
    return PP_ERROR_NOTSUPPORTED;
  return get_interface<PPB_Messaging_1_2>()->RegisterMessageHandler(
      pp_instance(),
      message_handler,
      &message_handler_if,
      message_loop.pp_resource());
}

void Instance::UnregisterMessageHandler() {
  if (!has_interface<PPB_Messaging_1_2>())
    return;
  get_interface<PPB_Messaging_1_2>()->UnregisterMessageHandler(pp_instance());
}

void Instance::LogToConsole(PP_LogLevel level, const Var& value) {
  if (!has_interface<PPB_Console_1_0>())
    return;
  get_interface<PPB_Console_1_0>()->Log(
      pp_instance(), level, value.pp_var());
}

void Instance::LogToConsoleWithSource(PP_LogLevel level,
                                      const Var& source,
                                      const Var& value) {
  if (!has_interface<PPB_Console_1_0>())
    return;
  get_interface<PPB_Console_1_0>()->LogWithSource(
      pp_instance(), level, source.pp_var(), value.pp_var());
}

void Instance::AddPerInstanceObject(const std::string& interface_name,
                                    void* object) {
  // Ensure we're not trying to register more than one object per interface
  // type. Otherwise, we'll get confused in GetPerInstanceObject.
  PP_DCHECK(interface_name_to_objects_.find(interface_name) ==
            interface_name_to_objects_.end());
  interface_name_to_objects_[interface_name] = object;
}

void Instance::RemovePerInstanceObject(const std::string& interface_name,
                                       void* object) {
  InterfaceNameToObjectMap::iterator found = interface_name_to_objects_.find(
      interface_name);
  if (found == interface_name_to_objects_.end()) {
    // Attempting to unregister an object that doesn't exist or was already
    // unregistered.
    PP_DCHECK(false);
    return;
  }

  // Validate that we're removing the object we thing we are.
  PP_DCHECK(found->second == object);
  (void)object;  // Prevent warning in release mode.

  interface_name_to_objects_.erase(found);
}

// static
void Instance::RemovePerInstanceObject(const InstanceHandle& instance,
                                       const std::string& interface_name,
                                       void* object) {
  // TODO(brettw) assert we're on the main thread.
  Instance* that = Module::Get()->InstanceForPPInstance(instance.pp_instance());
  if (!that)
    return;
  that->RemovePerInstanceObject(interface_name, object);
}

// static
void* Instance::GetPerInstanceObject(PP_Instance instance,
                                     const std::string& interface_name) {
  Instance* that = Module::Get()->InstanceForPPInstance(instance);
  if (!that)
    return NULL;
  InterfaceNameToObjectMap::iterator found =
      that->interface_name_to_objects_.find(interface_name);
  if (found == that->interface_name_to_objects_.end())
    return NULL;
  return found->second;
}

}  // namespace pp
