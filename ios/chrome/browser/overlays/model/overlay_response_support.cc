// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_response_support.h"

#include "base/no_destructor.h"

namespace {
// OverlayResponseSupport that always returns true for IsRequestSupported().
class UniversalOverlayResponseSupport : public OverlayResponseSupport {
 public:
  bool IsResponseSupported(OverlayResponse* response) const override {
    return true;
  }
};
// OverlayResponseSupport that always returns false for IsRequestSupported().
class DisabledOverlayResponseSupport : public OverlayResponseSupport {
 public:
  bool IsResponseSupported(OverlayResponse* response) const override {
    return false;
  }
};
}  // namespace

OverlayResponseSupport::OverlayResponseSupport(
    const std::vector<const OverlayResponseSupport*>& supports)
    : aggregated_support_(supports) {}

OverlayResponseSupport::OverlayResponseSupport() = default;

OverlayResponseSupport::~OverlayResponseSupport() = default;

bool OverlayResponseSupport::IsResponseSupported(
    OverlayResponse* response) const {
  for (const OverlayResponseSupport* support : aggregated_support_) {
    if (support->IsResponseSupported(response))
      return true;
  }
  return false;
}

// static
const OverlayResponseSupport* OverlayResponseSupport::All() {
  static base::NoDestructor<UniversalOverlayResponseSupport> support;
  return support.get();
}

// static
const OverlayResponseSupport* OverlayResponseSupport::None() {
  static base::NoDestructor<DisabledOverlayResponseSupport> support;
  return support.get();
}
