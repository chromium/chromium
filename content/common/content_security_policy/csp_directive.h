// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_DIRECTIVE_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_DIRECTIVE_

#include <string>
#include "content/common/content_export.h"
#include "content/common/content_security_policy/csp_source_list.h"

namespace content {

// CSPDirective contains a set of allowed sources for a given Content Security
// Policy directive.
//
// For example, the Content Security Policy `default-src img.cdn.com
// example.com` would produce a CSPDirective object whose 'name' is
// 'DefaultSrc', and whose 'source_list' contains two CSPSourceExpressions
// representing 'img.cdn.com' and 'example.com' respectively.
//
// https://w3c.github.io/webappsec-csp/#framework-directives
struct CONTENT_EXPORT CSPDirective {
  enum Name {
    DefaultSrc,
    ChildSrc,
    FrameSrc,
    FormAction,
    UpgradeInsecureRequests,
    NavigateTo,
    FrameAncestors,

    Unknown,
    NameLast = Unknown,
  };

  static std::string NameToString(Name name);
  static Name StringToName(const std::string& name);

  CSPDirective();
  CSPDirective(Name name, const CSPSourceList& source_list);
  CSPDirective(const CSPDirective&);

  Name name;
  CSPSourceList source_list;

  std::string ToString() const;
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_DIRECTIVE_
