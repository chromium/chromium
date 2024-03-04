// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PLAYLIST_TEST_BUILDER_H_
#define MEDIA_FORMATS_HLS_PLAYLIST_TEST_BUILDER_H_

#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "media/base/media_serializers_base.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media::hls {

// Base helper for building playlist test cases. This should be extended by a
// playlist-type specific builder with additional methods for creating
// assertions specific to that type of playlist, and parameterized by the type
// of playlist.
template <typename PlaylistT>
class PlaylistTestBuilder {
 public:
  static_assert(std::is_base_of_v<Playlist, PlaylistT>);

  // Sets the URI for the playlist being built.
  void SetUri(GURL uri) { uri_ = std::move(uri); }

  // Sets the expected version for the playlist being built.
  void SetVersion(types::DecimalInteger version) { version_ = version; }

  // Appends fragments of text to the playlist, without a trailing newline.
  template <typename... T>
  void Append(std::string_view text1, T&&... rem) {
    for (auto text : {text1, std::string_view(rem)...}) {
      source_.append(text);
    }
  }

  // Appends fragments of text to the playlist, followed by a newline.
  template <typename... T>
  void AppendLine(std::string_view part1, T&&... rem) {
    this->Append(part1, std::forward<T>(rem)..., "\n");
  }

  // Adds a new expectation for the playlist, which will be checked during
  // `ExpectOk`.
  template <typename Fn, typename Arg>
  void ExpectPlaylist(Fn fn,
                      Arg arg,
                      base::Location location = base::Location::Current()) {
    playlist_expectations_.push_back(base::BindRepeating(
        [](Fn fn, Arg arg, const base::Location& from,
           const PlaylistT& playlist) { fn(arg, from, playlist); },
        std::move(fn), std::move(arg), std::move(location)));
  }

  template <typename... Args>
  scoped_refptr<PlaylistT> Parse(
      Args&&... args,
      const base::Location& from = base::Location::Current()) {
    auto result =
        PlaylistT::Parse(source_, uri_, version_, std::forward<Args>(args)...);

    if (!result.has_value()) {
      EXPECT_TRUE(result.has_value())
          << MediaSerialize(std::move(result).error())
          << "\nFrom: " << from.ToString();
      return nullptr;
    } else {
      auto playlist = std::move(result).value();
      // Ensure that playlist has expected version
      EXPECT_EQ(playlist->GetVersion(), version_) << from.ToString();
      return std::move(playlist);
    }
  }

 protected:
  // Attempts to parse the playlist as-is, checking for the given
  // error code.
  template <typename... Args>
  void ExpectError(ParseStatusCode code,
                   const base::Location& from,
                   Args&&... args) const {
    auto result =
        PlaylistT::Parse(source_, uri_, version_, std::forward<Args>(args)...);
    ASSERT_FALSE(result.has_value()) << from.ToString();

    auto actual_code = std::move(result).error().code();
    EXPECT_EQ(actual_code, code)
        << "Error: " << ParseStatusCodeToString(actual_code) << "\n"
        << "Expected Error: " << ParseStatusCodeToString(code) << "\n"
        << from.ToString();
  }

  // Attempts to parse the playlist as-is, checking all playlist and segment
  // expectations.
  template <typename... Args>
  void ExpectOk(const base::Location& from, Args&&... args) const {
    auto result =
        PlaylistT::Parse(source_, uri_, version_, std::forward<Args>(args)...);
    ASSERT_TRUE(result.has_value())
        << "Error: "
        << ParseStatusCodeToString(std::move(result).error().code()) << "\n"
        << from.ToString();
    auto playlist = std::move(result).value();

    // Ensure that playlist has expected version
    EXPECT_EQ(playlist->GetVersion(), version_) << from.ToString();

    for (const auto& expectation : playlist_expectations_) {
      expectation.Run(*playlist);
    }

    this->VerifyExpectations(*playlist, from);
  }

 private:
  virtual void VerifyExpectations(const PlaylistT&,
                                  const base::Location& from) const = 0;

  std::vector<base::RepeatingCallback<void(const PlaylistT&)>>
      playlist_expectations_;
  GURL uri_ = GURL("http://localhost/playlist.m3u8");
  types::DecimalInteger version_ = Playlist::kDefaultVersion;
  std::string source_;
};

// Checks the playlist's `AreSegmentsIndependent` property against the given
// value.
inline void HasIndependentSegments(bool value,
                                   const base::Location& from,
                                   const Playlist& playlist) {
  EXPECT_EQ(playlist.AreSegmentsIndependent(), value) << from.ToString();
}

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PLAYLIST_TEST_BUILDER_H_
