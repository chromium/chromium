// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_RUSTY_PNG_FEATURE_H_
#define SKIA_RUSTY_PNG_FEATURE_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "skia/buildflags.h"

namespace skia {

// Exposing the feature so that tests can inspect it and turn it on/off,
// but product code should instead use `IsRustyPngEnabled`.
COMPONENT_EXPORT(SKIA_RUSTY_PNG_FEATURE_DETECTION)
BASE_DECLARE_FEATURE(kRustyPngFeature);

// Returns true if Rust should be used for PNG decoding:
// 1) the GN-level `enable_rust_png` is true.
// *and*
// 2) the `"RustyPng"` base::Feature has been enabled.
//
// See also  https://crbug.com/40278281 and the "Rollout plan" in
// https://docs.google.com/document/d/1glx5ue5JDlCld5WzWgTOGK3wsMErQFnkY5N5Dsbi91Y
inline bool IsRustyPngEnabled() {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
  return base::FeatureList::IsEnabled(kRustyPngFeature);
#else
  return false;
#endif
}

}  // namespace skia

#endif  // SKIA_RUSTY_PNG_FEATURE_H_
