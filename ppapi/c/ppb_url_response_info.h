/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_url_response_info.idl modified Thu May 17 11:28:03 2018. */

#ifndef PPAPI_C_PPB_URL_RESPONSE_INFO_H_
#define PPAPI_C_PPB_URL_RESPONSE_INFO_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_URLRESPONSEINFO_INTERFACE_1_0 "PPB_URLResponseInfo;1.0"
#define PPB_URLRESPONSEINFO_INTERFACE PPB_URLRESPONSEINFO_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_URLResponseInfo</code> API for examining URL
 * responses.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains properties set on a URL response.
 */
typedef enum {
  /**
   * This corresponds to a string (PP_VARTYPE_STRING); an absolute URL formed by
   * resolving the relative request URL with the absolute document URL. Refer
   * to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1.2">
   * HTTP Request URI</a> and
   * <a href="http://www.w3.org/TR/html4/struct/links.html#h-12.4.1">
   * HTML Resolving Relative URIs</a> documentation for further information.
   */
  PP_URLRESPONSEPROPERTY_URL = 0,
  /**
   * This corresponds to a string (PP_VARTYPE_STRING); the absolute URL returned
   * in the response header's 'Location' field if this is a redirect response,
   * an empty string otherwise. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.3">
   * HTTP Status Codes - Redirection</a> documentation for further information.
   */
  PP_URLRESPONSEPROPERTY_REDIRECTURL = 1,
  /**
   * This corresponds to a string (PP_VARTYPE_STRING); the HTTP method to be
   * used in a new request if this is a redirect response, an empty string
   * otherwise. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.3">
   * HTTP Status Codes - Redirection</a> documentation for further information.
   */
  PP_URLRESPONSEPROPERTY_REDIRECTMETHOD = 2,
  /**
   * This corresponds to an int32 (PP_VARETYPE_INT32); the status code from the
   * response, e.g., 200 if the request was successful. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1.1">
   * HTTP Status Code and Reason Phrase</a> documentation for further
   * information.
   */
  PP_URLRESPONSEPROPERTY_STATUSCODE = 3,
  /**
   * This corresponds to a string (PP_VARTYPE_STRING); the status line
   * from the response. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1">
   * HTTP Response Status Line</a> documentation for further information.
   */
  PP_URLRESPONSEPROPERTY_STATUSLINE = 4,
  /**
   * This corresponds to a string(PP_VARTYPE_STRING), a \n-delimited list of
   * header field/value pairs of the form "field: value", returned by the
   * server. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14">
   * HTTP Header Field Definitions</a> documentation for further information.
   */
  PP_URLRESPONSEPROPERTY_HEADERS = 5
} PP_URLResponseProperty;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLResponseProperty, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The PPB_URLResponseInfo interface contains APIs for
 * examining URL responses. Refer to <code>PPB_URLLoader</code> for further
 * information.
 */
struct PPB_URLResponseInfo_1_0 {
  /**
   * IsURLResponseInfo() determines if a response is a
   * <code>URLResponseInfo</code>.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>URLResponseInfo</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>URLResponseInfo</code>.
   */
  PP_Bool (*IsURLResponseInfo)(PP_Resource resource);
  /**
   * GetProperty() gets a response property.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   * @param[in] property A <code>PP_URLResponseProperty</code> identifying
   * the type of property in the response.
   *
   * @return A <code>PP_Var</code> containing the response property value if
   * successful, <code>PP_VARTYPE_VOID</code> if an input parameter is invalid.
   */
  struct PP_Var (*GetProperty)(PP_Resource response,
                               PP_URLResponseProperty property);
  /**
   * GetBodyAsFileRef() always returns 0, because
   * <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> is no longer supported.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   *
   * @return 0, because <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> is no
   * longer supported.
   */
  PP_Resource (*GetBodyAsFileRef)(PP_Resource response);
};

typedef struct PPB_URLResponseInfo_1_0 PPB_URLResponseInfo;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_RESPONSE_INFO_H_ */

