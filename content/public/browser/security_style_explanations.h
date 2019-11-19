// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/security_style_explanation.h"
#include "third_party/blink/public/common/security/security_style.h"

namespace content {

// SecurityStyleExplanations provide context for why the specific security style
// was chosen for the page.
//
// Each page has a single security style, which is chosen based on factors like
// whether the page was delivered over HTTPS with a valid certificate, is free
// of mixed content, does not use a deprecated protocol, and is not flagged as
// dangerous.
//
// Each factor that impacts the SecurityStyle has an accompanying
// SecurityStyleExplanation that contains a human-readable explanation of the
// factor. A single page may contain multiple explanations, each of which may
// have a different severity level ("secure", "warning", "insecure" and "info").
struct CONTENT_EXPORT SecurityStyleExplanations {
  SecurityStyleExplanations();
  SecurityStyleExplanations(const SecurityStyleExplanations& other);
  ~SecurityStyleExplanations();

  bool scheme_is_cryptographic;

  // User-visible summary of the security style, set only when
  // the style cannot be determined from HTTPS status alone.
  std::string summary;

  // Explanations corresponding to each security level.

  // |secure_explanations| explains why the page was marked secure.
  std::vector<SecurityStyleExplanation> secure_explanations;
  // |neutral_explanations| explains why the page was marked neutrally: for
  // example, the page's lock icon was taken away due to mixed content, or the
  // page was not loaded over HTTPS.
  std::vector<SecurityStyleExplanation> neutral_explanations;
  // |insecure_explanations| explains why the page was marked as insecure or
  // dangerous: for example, the page was loaded with a certificate error.
  std::vector<SecurityStyleExplanation> insecure_explanations;
  // |info_explanations| contains information that did not affect the page's
  // security style, but is still relevant to the page's security state: for
  // example, an upcoming deprecation that will affect the security style in
  // future.
  std::vector<SecurityStyleExplanation> info_explanations;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_
