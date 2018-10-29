// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

bool AllowFromSources(const GURL& url,
                      const std::vector<CSPSource>& sources,
                      CSPContext* context,
                      bool has_followed_redirect) {
  for (const CSPSource& source : sources) {
    if (CSPSource::Allow(source, url, context, has_followed_redirect))
      return true;
  }
  return false;
}

};  // namespace

CSPSourceList::CSPSourceList()
    : allow_self(false),
      allow_star(false),
      allow_response_redirects(false),
      sources() {}

CSPSourceList::CSPSourceList(bool allow_self,
                             bool allow_star,
                             bool allow_response_redirects,
                             std::vector<CSPSource> sources)
    : allow_self(allow_self),
      allow_star(allow_star),
      allow_response_redirects(allow_response_redirects),
      sources(sources) {}

CSPSourceList::CSPSourceList(const CSPSourceList&) = default;
CSPSourceList::~CSPSourceList() = default;

// static
bool CSPSourceList::Allow(const CSPSourceList& source_list,
                          const GURL& url,
                          CSPContext* context,
                          bool has_followed_redirect,
                          bool is_response_check) {
  // If the source list allows all redirects, the decision can't be made until
  // the response is received.
  if (source_list.allow_response_redirects && !is_response_check)
    return true;

  // If the source list does not allow all redirects, the decision has already
  // been made when checking the request.
  if (!source_list.allow_response_redirects && is_response_check)
    return true;

  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list.allow_star) {
    if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS() ||
        url.SchemeIs("ftp")) {
      return true;
    }
    if (context->self_source() && url.SchemeIs(context->self_source()->scheme))
      return true;
  }

  if (source_list.allow_self && context->self_source() &&
      CSPSource::Allow(context->self_source().value(), url, context,
                       has_followed_redirect)) {
    return true;
  }

  return AllowFromSources(url, source_list.sources, context,
                          has_followed_redirect);
}

std::string CSPSourceList::ToString() const {
  if (IsNone())
    return "'none'";
  if (allow_star)
    return "*";

  bool is_empty = true;
  std::stringstream text;
  if (allow_self) {
    text << "'self'";
    is_empty = false;
  }

  for (const auto& source : sources) {
    if (!is_empty)
      text << " ";
    text << source.ToString();
    is_empty = false;
  }

  return text.str();
}

bool CSPSourceList::IsNone() const {
  return !allow_self && !allow_star && sources.empty();
}

}  // namespace content
