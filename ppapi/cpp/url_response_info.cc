// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_response_info.h"

#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_URLResponseInfo_1_0>() {
  return PPB_URLRESPONSEINFO_INTERFACE_1_0;
}

}  // namespace

URLResponseInfo::URLResponseInfo(const URLResponseInfo& other)
    : Resource(other) {}

URLResponseInfo& URLResponseInfo::operator=(const URLResponseInfo& other) {
  Resource::operator=(other);
  return *this;
}

URLResponseInfo::URLResponseInfo(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

Var URLResponseInfo::GetProperty(PP_URLResponseProperty property) const {
  if (!has_interface<PPB_URLResponseInfo_1_0>())
    return Var();
  return Var(PASS_REF,
      get_interface<PPB_URLResponseInfo_1_0>()->GetProperty(pp_resource(),
                                                            property));
}

FileRef URLResponseInfo::GetBodyAsFileRef() const {
  if (!has_interface<PPB_URLResponseInfo_1_0>())
    return FileRef();
  return FileRef(PASS_REF,
      get_interface<PPB_URLResponseInfo_1_0>()->GetBodyAsFileRef(
          pp_resource()));
}

}  // namespace pp
