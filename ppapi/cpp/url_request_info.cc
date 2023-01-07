// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_request_info.h"

#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_URLRequestInfo_1_0>() {
  return PPB_URLREQUESTINFO_INTERFACE_1_0;
}

}  // namespace

URLRequestInfo::URLRequestInfo(const InstanceHandle& instance) {
  if (!has_interface<PPB_URLRequestInfo_1_0>())
    return;
  PassRefFromConstructor(
      get_interface<PPB_URLRequestInfo_1_0>()->Create(instance.pp_instance()));
}

URLRequestInfo::URLRequestInfo(const URLRequestInfo& other)
    : Resource(other) {
}

bool URLRequestInfo::SetProperty(PP_URLRequestProperty property,
                                 const Var& value) {
  if (!has_interface<PPB_URLRequestInfo_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_URLRequestInfo_1_0>()->SetProperty(
      pp_resource(), property, value.pp_var()));
}

bool URLRequestInfo::AppendDataToBody(const void* data, uint32_t len) {
  if (!has_interface<PPB_URLRequestInfo_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_URLRequestInfo_1_0>()->AppendDataToBody(
      pp_resource(), data, len));
}

bool URLRequestInfo::AppendFileToBody(const FileRef& file_ref,
                                      PP_Time expected_last_modified_time) {
  if (!has_interface<PPB_URLRequestInfo_1_0>())
    return false;
  return PP_ToBool(
      get_interface<PPB_URLRequestInfo_1_0>()->AppendFileToBody(
          pp_resource(),
          file_ref.pp_resource(),
          0,
          -1,
          expected_last_modified_time));
}

bool URLRequestInfo::AppendFileRangeToBody(
    const FileRef& file_ref,
    int64_t start_offset,
    int64_t length,
    PP_Time expected_last_modified_time) {
  if (!has_interface<PPB_URLRequestInfo_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_URLRequestInfo_1_0>()->AppendFileToBody(
      pp_resource(),
      file_ref.pp_resource(),
      start_offset,
      length,
      expected_last_modified_time));
}

}  // namespace pp
