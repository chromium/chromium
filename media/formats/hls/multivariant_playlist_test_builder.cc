// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist_test_builder.h"

#include "base/functional/callback.h"
#include "base/location.h"
#include "media/formats/hls/audio_rendition.h"
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

  // Validate rendition group expectations
  // Begin by constructing a table of group_id -> group
  base::flat_map<std::string, scoped_refptr<AudioRenditionGroup>>
      audio_rendition_groups;
  for (const auto& variant : playlist.GetVariants()) {
    const auto& group = variant.GetAudioRenditionGroup();
    if (group == nullptr) {
      continue;
    }

    auto iter = audio_rendition_groups.find(group->GetId());
    if (iter != audio_rendition_groups.end()) {
      // The same ID should always refer to the same group.
      DCHECK(iter->second.get() == group.get());
    } else {
      audio_rendition_groups.insert(std::make_pair(group->GetId(), group));
    }
  }

  // Check expectations against these groups
  for (const auto& expectation : audio_rendition_group_expectations_) {
    const auto iter = audio_rendition_groups.find(expectation.id);
    EXPECT_NE(iter, audio_rendition_groups.end())
        << expectation.from.ToString();
    expectation.func.Run(expectation.from, *iter->second);
  }

  // Check rendition expectations
  for (const auto& expectation : audio_rendition_expectations_) {
    const auto group_iter = audio_rendition_groups.find(expectation.group_id);
    ASSERT_NE(group_iter, audio_rendition_groups.end())
        << expectation.from.ToString();
    const auto& group = *group_iter->second;

    const auto rendition_iter = base::ranges::find(
        group.GetRenditions(), expectation.name, &AudioRendition::GetName);
    ASSERT_NE(rendition_iter, group.GetRenditions().end())
        << expectation.from.ToString();
    expectation.func.Run(expectation.from, *rendition_iter);
  }
}

}  // namespace media::hls
