// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_var.h"

PluginResourceVar::PluginResourceVar() {}

PluginResourceVar::PluginResourceVar(ppapi::Resource* resource)
    : resource_(resource) {}

PP_Resource PluginResourceVar::GetPPResource() const {
  return resource_.get() ? resource_->pp_resource() : 0;
}

bool PluginResourceVar::IsPending() const {
  return false;
}

PluginResourceVar::~PluginResourceVar() {}
