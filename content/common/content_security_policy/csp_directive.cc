// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_directive.h"

namespace content {

CSPDirective::CSPDirective() = default;

CSPDirective::CSPDirective(Name name, const CSPSourceList& source_list)
    : name(name), source_list(source_list) {}

CSPDirective::CSPDirective(const CSPDirective&) = default;

std::string CSPDirective::ToString() const {
  return NameToString(name) + " " + source_list.ToString();
}

// static
std::string CSPDirective::NameToString(CSPDirective::Name name) {
  switch (name) {
    case DefaultSrc:
      return "default-src";
    case ChildSrc:
      return "child-src";
    case FrameSrc:
      return "frame-src";
    case FormAction:
      return "form-action";
    case UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case NavigateTo:
      return "navigate-to";
    case FrameAncestors:
      return "frame-ancestors";
    case Unknown:
      return "";
  }
  NOTREACHED();
  return "";
}

// static
CSPDirective::Name CSPDirective::StringToName(const std::string& name) {
  if (name == "default-src")
    return CSPDirective::DefaultSrc;
  if (name == "child-src")
    return CSPDirective::ChildSrc;
  if (name == "frame-src")
    return CSPDirective::FrameSrc;
  if (name == "form-action")
    return CSPDirective::FormAction;
  if (name == "upgrade-insecure-requests")
    return CSPDirective::UpgradeInsecureRequests;
  if (name == "navigate-to")
    return CSPDirective::NavigateTo;
  if (name == "frame-ancestors")
    return CSPDirective::FrameAncestors;
  return CSPDirective::Unknown;
}

}  // namespace content
