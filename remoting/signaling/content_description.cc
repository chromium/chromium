// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/content_description.h"

#include <memory>
#include <utility>

namespace remoting {

const char ContentDescription::kChromotingContentName[] = "chromoting";

ContentDescription::ContentDescription(
    std::unique_ptr<CandidateSessionConfig> config,
    const JingleAuthentication& authentication)
    : candidate_config_(std::move(config)), authentication_(authentication) {}

ContentDescription::ContentDescription(const ContentDescription& other)
    : candidate_config_(other.candidate_config_->Clone()),
      authentication_(other.authentication_) {}

ContentDescription::~ContentDescription() = default;

std::unique_ptr<ContentDescription> ContentDescription::Clone() const {
  return std::make_unique<ContentDescription>(*this);
}

}  // namespace remoting
