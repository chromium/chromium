/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_url_util_dev.idl modified Wed Aug 28 19:09:17 2013. */

#ifndef PPAPI_C_DEV_PPB_URL_UTIL_DEV_H_
#define PPAPI_C_DEV_PPB_URL_UTIL_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_URLUTIL_DEV_INTERFACE_0_6 "PPB_URLUtil(Dev);0.6"
#define PPB_URLUTIL_DEV_INTERFACE_0_7 "PPB_URLUtil(Dev);0.7"
#define PPB_URLUTIL_DEV_INTERFACE PPB_URLUTIL_DEV_INTERFACE_0_7

/**
 * @file
 * This file defines the <code>PPB_URLUtil_Dev</code> interface.
 */


/**
 * @addtogroup Structs
 * @{
 */
/*
 * A component specifies the range of the part of the URL. The begin specifies
 * the index into the string of the first character of that component. The len
 * specifies the length of that component.
 *
 * This range does not include any special delimiter for that component, so
 * the scheme doesn't include the trailing colon, the username and password
 * don't include the @ and :, the port doesn't include the colon, the query
 * doesn't include the ?, and the ref doesn't include the #.
 *
 * The exception is that the path *does* include the first /, since that's an
 * integral part of the path.
 *
 * If the component is not present at all, begin will be 0 and len will be -1.
 * If the component is present but empty, the length will be 0 instead. Example:
 *   http://foo/search    -> query = (0, -1)
 *   http://foo/search?   -> query = (18, 0)
 */
struct PP_URLComponent_Dev {
  int32_t begin;
  int32_t len;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_URLComponent_Dev, 8);

struct PP_URLComponents_Dev {
  struct PP_URLComponent_Dev scheme;
  struct PP_URLComponent_Dev username;
  struct PP_URLComponent_Dev password;
  struct PP_URLComponent_Dev host;
  struct PP_URLComponent_Dev port;
  struct PP_URLComponent_Dev path;
  struct PP_URLComponent_Dev query;
  struct PP_URLComponent_Dev ref;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_URLComponents_Dev, 64);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/*
 * URL encoding: URLs are supplied to this interface as NULL-terminated 8-bit
 * strings. You can pass non-ASCII characters which will be interpreted as
 * UTF-8. Canonicalized URL strings returned by these functions will be ASCII
 * except for the reference fragment (stuff after the '#') which will be
 * encoded as UTF-8.
 */
