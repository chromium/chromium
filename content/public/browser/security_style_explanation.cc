// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/security_style_explanation.h"

#include <utility>

namespace content {

SecurityStyleExplanation::SecurityStyleExplanation() {}

SecurityStyleExplanation::SecurityStyleExplanation(std::string summary,
                                                   std::string description)
    : SecurityStyleExplanation(std::string(),
                               std::move(summary),
                               std::move(description)) {}

SecurityStyleExplanation::SecurityStyleExplanation(std::string title,
                                                   std::string summary,
                                                   std::string description)
    : SecurityStyleExplanation(std::move(title),
                               std::move(summary),
                               std::move(description),
                               {}) {}

SecurityStyleExplanation::SecurityStyleExplanation(
    std::string title,
    std::string summary,
    std::string description,
    scoped_refptr<net::X509Certificate> certificate,
    blink::WebMixedContentContextType mixed_content_type)
    : title(std::move(title)),
      summary(std::move(summary)),
      description(std::move(description)),
      certificate(std::move(certificate)),
      mixed_content_type(mixed_content_type) {}

SecurityStyleExplanation::SecurityStyleExplanation(
    std::string title,
    std::string summary,
    std::string description,
    std::vector<std::string> recommendations)
    : title(std::move(title)),
      summary(std::move(summary)),
      description(std::move(description)),
      mixed_content_type(blink::WebMixedContentContextType::kNotMixedContent),
      recommendations(std::move(recommendations)) {}

SecurityStyleExplanation::SecurityStyleExplanation(
    const SecurityStyleExplanation& other) = default;

SecurityStyleExplanation::SecurityStyleExplanation(
    SecurityStyleExplanation&& other) = default;

SecurityStyleExplanation& SecurityStyleExplanation::operator=(
    const SecurityStyleExplanation& other) = default;

SecurityStyleExplanation& SecurityStyleExplanation::operator=(
    SecurityStyleExplanation&& other) = default;

SecurityStyleExplanation::~SecurityStyleExplanation() {}

}  // namespace content
