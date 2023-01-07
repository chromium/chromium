// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist_test_builder.h"

#include "base/callback.h"
#include "base/location.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/variant_stream.h"

namespace media::hls {

MultivariantPlaylistTestBuilder::MultivariantPlaylistTestBuilder() = default;

MultivariantPlaylistTestBuilder::~MultivariantPlaylistTestBuilder() = default;

MultivariantPlaylistTestBuilder::MultivariantPlaylistTestBuilder(
    const MultivariantPlaylistTestBuilder&) = default;

MultivariantPlaylistTestBuilder::MultivariantPlaylistTestBuilder(
    MultivariantPlaylistTestBuilder&&) = default;

MultivariantPlaylistTestBuilder& MultivariantPlaylistTestBuilder::operator=(
    const MultivariantPlaylistTestBuilder&) = default;

MultivariantPlaylistTestBuilder& MultivariantPlaylistTestBuilder::operator=(
    MultivariantPlaylistTestBuilder&&) = default;

MultivariantPlaylistTestBuilder::VariantExpectations::VariantExpectations() =
    default;

MultivariantPlaylistTestBuilder::VariantExpectations::~VariantExpectations() =
    default;

MultivariantPlaylistTestBuilder::VariantExpectations::VariantExpectations(
    const VariantExpectations&) = default;

MultivariantPlaylistTestBuilder::VariantExpectations::VariantExpectations(
    VariantExpectations&&) = default;

MultivariantPlaylistTestBuilder::VariantExpectations&
MultivariantPlaylistTestBuilder::VariantExpectations::operator=(
    const VariantExpectations&) = default;

MultivariantPlaylistTestBuilder::VariantExpectations&
MultivariantPlaylistTestBuilder::VariantExpectations::operator=(
    VariantExpectations&&) = default;

void MultivariantPlaylistTestBuilder::VerifyExpectations(
    const MultivariantPlaylist& playlist,
    const base::Location& from) const {
  ASSERT_EQ(variant_expectations_.size(), playlist.GetVariants().size())
      << from.ToString();
  for (size_t i = 0; i < variant_expectations_.size(); ++i) {
    const auto& variant = playlist.GetVariants().at(i);
    const auto& expectations = variant_expectations_.at(i);
    for (const auto& expectation : expectations.expectations) {
      expectation.Run(variant);
    }
  }
}

}  // namespace media::hls
