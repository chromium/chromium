// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_url_util_shared.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "url/gurl.h"

namespace ppapi {

namespace {

void ConvertComponent(const url::Component& input,
                      PP_URLComponent_Dev* output) {
  output->begin = input.begin;
  output->len = input.len;
}

// Converts components from a GoogleUrl parsed to a PPAPI parsed structure.
// Output can be NULL to specify "do nothing." This rule is followed by all
// the url util functions, so we implement it once here.
//
// Output can be NULL to specify "do nothing." This rule is followed by all the
// url util functions, so we implement it once here.
void ConvertComponents(const url::Parsed& input, PP_URLComponents_Dev* output) {
  if (!output)
    return;

  ConvertComponent(input.scheme, &output->scheme);
  ConvertComponent(input.username, &output->username);
  ConvertComponent(input.password, &output->password);
  ConvertComponent(input.host, &output->host);
  ConvertComponent(input.port, &output->port);
  ConvertComponent(input.path, &output->path);
  ConvertComponent(input.query, &output->query);
  ConvertComponent(input.ref, &output->ref);
}

}  // namespace

// static
PP_Var PPB_URLUtil_Shared::Canonicalize(PP_Var url,
                                        PP_URLComponents_Dev* components) {
  ProxyAutoLock lock;
  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_MakeNull();
  return GenerateURLReturn(GURL(url_string->value()), components);
}

// static
PP_Var PPB_URLUtil_Shared::ResolveRelativeToURL(
    PP_Var base_url,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  ProxyAutoLock lock;
  StringVar* base_url_string = StringVar::FromPPVar(base_url);
  StringVar* relative_string = StringVar::FromPPVar(relative);
  if (!base_url_string || !relative_string)
    return PP_MakeNull();

  GURL base_gurl(base_url_string->value());
  if (!base_gurl.is_valid())
    return PP_MakeNull();
  return GenerateURLReturn(base_gurl.Resolve(relative_string->value()),
                           components);
}

// static
PP_Bool PPB_URLUtil_Shared::IsSameSecurityOrigin(PP_Var url_a, PP_Var url_b) {
  ProxyAutoLock lock;
  StringVar* url_a_string = StringVar::FromPPVar(url_a);
  StringVar* url_b_string = StringVar::FromPPVar(url_b);
  if (!url_a_string || !url_b_string)
    return PP_FALSE;

  GURL gurl_a(url_a_string->value());
  GURL gurl_b(url_b_string->value());
  if (!gurl_a.is_valid() || !gurl_b.is_valid())
    return PP_FALSE;

  return gurl_a.DeprecatedGetOriginAsURL() == gurl_b.DeprecatedGetOriginAsURL()
             ? PP_TRUE
             : PP_FALSE;
}

// Used for returning the given GURL from a PPAPI function, with an optional
// out param indicating the components.
PP_Var PPB_URLUtil_Shared::GenerateURLReturn(const GURL& url,
                                             PP_URLComponents_Dev* components) {
  if (!url.is_valid())
    return PP_MakeNull();
  ConvertComponents(url.parsed_for_possibly_invalid_spec(), components);
  return StringVar::StringToPPVar(url.possibly_invalid_spec());
}

PP_Var PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
    const PP_Var& url,
    PP_URLComponents_Dev* components) {
  if (!components)
    return url;  // Common case - plugin doesn't care about parsing.

  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return url;

  PP_Var result = Canonicalize(url, components);
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(url);
  return result;
}

}  // namespace ppapi
