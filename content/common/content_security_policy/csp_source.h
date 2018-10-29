// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class CSPContext;

// A CSPSource represents an expression that matches a set of urls.
// Examples of CSPSource:
// - domain.example.com
// - *.example.com
// - https://cdn.com
// - data:
// - 'none'
// - 'self'
// - *
struct CONTENT_EXPORT CSPSource {
  CSPSource();
  CSPSource(const std::string& scheme,
            const std::string& host,
            bool is_host_wildcard,
            int port,
            bool is_port_wildcard,
            const std::string& path);
  CSPSource(const CSPSource& source);
  ~CSPSource();

  // Scheme.
  std::string scheme;

  // Host.
  std::string host;
  bool is_host_wildcard;

  // port.
  int port;
  bool is_port_wildcard;

  // Path.
  std::string path;

  std::string ToString() const;

  bool IsSchemeOnly() const;
  bool HasPort() const;
  bool HasHost() const;
  bool HasPath() const;

  // Returns true if the |source| matches the |url| for a given |context|.
  static bool Allow(const CSPSource& source,
                    const GURL& url,
                    CSPContext* context,
                    bool has_followed_redirect = false);
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
