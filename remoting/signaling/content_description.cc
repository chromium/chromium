// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/content_description.h"

#include <memory>
#include <utility>

namespace remoting {

const char ContentDescription::kChromotingContentName[] = "chromoting";

ContentDescription::ContentDescription(
    const JingleAuthentication& authentication)
    : authentication_(authentication) {}

ContentDescription::ContentDescription(const ContentDescription& other)
    : authentication_(other.authentication_) {}

ContentDescription::~ContentDescription() = default;

std::unique_ptr<ContentDescription> ContentDescription::Clone() const {
  return std::make_unique<ContentDescription>(*this);
}

}  // namespace remoting
