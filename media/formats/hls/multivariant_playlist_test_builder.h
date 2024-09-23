// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_TEST_BUILDER_H_
#define MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_TEST_BUILDER_H_

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/playlist_test_builder.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"

namespace media::hls {

// Helper for building multivariant playlist test cases that allows writing
// assertions next to the playlist lines they check, as well as "forking" test
// cases via copying the builder.
class MultivariantPlaylistTestBuilder
    : public PlaylistTestBuilder<MultivariantPlaylist> {
 public:
  MultivariantPlaylistTestBuilder();
  ~MultivariantPlaylistTestBuilder();
  MultivariantPlaylistTestBuilder(const MultivariantPlaylistTestBuilder&);
  MultivariantPlaylistTestBuilder(MultivariantPlaylistTestBuilder&&);
  MultivariantPlaylistTestBuilder& operator=(
      const MultivariantPlaylistTestBuilder&);
  MultivariantPlaylistTestBuilder& operator=(MultivariantPlaylistTestBuilder&&);

  // Increments the number of variants that are expected to be contained in the
  // playlist.
  void ExpectAdditionalVariant() { variant_expectations_.emplace_back(); }

  // Adds a new expectation for the latest variant in the playlist, which will
  // be checked during `ExpectOk`.
  template <typename Fn, typename Arg>
  void ExpectVariant(Fn fn,
                     Arg arg,
                     base::Location location = base::Location::Current()) {
    variant_expectations_.back().expectations.push_back(base::BindRepeating(
        std::move(fn), std::move(arg), std::move(location)));
  }

  // Adds a new expectation for the audio rendition group identified by `id`.
  // The test will fail if the group does not exist, or is unreferenced by
  // any variant.
  template <typename Fn, typename Arg>
  void ExpectAudioRenditionGroup(
      std::string id,
      Fn fn,
      Arg arg,
      base::Location from = base::Location::Current()) {
    ExpectRenditionGroup(audio_rendition_group_expectations_, std::move(id),
                         std::move(fn), std::move(arg), std::move(from));
  }

  // Adds a new expectation for the audio rendition identified by `group_id` and
  // `name`. The test will fail if the rendition does not exist, or its group is
  // unreferenced by any variant.
  template <typename Fn, typename Arg>
  void ExpectAudioRendition(std::string group_id,
                            std::string name,
                            Fn fn,
                            Arg arg,
                            base::Location from = base::Location::Current()) {
    ExpectRendition(audio_rendition_expectations_, std::move(group_id),
                    std::move(name), std::move(fn), std::move(arg),
                    std::move(from));
  }

  void ExpectOk(const base::Location& from = base::Location::Current()) const {
    PlaylistTestBuilder::ExpectOk(from);
  }

  void ExpectError(
      ParseStatusCode code,
      const base::Location& from = base::Location::Current()) const {
    PlaylistTestBuilder::ExpectError(code, from);
  }

 private:
  struct VariantExpectations {
    VariantExpectations();
    ~VariantExpectations();
    VariantExpectations(const VariantExpectations&);
    VariantExpectations(VariantExpectations&&);
    VariantExpectations& operator=(const VariantExpectations&);
    VariantExpectations& operator=(VariantExpectations&&);

    std::vector<base::RepeatingCallback<void(const VariantStream&)>>
        expectations;
  };

  template <typename T>
  struct RenditionGroupExpectation {
    base::Location from;
    std::string id;
    base::RepeatingCallback<void(const base::Location&, const T&)> func;
  };

  template <typename T>
  struct RenditionExpectation {
    base::Location from;
    std::string group_id;
    std::string name;
    base::RepeatingCallback<void(const base::Location&, const T&)> func;
  };

  template <typename Expectation, typename Fn, typename Arg>
  void ExpectRenditionGroup(std::vector<Expectation>& type_expectations,
                            std::string id,
                            Fn fn,
                            Arg arg,
                            base::Location from) {
    type_expectations.push_back(Expectation{
        .from = from,
        .id = std::move(id),
        .func = base::BindRepeating(std::move(fn), std::move(arg)),
    });
  }

  template <typename Expectation, typename Fn, typename Arg>
  void ExpectRendition(std::vector<Expectation>& type_expectations,
                       std::string group_id,
                       std::string name,
                       Fn fn,
                       Arg arg,
                       base::Location from) {
    type_expectations.push_back(Expectation{
        .from = from,
        .group_id = std::move(group_id),
        .name = std::move(name),
        .func = base::BindRepeating(std::move(fn), std::move(arg)),
    });
  }

  void VerifyExpectations(const MultivariantPlaylist& playlist,
                          const base::Location& from) const override;

  std::vector<RenditionGroupExpectation<AudioRenditionGroup>>
      audio_rendition_group_expectations_;
  std::vector<RenditionExpectation<AudioRendition>>
      audio_rendition_expectations_;
  std::vector<VariantExpectations> variant_expectations_;
};

// Checks that the latest variant has the given primary rendition URI.
inline void HasPrimaryRenditionUri(const GURL& uri,
                                   const base::Location& from,
                                   const VariantStream& variant) {
  EXPECT_EQ(variant.GetPrimaryRenditionUri(), uri) << from.ToString();
}

// Checks the value of `GetBandwidth` on the latest variant against the given
// value.
inline void HasBandwidth(types::DecimalInteger bandwidth,
                         const base::Location& from,
                         const VariantStream& variant) {
  EXPECT_EQ(variant.GetBandwidth(), bandwidth) << from.ToString();
}

// Checks the value of `GetAverageBandwidth` on the latest variant against the
// given value.
inline void HasAverageBandwidth(
    std::optional<types::DecimalInteger> average_bandwidth,
    const base::Location& from,
    const VariantStream& variant) {
  EXPECT_EQ(variant.GetAverageBandwidth(), average_bandwidth)
      << from.ToString();
}

// Checks the value of `GetScore` on the latest variant against the given value.
inline void HasScore(std::optional<types::DecimalFloatingPoint> score,
                     const base::Location& from,
                     const VariantStream& variant) {
  EXPECT_EQ(variant.GetScore(), score) << from.ToString();
}

// Checks the value of `GetCodecs` on the latest variant against the given
// value.
inline void HasCodecs(std::optional<std::vector<std::string>> codecs,
                      const base::Location& from,
                      const VariantStream& variant) {
  EXPECT_EQ(variant.GetCodecs(), codecs) << from.ToString();
}

// Checks the value of `GetResolution` on the latest variant against the given
// value.
inline void HasResolution(std::optional<types::DecimalResolution> resolution,
                          const base::Location& from,
                          const VariantStream& variant) {
  ASSERT_EQ(variant.GetResolution().has_value(), resolution.has_value())
      << from.ToString();
  if (resolution.has_value()) {
    EXPECT_EQ(variant.GetResolution()->width, resolution->width)
        << from.ToString();
    EXPECT_EQ(variant.GetResolution()->height, resolution->height)
        << from.ToString();
  }
}

// Checks the value of `GetFrameRate` on the latest variant against the given
// value.
inline void HasFrameRate(std::optional<types::DecimalFloatingPoint> frame_rate,
                         const base::Location& from,
                         const VariantStream& variant) {
  ASSERT_EQ(variant.GetFrameRate().has_value(), frame_rate.has_value())
      << from.ToString();
  if (frame_rate.has_value()) {
    EXPECT_DOUBLE_EQ(variant.GetFrameRate().value(), frame_rate.value())
        << from.ToString();
  }
}

// Checks that the audio rendition group associated with the latest variant has
// the given `group_id`.
inline void HasAudioRenditionGroup(std::optional<std::string> group_id,
                                   const base::Location& from,
                                   const VariantStream& variant) {
  if (variant.GetAudioRenditionGroup()) {
    EXPECT_EQ(variant.GetAudioRenditionGroup()->GetId(), group_id)
        << from.ToString();
  } else {
    EXPECT_EQ(std::nullopt, group_id) << from.ToString();
  }
}

// Checks that the audio rendition has the given URI.
inline void RenditionHasUri(std::optional<GURL> uri,
                            const base::Location& from,
                            const AudioRendition& rendition) {
  EXPECT_EQ(rendition.GetUri(), uri) << from.ToString();
}

// Checks that the audio rendition has the given language.
inline void HasLanguage(std::optional<std::string> language,
                        const base::Location& from,
                        const AudioRendition& rendition) {
  EXPECT_EQ(rendition.GetLanguage(), language) << from.ToString();
}

// Checks that the audio rendition has the given associated language.
inline void HasAssociatedLanguage(std::optional<std::string> language,
                                  const base::Location& from,
                                  const AudioRendition& rendition) {
  EXPECT_EQ(rendition.GetAssociatedLanguage(), language) << from.ToString();
}

// Checks that the audio rendition has the given StableId.
inline void HasStableRenditionId(std::optional<types::StableId> id,
                                 const base::Location& from,
                                 const AudioRendition& rendition) {
  EXPECT_EQ(rendition.GetStableRenditionId(), id) << from.ToString();
}

// Checks that the audio rendition may be autoselected (AUTOSELECT=YES or
// DEFAULT=YES).
inline void MayAutoSelect(bool value,
                          const base::Location& from,
                          const AudioRendition& rendition) {
  EXPECT_EQ(rendition.MayAutoSelect(), value) << from.ToString();
}

// Checks that the audio rendition group has a default rendition with the given
// name (or `std::nullopt` for no default rendition).
inline void HasDefaultRendition(std::optional<std::string> name,
                                const base::Location& from,
                                const AudioRenditionGroup& group) {
  if (group.GetDefaultRendition()) {
    EXPECT_EQ(group.GetDefaultRendition()->GetName(), name) << from.ToString();
  } else {
    EXPECT_EQ(std::nullopt, name) << from.ToString();
  }
}

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_TEST_BUILDER_H_
