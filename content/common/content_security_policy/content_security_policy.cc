// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

static CSPDirective::Name CSPFallback(CSPDirective::Name directive) {
  switch (directive) {
    case CSPDirective::DefaultSrc:
    case CSPDirective::FormAction:
    case CSPDirective::UpgradeInsecureRequests:
    case CSPDirective::NavigateTo:
      return CSPDirective::Unknown;

    case CSPDirective::FrameSrc:
      return CSPDirective::ChildSrc;

    case CSPDirective::ChildSrc:
      return CSPDirective::DefaultSrc;

    case CSPDirective::Unknown:
      NOTREACHED();
      return CSPDirective::Unknown;
  }
  NOTREACHED();
  return CSPDirective::Unknown;
}

// Looks by name for a directive in a list of directives.
// If it is not found, returns nullptr.
static const CSPDirective* FindDirective(
    const CSPDirective::Name name,
    const std::vector<CSPDirective>& directives) {
  for (const CSPDirective& directive : directives) {
    if (directive.name == name) {
      return &directive;
    }
  }
  return nullptr;
}

std::string ElideURLForReportViolation(const GURL& url) {
  // TODO(arthursonzogni): the url length should be limited to 1024 char. Find
  // a function that will not break the utf8 encoding while eliding the string.
  return url.spec();
}

void ReportViolation(CSPContext* context,
                     const ContentSecurityPolicy& policy,
                     const CSPDirective& directive,
                     const CSPDirective::Name directive_name,
                     const GURL& url,
                     bool has_followed_redirect,
                     const SourceLocation& source_location) {
  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(directive_name, CSPDirective::DefaultSrc);
  DCHECK_NE(directive_name, CSPDirective::ChildSrc);

  // For security reasons, some urls must not be disclosed. This includes the
  // blocked url and the source location of the error. Care must be taken to
  // ensure that these are not transmitted between different cross-origin
  // renderers.
  GURL safe_url = url;
  SourceLocation safe_source_location = source_location;
  context->SanitizeDataForUseInCspViolation(
      has_followed_redirect, directive_name, &safe_url, &safe_source_location);

  std::stringstream message;

  if (policy.header.type == blink::kWebContentSecurityPolicyTypeReport)
    message << "[Report Only] ";

  if (directive_name == CSPDirective::FormAction)
    message << "Refused to send form data to '";
  else if (directive_name == CSPDirective::FrameSrc)
    message << "Refused to frame '";
  else if (directive_name == CSPDirective::NavigateTo)
    message << "Refused to navigate to '";

  message << ElideURLForReportViolation(safe_url)
          << "' because it violates the following Content Security Policy "
             "directive: \""
          << directive.ToString() << "\".";

  if (directive.name != directive_name)
    message << " Note that '" << CSPDirective::NameToString(directive_name)
            << "' was not explicitly set, so '"
            << CSPDirective::NameToString(directive.name)
            << "' is used as a fallback.";

  message << "\n";

  context->ReportContentSecurityPolicyViolation(CSPViolationParams(
      CSPDirective::NameToString(directive.name),
      CSPDirective::NameToString(directive_name), message.str(), safe_url,
      policy.report_endpoints, policy.use_reporting_api,
      policy.header.header_value, policy.header.type, has_followed_redirect,
      safe_source_location));
}

bool AllowDirective(CSPContext* context,
                    const ContentSecurityPolicy& policy,
                    const CSPDirective& directive,
                    CSPDirective::Name directive_name,
                    const GURL& url,
                    bool has_followed_redirect,
                    bool is_response_check,
                    const SourceLocation& source_location) {
  if (CSPSourceList::Allow(directive.source_list, url, context,
                           has_followed_redirect, is_response_check)) {
    return true;
  }

  ReportViolation(context, policy, directive, directive_name, url,
                  has_followed_redirect, source_location);
  return false;
}

const GURL ExtractInnerURL(const GURL& url) {
  if (const GURL* inner_url = url.inner_url())
    return *inner_url;
  else
    // TODO(arthursonzogni): revisit this once GURL::inner_url support blob-URL.
    return GURL(url.path());
}

bool ShouldBypassContentSecurityPolicy(CSPContext* context, const GURL& url) {
  if (url.SchemeIsFileSystem() || url.SchemeIsBlob()) {
    return context->SchemeShouldBypassCSP(ExtractInnerURL(url).scheme());
  } else {
    return context->SchemeShouldBypassCSP(url.scheme());
  }
}

}  // namespace

ContentSecurityPolicy::ContentSecurityPolicy() = default;

ContentSecurityPolicy::ContentSecurityPolicy(
    const ContentSecurityPolicyHeader& header,
    const std::vector<CSPDirective>& directives,
    const std::vector<std::string>& report_endpoints,
    bool use_reporting_api)
    : header(header),
      directives(directives),
      report_endpoints(report_endpoints),
      use_reporting_api(use_reporting_api) {}

ContentSecurityPolicy::ContentSecurityPolicy(
    const ContentSecurityPolicy& other) = default;

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

// static
bool ContentSecurityPolicy::Allow(const ContentSecurityPolicy& policy,
                                  CSPDirective::Name directive_name,
                                  const GURL& url,
                                  bool has_followed_redirect,
                                  bool is_response_check,
                                  CSPContext* context,
                                  const SourceLocation& source_location,
                                  bool is_form_submission) {
  if (ShouldBypassContentSecurityPolicy(context, url))
    return true;

  // 'navigate-to' has no effect when doing a form submission and a
  // 'form-action' directive is present.
  if (is_form_submission && directive_name == CSPDirective::Name::NavigateTo &&
      FindDirective(CSPDirective::Name::FormAction, policy.directives)) {
    return true;
  }

  CSPDirective::Name current_directive_name = directive_name;
  do {
    const CSPDirective* current_directive =
        FindDirective(current_directive_name, policy.directives);
    if (current_directive) {
      bool allowed = AllowDirective(context, policy, *current_directive,
                                    directive_name, url, has_followed_redirect,
                                    is_response_check, source_location);
      return allowed ||
             policy.header.type == blink::kWebContentSecurityPolicyTypeReport;
    }
    current_directive_name = CSPFallback(current_directive_name);
  } while (current_directive_name != CSPDirective::Unknown);
  return true;
}

std::string ContentSecurityPolicy::ToString() const {
  std::stringstream text;
  bool is_first_policy = true;
  for (const CSPDirective& directive : directives) {
    if (!is_first_policy)
      text << "; ";
    is_first_policy = false;
    text << directive.ToString();
  }

  if (!report_endpoints.empty()) {
    if (!is_first_policy)
      text << "; ";
    is_first_policy = false;
    text << "report-uri";
    for (const std::string& endpoint : report_endpoints)
      text << " " << endpoint;
  }

  return text.str();
}

// static
bool ContentSecurityPolicy::ShouldUpgradeInsecureRequest(
    const ContentSecurityPolicy& policy) {
  for (const CSPDirective& directive : policy.directives) {
    if (directive.name == CSPDirective::UpgradeInsecureRequests)
      return true;
  }
  return false;
}

}  // namespace content
