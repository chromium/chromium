// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/instance.h"

namespace media {
namespace learning {

Instance::Instance() = default;
Instance::~Instance() = default;
Instance::Instance(Instance&& rhs) noexcept {
  features = std::move(rhs.features);
}

std::ostream& operator<<(std::ostream& out, const Instance& instance) {
  for (const auto& feature : instance.features)
    out << " " << feature;

  return out;
}

}  // namespace learning
}  // namespace media
