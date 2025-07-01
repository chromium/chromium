// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/uma_private_resource.h"

#include "base/functional/bind.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/var.h"

namespace {

std::string StringFromPPVar(const PP_Var& var) {
  scoped_refptr<ppapi::StringVar> name_stringvar =
      ppapi::StringVar::FromPPVar(var);
  if (!name_stringvar.get())
    return std::string();
  return name_stringvar->value();
}

}

namespace ppapi {
namespace proxy {

UMAPrivateResource::UMAPrivateResource(
    Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_UMA_Create());
}

UMAPrivateResource::~UMAPrivateResource() {
}

thunk::PPB_UMA_Singleton_API* UMAPrivateResource::AsPPB_UMA_Singleton_API() {
  return this;
}

void UMAPrivateResource::HistogramCustomTimes(
    PP_Instance instance,
    struct PP_Var name,
    int64_t sample,
    int64_t min,
    int64_t max,
    uint32_t bucket_count) {
  if (name.type != PP_VARTYPE_STRING)
    return;

  Post(RENDERER, PpapiHostMsg_UMA_HistogramCustomTimes(StringFromPPVar(name),
                                                       sample,
                                                       min,
                                                       max,
                                                       bucket_count));
}

void UMAPrivateResource::HistogramCustomCounts(
    PP_Instance instance,
    struct PP_Var name,
    int32_t sample,
    int32_t min,
    int32_t max,
    uint32_t bucket_count) {
  if (name.type != PP_VARTYPE_STRING)
    return;

  Post(RENDERER, PpapiHostMsg_UMA_HistogramCustomCounts(StringFromPPVar(name),
                                                        sample,
                                                        min,
                                                        max,
                                                        bucket_count));
}

void UMAPrivateResource::HistogramEnumeration(
    PP_Instance instance,
    struct PP_Var name,
    int32_t sample,
    int32_t boundary_value) {
  if (name.type != PP_VARTYPE_STRING)
    return;

  Post(RENDERER, PpapiHostMsg_UMA_HistogramEnumeration(StringFromPPVar(name),
                                                       sample,
                                                       boundary_value));
}

int32_t UMAPrivateResource::IsCrashReportingEnabled(
    PP_Instance instance,
    scoped_refptr<TrackedCallback> callback) {
  if (pending_callback_.get() != NULL)
    return PP_ERROR_INPROGRESS;
  pending_callback_ = callback;
  Call<PpapiPluginMsg_UMA_IsCrashReportingEnabledReply>(
      RENDERER, PpapiHostMsg_UMA_IsCrashReportingEnabled(),
      base::BindOnce(&UMAPrivateResource::OnPluginMsgIsCrashReportingEnabled,
                     this));
  return PP_OK_COMPLETIONPENDING;
}

void UMAPrivateResource::OnPluginMsgIsCrashReportingEnabled(
    const ResourceMessageReplyParams& params) {
  if (TrackedCallback::IsPending(pending_callback_))
    pending_callback_->Run(params.result());
  pending_callback_.reset();
}

}  // namespace proxy
}  // namespace ppapi