struct PPB_URLUtil_Dev_0_7 {
  /*
   * Canonicalizes the given URL string according to the rules of the host
   * browser. If the URL is invalid or the var is not a string, this will
   * return a Null var and the components structure will be unchanged.
   *
   * The components pointer, if non-NULL and the canonicalized URL is valid,
   * will identify the components of the resulting URL. Components may be NULL
   * to specify that no component information is necessary.
   */
  struct PP_Var (*Canonicalize)(struct PP_Var url,
                                struct PP_URLComponents_Dev* components);
  /*
   *  Resolves the given URL relative to the given base URL. The resulting URL
   *  is returned as a string. If the resolution is invalid or either of the
   *  inputs are not strings, a Null var will be returned. The resulting URL
   *  will also be canonicalized according to the rules of the browser.
   *
   *  Note that the "relative" URL may in fact be absolute, in which case it
   *  will be returned. This function is identical to resolving the full URL
   *  for an <a href="..."> on a web page. Attempting to resolve a relative URL
   *  on a base URL that doesn't support this (e.g. "data") will fail and will
   *  return a Null var, unless the relative URL is itself absolute.
   *
   *  The components pointer, if non-NULL and the canonicalized URL is valid,
   *  will identify the components of the resulting URL. Components may be NULL
   *  to specify that no component information is necessary.
   */
  struct PP_Var (*ResolveRelativeToURL)(
      struct PP_Var base_url,
      struct PP_Var relative_string,
      struct PP_URLComponents_Dev* components);
  /*
   *  Identical to ResolveRelativeToURL except that the base URL is the base
   *  URL of the document containing the given plugin instance.
   *
   *  Danger: This will be identical to resolving a relative URL on the page,
   *  and might be overridden by the page to something different than its actual
   *  URL via the <base> tag. Therefore, resolving a relative URL of "" won't
   *  necessarily give you the URL of the page!
   */
  struct PP_Var (*ResolveRelativeToDocument)(
      PP_Instance instance,
      struct PP_Var relative_string,
      struct PP_URLComponents_Dev* components);
  /*
   * Checks whether the given two URLs are in the same security origin. Returns
   * FALSE if either of the URLs are invalid.
   */
  PP_Bool (*IsSameSecurityOrigin)(struct PP_Var url_a, struct PP_Var url_b);
  /*
   * Checks whether the document hosting the given plugin instance can access
   * the given URL according to the same origin policy of the browser. Returns
   * PP_FALSE if the instance or the URL is invalid.
   */
  PP_Bool (*DocumentCanRequest)(PP_Instance instance, struct PP_Var url);
  /*
   * Checks whether the document containing the |active| plugin instance can
   * access the document containing the |target| plugin instance according to
   * the security policy of the browser. This includes the same origin policy
   * and any cross-origin capabilities enabled by the document. If either of
   * the plugin instances are invalid, returns PP_FALSE.
   */
  PP_Bool (*DocumentCanAccessDocument)(PP_Instance active, PP_Instance target);
  /*
   * Returns the URL for the document. This is a safe way to retrieve
   * window.location.href.
   * The components pointer, if non-NULL and the canonicalized URL is valid,
   * will identify the components of the resulting URL. Components may be NULL
   * to specify that no component information is necessary.
   */
  struct PP_Var (*GetDocumentURL)(PP_Instance instance,
                                  struct PP_URLComponents_Dev* components);
  /*
   * Returns the Source URL for the plugin. This returns the URL that would be
   * streamed to the plugin if it were a NPAPI plugin. This is usually the src
   * attribute on the <embed> element, but the rules are obscure and different
   * based on whether the plugin is loaded from an <embed> element or an
   * <object> element.
   * The components pointer, if non-NULL and the canonicalized URL is valid,
   * will identify the components of the resulting URL. Components may be NULL
   * to specify that no component information is necessary.
   */
  struct PP_Var (*GetPluginInstanceURL)(
      PP_Instance instance,
      struct PP_URLComponents_Dev* components);
  /*
   * Returns the Referrer URL of the HTTP request that loaded the plugin. This
   * is the value of the 'Referer' header of the request. An undefined value
   * means the 'Referer' header was absent.
   * The components pointer, if non-NULL and the canonicalized URL is valid,
   * will identify the components of the resulting URL. Components may be NULL
   * to specify that no component information is necessary.
   */
  struct PP_Var (*GetPluginReferrerURL)(
      PP_Instance instance,
      struct PP_URLComponents_Dev* components);
};

typedef struct PPB_URLUtil_Dev_0_7 PPB_URLUtil_Dev;

struct PPB_URLUtil_Dev_0_6 {
  struct PP_Var (*Canonicalize)(struct PP_Var url,
                                struct PP_URLComponents_Dev* components);
  struct PP_Var (*ResolveRelativeToURL)(
      struct PP_Var base_url,
      struct PP_Var relative_string,
      struct PP_URLComponents_Dev* components);
  struct PP_Var (*ResolveRelativeToDocument)(
      PP_Instance instance,
      struct PP_Var relative_string,
      struct PP_URLComponents_Dev* components);
  PP_Bool (*IsSameSecurityOrigin)(struct PP_Var url_a, struct PP_Var url_b);
  PP_Bool (*DocumentCanRequest)(PP_Instance instance, struct PP_Var url);
  PP_Bool (*DocumentCanAccessDocument)(PP_Instance active, PP_Instance target);
  struct PP_Var (*GetDocumentURL)(PP_Instance instance,
                                  struct PP_URLComponents_Dev* components);
  struct PP_Var (*GetPluginInstanceURL)(
      PP_Instance instance,
      struct PP_URLComponents_Dev* components);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_URL_UTIL_DEV_H_ */

