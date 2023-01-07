// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ../test_thunk/simple.idl modified Fri Nov 16 11:26:06 2012.

#include <stdint.h>

#include "ppapi/c/../test_thunk/simple.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/simple_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_Simple::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateSimple(instance);
}

PP_Bool IsSimple(PP_Resource resource) {
  VLOG(4) << "PPB_Simple::IsSimple()";
  EnterResource<PPB_Simple_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

void PostMessage(PP_Instance instance, PP_Var message) {
  VLOG(4) << "PPB_Simple::PostMessage()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->PostMessage(instance, message);
}

uint32_t DoUint32Instance_0_5(PP_Instance instance) {
  VLOG(4) << "PPB_Simple::DoUint32Instance()";
  EnterInstance enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->DoUint32Instance0_5(instance);
}

uint32_t DoUint32Instance(PP_Instance instance, PP_Resource resource) {
  VLOG(4) << "PPB_Simple::DoUint32Instance()";
  EnterInstance enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->DoUint32Instance(instance, resource);
}

uint32_t DoUint32Resource(PP_Resource instance) {
  VLOG(4) << "PPB_Simple::DoUint32Resource()";
  EnterResource<PPB_Simple_API> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.object()->DoUint32Resource();
}

uint32_t DoUint32ResourceNoErrors(PP_Resource instance) {
  VLOG(4) << "PPB_Simple::DoUint32ResourceNoErrors()";
  EnterResource<PPB_Simple_API> enter(instance, false);
  if (enter.failed())
    return 0;
  return enter.object()->DoUint32ResourceNoErrors();
}

int32_t OnFailure12(PP_Instance instance) {
  VLOG(4) << "PPB_Simple::OnFailure12()";
  EnterInstance enter(instance);
  if (enter.failed())
    return 12;
  return enter.functions()->OnFailure12(instance);
}

const PPB_Simple_0_5 g_ppb_simple_thunk_0_5 = {
  &Create,
  &IsSimple,
  &PostMessage,
  &DoUint32Instance_0_5,
  &DoUint32Resource,
  &DoUint32ResourceNoErrors
};

const PPB_Simple_1_0 g_ppb_simple_thunk_1_0 = {
  &Create,
  &IsSimple,
  &DoUint32Instance_0_5,
  &DoUint32Resource,
  &DoUint32ResourceNoErrors,
  &OnFailure12
};

const PPB_Simple_1_5 g_ppb_simple_thunk_1_5 = {
  &Create,
  &IsSimple,
  &DoUint32Instance,
  &DoUint32Resource,
  &DoUint32ResourceNoErrors,
  &OnFailure12
};

}  // namespace

const PPB_Simple_0_5* GetPPB_Simple_0_5_Thunk() {
  return &g_ppb_simple_thunk_0_5;
}

const PPB_Simple_1_0* GetPPB_Simple_1_0_Thunk() {
  return &g_ppb_simple_thunk_1_0;
}

const PPB_Simple_1_5* GetPPB_Simple_1_5_Thunk() {
  return &g_ppb_simple_thunk_1_5;
}

}  // namespace thunk
}  // namespace ppapi
