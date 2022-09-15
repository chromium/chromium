// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/mouse_lock.h"

#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPMouseLockInterface[] = PPP_MOUSELOCK_INTERFACE;

void MouseLockLost(PP_Instance instance) {
  void* object =
      Instance::GetPerInstanceObject(instance, kPPPMouseLockInterface);
  if (!object)
    return;
  static_cast<MouseLock*>(object)->MouseLockLost();
}

const PPP_MouseLock ppp_mouse_lock = {
  &MouseLockLost
};

template <> const char* interface_name<PPB_MouseLock_1_0>() {
  return PPB_MOUSELOCK_INTERFACE_1_0;
}

}  // namespace

MouseLock::MouseLock(Instance* instance)
    : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPMouseLockInterface, &ppp_mouse_lock);
  instance->AddPerInstanceObject(kPPPMouseLockInterface, this);
}

MouseLock::~MouseLock() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPMouseLockInterface, this);
}

int32_t MouseLock::LockMouse(const CompletionCallback& cc) {
  if (!has_interface<PPB_MouseLock_1_0>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_MouseLock_1_0>()->LockMouse(
      associated_instance_.pp_instance(), cc.pp_completion_callback());
}

void MouseLock::UnlockMouse() {
  if (has_interface<PPB_MouseLock_1_0>()) {
    get_interface<PPB_MouseLock_1_0>()->UnlockMouse(
        associated_instance_.pp_instance());
  }
}

}  // namespace pp
