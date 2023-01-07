// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/url_request_info_resource.h"

#include "base/strings/string_number_conversions.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

namespace ppapi {
namespace proxy {

URLRequestInfoResource::URLRequestInfoResource(Connection connection,
                                               PP_Instance instance,
                                               const URLRequestInfoData& data)
    : PluginResource(connection, instance),
      data_(data) {
}

URLRequestInfoResource::~URLRequestInfoResource() {
}

thunk::PPB_URLRequestInfo_API*
URLRequestInfoResource::AsPPB_URLRequestInfo_API() {
  return this;
}

PP_Bool URLRequestInfoResource::SetProperty(PP_URLRequestProperty property,
                                            PP_Var var) {
  // IMPORTANT: Do not do security validation of parameters at this level
  // without also adding them to PPB_URLRequestInfo_Impl::ValidateData. This
  // code is used both in the plugin (which we don't trust) and in the renderer
  // (which we trust more). When running out-of-process, the plugin calls this
  // function to configure the URLRequestInfoData, which is then sent to
  // the renderer and *not* run through SetProperty again.
  //
  // This means that anything in the PPB_URLRequestInfo_Data needs to be
  // validated at the time the URL is requested (which is what ValidateData
  // does). If your feature requires security checks, it should be in the
  // implementation in the renderer when the WebKit request is actually
  // constructed.
  //
  // It is legal to do some validation here if you want to report failure to
  // the plugin as a convenience, as long as you also do it in the renderer
  // later.
  PP_Bool result = PP_FALSE;
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      result = PP_FromBool(SetUndefinedProperty(property));
      break;
    case PP_VARTYPE_BOOL:
      result = PP_FromBool(
          SetBooleanProperty(property, PP_ToBool(var.value.as_bool)));
      break;
    case PP_VARTYPE_INT32:
      result = PP_FromBool(
          SetIntegerProperty(property, var.value.as_int));
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (string)
        result = PP_FromBool(SetStringProperty(property, string->value()));
      break;
    }
    default:
      break;
  }
  if (!result) {
    std::string error_msg("PPB_URLRequestInfo.SetProperty: Attempted to set a "
                          "value for PP_URLRequestProperty ");
    error_msg += base::NumberToString(property);
    error_msg += ", but either this property type is invalid or its parameter "
                 "was inappropriate (e.g., the wrong type of PP_Var).";
    Log(PP_LOGLEVEL_ERROR, error_msg);
  }
  return result;
}

PP_Bool URLRequestInfoResource::AppendDataToBody(const void* data,
                                                 uint32_t len) {
  if (len > 0) {
    data_.body.push_back(URLRequestInfoData::BodyItem(
        std::string(static_cast<const char*>(data), len)));
  }
  return PP_TRUE;
}

PP_Bool URLRequestInfoResource::AppendFileToBody(
    PP_Resource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    PP_Time expected_last_modified_time) {
  thunk::EnterResourceNoLock<thunk::PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_FALSE;

  // Ignore a call to append nothing.
  if (number_of_bytes == 0)
    return PP_TRUE;

  // Check for bad values.  (-1 means read until end of file.)
  if (start_offset < 0 || number_of_bytes < -1)
    return PP_FALSE;

  data_.body.push_back(URLRequestInfoData::BodyItem(
      enter.resource(),
      start_offset,
      number_of_bytes,
      expected_last_modified_time));
  return PP_TRUE;
}

const URLRequestInfoData& URLRequestInfoResource::GetData() const {
  return data_;
}

bool URLRequestInfoResource::SetUndefinedProperty(
    PP_URLRequestProperty property) {
  // IMPORTANT: Do not do security validation of parameters at this level
  // without also adding them to PPB_URLRequestInfo_Impl::ValidateData. See
  // SetProperty() above for why.
  switch (property) {
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      data_.has_custom_referrer_url = false;
      data_.custom_referrer_url = std::string();
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      data_.has_custom_content_transfer_encoding = false;
      data_.custom_content_transfer_encoding = std::string();
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT:
      data_.has_custom_user_agent = false;
      data_.custom_user_agent = std::string();
      return true;
    default:
      return false;
  }
}

bool URLRequestInfoResource::SetBooleanProperty(
    PP_URLRequestProperty property,
    bool value) {
  // IMPORTANT: Do not do security validation of parameters at this level
  // without also adding them to PPB_URLRequestInfo_Impl::ValidateData. See
  // SetProperty() above for why.
  switch (property) {
    case PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS:
      data_.follow_redirects = value;
      return true;
    case PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS:
      data_.record_download_progress = value;
      return true;
    case PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS:
      data_.record_upload_progress = value;
      return true;
    case PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS:
      data_.allow_cross_origin_requests = value;
      return true;
    case PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS:
      data_.allow_credentials = value;
      return true;
    default:
      return false;
  }
}

bool URLRequestInfoResource::SetIntegerProperty(
    PP_URLRequestProperty property,
    int32_t value) {
  // IMPORTANT: Do not do security validation of parameters at this level
  // without also adding them to PPB_URLRequestInfo_Impl::ValidateData. See
  // SetProperty() above for why.
  switch (property) {
    case PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD:
      data_.prefetch_buffer_upper_threshold = value;
      return true;
    case PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD:
      data_.prefetch_buffer_lower_threshold = value;
      return true;
    default:
      return false;
  }
}

bool URLRequestInfoResource::SetStringProperty(
    PP_URLRequestProperty property,
    const std::string& value) {
  // IMPORTANT: Do not do security validation of parameters at this level
  // without also adding them to PPB_URLRequestInfo_Impl::ValidateData. See
  // SetProperty() above for why.
  switch (property) {
    case PP_URLREQUESTPROPERTY_URL:
      data_.url = value;  // NOTE: This may be a relative URL.
      return true;
    case PP_URLREQUESTPROPERTY_METHOD:
      data_.method = value;
      return true;
    case PP_URLREQUESTPROPERTY_HEADERS:
      data_.headers = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      data_.has_custom_referrer_url = true;
      data_.custom_referrer_url = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      data_.has_custom_content_transfer_encoding = true;
      data_.custom_content_transfer_encoding = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT:
      data_.has_custom_user_agent = true;
      data_.custom_user_agent = value;
      return true;
    default:
      return false;
  }
}

}  // namespace proxy
}  // namespace ppapi
