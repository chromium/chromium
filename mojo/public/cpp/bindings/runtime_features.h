// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RUNTIME_FEATURES_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RUNTIME_FEATURES_H_

namespace mojo::internal {

// Trait is `true` if a mojom interface has a RuntimeFeature= attribute.
template <typename Interface>
inline constexpr bool kIsRuntimeFeatureGuarded = false;

// Helper to allow this to be a no-op on non-feature annotated interfaces.
template <typename Interface>
inline constexpr bool GetRuntimeFeature_IsEnabled() {
  return true;
}

// Helper to allow this to be a no-op on non-feature annotated interfaces.
// Call this if it would be sensible to DCHECK/DumpWithoutCrashing if the
// RuntimeFeature for `Interface` is not enabled.
template <typename Interface>
inline constexpr bool GetRuntimeFeature_ExpectEnabled() {
  return true;
}

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RUNTIME_FEATURES_H_
