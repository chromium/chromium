// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_TEST_BUILDER_H_
#define MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_TEST_BUILDER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
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

  void VerifyExpectations(const MultivariantPlaylist& playlist,
                          const base::Location& from) const override;

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
    absl::optional<types::DecimalInteger> average_bandwidth,
    const base::Location& from,
    const VariantStream& variant) {
  EXPECT_EQ(variant.GetAverageBandwidth(), average_bandwidth)
      << from.ToString();
}

// Checks the value of `GetScore` on the latest variant against the given value.
inline void HasScore(absl::optional<types::DecimalFloatingPoint> score,
                     const base::Location& from,
                     const VariantStream& variant) {
  EXPECT_EQ(variant.GetScore(), score) << from.ToString();
}

// Checks the value of `GetCodecs` on the latest variant against the given
// value.
inline void HasCodecs(absl::optional<base::StringPiece> codecs,
                      const base::Location& from,
                      const VariantStream& variant) {
  EXPECT_EQ(variant.GetCodecs(), codecs) << from.ToString();
}

}  // namespace media::hls

#endif
