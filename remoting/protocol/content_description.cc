// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/content_description.h"

#include <memory>
#include <utility>

namespace remoting::protocol {

const char ContentDescription::kChromotingContentName[] = "chromoting";

ContentDescription::ContentDescription(
    std::unique_ptr<CandidateSessionConfig> config,
    const JingleAuthentication& authentication)
    : candidate_config_(std::move(config)), authentication_(authentication) {}

ContentDescription::~ContentDescription() = default;

}  // namespace remoting::protocol
