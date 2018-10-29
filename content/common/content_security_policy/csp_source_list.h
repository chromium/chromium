// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_

#include <vector>

#include "content/common/content_security_policy/csp_source.h"
#include "url/gurl.h"

namespace content {

class CSPContext;

struct CONTENT_EXPORT CSPSourceList {
  CSPSourceList();
  CSPSourceList(bool allow_self,
                bool allow_star,
                bool allow_response_redirects,
                std::vector<CSPSource> source_list);
  CSPSourceList(const CSPSourceList&);
  ~CSPSourceList();

  // Wildcard hosts and 'self' aren't stored in source_list, but as attributes
  // on the source list itself.
  bool allow_self;
  bool allow_star;
  bool allow_response_redirects;
  std::vector<CSPSource> sources;

  std::string ToString() const;

  bool IsNone() const;

  // Return true when at least one source in the |source_list| matches the
  // |url| for a given |context|.
  static bool Allow(const CSPSourceList& source_list,
                    const GURL& url,
                    CSPContext* context,
                    bool has_followed_redirect = false,
                    bool is_response_check = false);
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
