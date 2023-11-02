// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/resource.h"

#include <algorithm>

#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

namespace pp {

Resource::Resource() : pp_resource_(0) {
}

Resource::Resource(const Resource& other) : pp_resource_(other.pp_resource_) {
  if (!is_null())
    Module::Get()->core()->AddRefResource(pp_resource_);
}

Resource::~Resource() {
  if (!is_null())
    Module::Get()->core()->ReleaseResource(pp_resource_);
}

Resource& Resource::operator=(const Resource& other) {
  if (!other.is_null())
    Module::Get()->core()->AddRefResource(other.pp_resource_);
  if (!is_null())
    Module::Get()->core()->ReleaseResource(pp_resource_);
  pp_resource_ = other.pp_resource_;
  return *this;
}

PP_Resource Resource::detach() {
  PP_Resource ret = pp_resource_;
  pp_resource_ = 0;
  return ret;
}

Resource::Resource(PP_Resource resource) : pp_resource_(resource) {
  if (!is_null())
    Module::Get()->core()->AddRefResource(pp_resource_);
}

Resource::Resource(PassRef, PP_Resource resource) : pp_resource_(resource) {
}

void Resource::PassRefFromConstructor(PP_Resource resource) {
  PP_DCHECK(!pp_resource_);
  pp_resource_ = resource;
}

void Resource::Clear() {
  if (is_null())
    return;
  Module::Get()->core()->ReleaseResource(pp_resource_);
  pp_resource_ = 0;
}

}  // namespace pp
