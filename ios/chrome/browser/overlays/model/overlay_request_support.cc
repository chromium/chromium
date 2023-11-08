// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

#include "base/no_destructor.h"

namespace {
// OverlayRequestSupport that always returns true for IsRequestSupported().
class UniversalOverlayRequestSupport : public OverlayRequestSupport {
 public:
  bool IsRequestSupported(OverlayRequest* request) const override {
    return true;
  }
};
// OverlayRequestSupport that always returns false for IsRequestSupported().
class DisabledOverlayRequestSupport : public OverlayRequestSupport {
 public:
  bool IsRequestSupported(OverlayRequest* request) const override {
    return false;
  }
};
}  // namespace

OverlayRequestSupport::OverlayRequestSupport(
    const std::vector<const OverlayRequestSupport*>& supports)
    : aggregated_support_(supports) {}

OverlayRequestSupport::OverlayRequestSupport() = default;

OverlayRequestSupport::~OverlayRequestSupport() = default;

bool OverlayRequestSupport::IsRequestSupported(OverlayRequest* request) const {
  for (const OverlayRequestSupport* support : aggregated_support_) {
    if (support->IsRequestSupported(request))
      return true;
  }
  return false;
}

// static
const OverlayRequestSupport* OverlayRequestSupport::All() {
  static base::NoDestructor<UniversalOverlayRequestSupport> support;
  return support.get();
}

// static
const OverlayRequestSupport* OverlayRequestSupport::None() {
  static base::NoDestructor<DisabledOverlayRequestSupport> support;
  return support.get();
}
