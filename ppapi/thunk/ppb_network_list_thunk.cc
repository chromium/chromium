// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_network_list.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_network_list_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsNetworkList(PP_Resource resource) {
  VLOG(4) << "PPB_NetworkList::IsNetworkList()";
  EnterResource<PPB_NetworkList_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

uint32_t GetCount(PP_Resource resource) {
  VLOG(4) << "PPB_NetworkList::GetCount()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCount();
}

struct PP_Var GetName(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList::GetName()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName(index);
}

PP_NetworkList_Type GetType(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList::GetType()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return PP_NETWORKLIST_TYPE_UNKNOWN;
  return enter.object()->GetType(index);
}

PP_NetworkList_State GetState(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList::GetState()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return PP_NETWORKLIST_STATE_DOWN;
  return enter.object()->GetState(index);
}

int32_t GetIpAddresses(PP_Resource resource,
                       uint32_t index,
                       struct PP_ArrayOutput output) {
  VLOG(4) << "PPB_NetworkList::GetIpAddresses()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetIpAddresses(index, output);
}

struct PP_Var GetDisplayName(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList::GetDisplayName()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetDisplayName(index);
}

uint32_t GetMTU(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList::GetMTU()";
  EnterResource<PPB_NetworkList_API> enter(resource, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetMTU(index);
}

const PPB_NetworkList_1_0 g_ppb_networklist_thunk_1_0 = {
    &IsNetworkList, &GetCount,       &GetName,        &GetType,
    &GetState,      &GetIpAddresses, &GetDisplayName, &GetMTU};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_NetworkList_1_0* GetPPB_NetworkList_1_0_Thunk() {
  return &g_ppb_networklist_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
