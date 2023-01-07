// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/url_util_dev.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_URLUtil_Dev_0_6>() {
  return PPB_URLUTIL_DEV_INTERFACE_0_6;
}

template <> const char* interface_name<PPB_URLUtil_Dev_0_7>() {
  return PPB_URLUTIL_DEV_INTERFACE_0_7;
}

}  // namespace

// static
const URLUtil_Dev* URLUtil_Dev::Get() {
  static URLUtil_Dev util;
  static bool tried_to_init = false;
  static bool interface_available = false;

  if (!tried_to_init) {
    tried_to_init = true;
    if (has_interface<PPB_URLUtil_Dev_0_7>() ||
        has_interface<PPB_URLUtil_Dev_0_6>())
      interface_available = true;
  }

  if (!interface_available)
    return NULL;
  return &util;
}

Var URLUtil_Dev::Canonicalize(const Var& url,
                              PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_7>()->Canonicalize(url.pp_var(),
                                                                  components));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_6>()->Canonicalize(url.pp_var(),
                                                                  components));
  }
  return Var();
}

Var URLUtil_Dev::ResolveRelativeToURL(const Var& base_url,
                                      const Var& relative_string,
                                      PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
      return Var(PASS_REF,
                 get_interface<PPB_URLUtil_Dev_0_7>()->ResolveRelativeToURL(
                     base_url.pp_var(),
                     relative_string.pp_var(),
                     components));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
      return Var(PASS_REF,
                 get_interface<PPB_URLUtil_Dev_0_6>()->ResolveRelativeToURL(
                     base_url.pp_var(),
                     relative_string.pp_var(),
                     components));
  }
  return Var();
}

Var URLUtil_Dev::ResolveRelativeToDocument(
    const InstanceHandle& instance,
    const Var& relative_string,
    PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_7>()->ResolveRelativeToDocument(
                   instance.pp_instance(),
                   relative_string.pp_var(),
                   components));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_6>()->ResolveRelativeToDocument(
                   instance.pp_instance(),
                   relative_string.pp_var(),
                   components));
  }
  return Var();
}

bool URLUtil_Dev::IsSameSecurityOrigin(const Var& url_a,
                                       const Var& url_b) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_7>()->IsSameSecurityOrigin(
            url_a.pp_var(),
            url_b.pp_var()));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_6>()->IsSameSecurityOrigin(
            url_a.pp_var(),
            url_b.pp_var()));
  }
  return false;
}

bool URLUtil_Dev::DocumentCanRequest(const InstanceHandle& instance,
                                     const Var& url) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_7>()->DocumentCanRequest(
            instance.pp_instance(),
            url.pp_var()));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_6>()->DocumentCanRequest(
            instance.pp_instance(),
            url.pp_var()));
  }
  return false;
}

bool URLUtil_Dev::DocumentCanAccessDocument(
    const InstanceHandle& active,
    const InstanceHandle& target) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_7>()->DocumentCanAccessDocument(
            active.pp_instance(),
            target.pp_instance()));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return PP_ToBool(
        get_interface<PPB_URLUtil_Dev_0_6>()->DocumentCanAccessDocument(
            active.pp_instance(),
            target.pp_instance()));
  }
  return false;
}

Var URLUtil_Dev::GetDocumentURL(const InstanceHandle& instance,
                                PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_7>()->GetDocumentURL(
                   instance.pp_instance(),
                   components));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_6>()->GetDocumentURL(
                   instance.pp_instance(),
                   components));
  }
  return Var();
}

Var URLUtil_Dev::GetPluginInstanceURL(const InstanceHandle& instance,
                                      PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_7>()->GetPluginInstanceURL(
                   instance.pp_instance(),
                   components));
  }
  if (has_interface<PPB_URLUtil_Dev_0_6>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_6>()->GetPluginInstanceURL(
                   instance.pp_instance(),
                   components));
  }
  return Var();
}

Var URLUtil_Dev::GetPluginReferrerURL(const InstanceHandle& instance,
                                      PP_URLComponents_Dev* components) const {
  if (has_interface<PPB_URLUtil_Dev_0_7>()) {
    return Var(PASS_REF,
               get_interface<PPB_URLUtil_Dev_0_7>()->GetPluginReferrerURL(
                   instance.pp_instance(),
                   components));
  }
  return Var();
}

}  // namespace pp
