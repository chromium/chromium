// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tag_name.h"

#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"

namespace media::hls {

namespace {
// Ensure that tag name enums are disjoint.
template <typename A, typename B>
constexpr bool are_disjoint() {
  return ToTagName(A::kMaxValue) < ToTagName(B::kMinValue) ||
         ToTagName(B::kMaxValue) < ToTagName(A::kMinValue);
}

// Ensure all tag names are within the range [kMinValue, kMaxValue]
static_assert(ToTagName(CommonTagName::kMinValue) >= kMinTagName &&
              ToTagName(CommonTagName::kMaxValue) <= kMaxTagName);
static_assert(ToTagName(MultivariantPlaylistTagName::kMinValue) >=
                  kMinTagName &&
              ToTagName(MultivariantPlaylistTagName::kMaxValue) <= kMaxTagName);
static_assert(ToTagName(MediaPlaylistTagName::kMinValue) >= kMinTagName &&
              ToTagName(MediaPlaylistTagName::kMaxValue) <= kMaxTagName);

// Ensure tag name sets are contiguous
static_assert(ToTagName(MultivariantPlaylistTagName::kMinValue) ==
              ToTagName(CommonTagName::kMaxValue) + 1);
static_assert(ToTagName(MediaPlaylistTagName::kMinValue) ==
              ToTagName(MultivariantPlaylistTagName::kMaxValue) + 1);

// Ensure tag name sets are non-overlapping
static_assert(are_disjoint<CommonTagName, MultivariantPlaylistTagName>());
static_assert(are_disjoint<CommonTagName, MediaPlaylistTagName>());
static_assert(
    are_disjoint<MultivariantPlaylistTagName, MediaPlaylistTagName>());

template <typename T>
constexpr std::pair<std::string_view, TagName> TagNameEntry(
    std::string_view name_str,
    T name) {
  return std::make_pair(name_str, ToTagName(name));
}

// Mapping of tag names to their constants. This must remain sorted by the
// string value.
constexpr auto kTagNames = base::MakeFixedFlatMap({
    TagNameEntry("EXT-X-BITRATE", MediaPlaylistTagName::kXBitrate),
    TagNameEntry("EXT-X-BYTERANGE", MediaPlaylistTagName::kXByteRange),
    TagNameEntry("EXT-X-CONTENT-STEERING",
                 MultivariantPlaylistTagName::kXContentSteering),
    TagNameEntry("EXT-X-DATERANGE", MediaPlaylistTagName::kXDateRange),
    TagNameEntry("EXT-X-DEFINE", CommonTagName::kXDefine),
    TagNameEntry("EXT-X-DISCONTINUITY", MediaPlaylistTagName::kXDiscontinuity),
    TagNameEntry("EXT-X-DISCONTINUITY-SEQUENCE",
                 MediaPlaylistTagName::kXDiscontinuitySequence),
    TagNameEntry("EXT-X-ENDLIST", MediaPlaylistTagName::kXEndList),
    TagNameEntry("EXT-X-GAP", MediaPlaylistTagName::kXGap),
    TagNameEntry("EXT-X-I-FRAME-STREAM-INF",
                 MultivariantPlaylistTagName::kXIFrameStreamInf),
    TagNameEntry("EXT-X-I-FRAMES-ONLY", MediaPlaylistTagName::kXIFramesOnly),
    TagNameEntry("EXT-X-INDEPENDENT-SEGMENTS",
                 CommonTagName::kXIndependentSegments),
    TagNameEntry("EXT-X-KEY", MediaPlaylistTagName::kXKey),
    TagNameEntry("EXT-X-MAP", MediaPlaylistTagName::kXMap),
    TagNameEntry("EXT-X-MEDIA", MultivariantPlaylistTagName::kXMedia),
    TagNameEntry("EXT-X-MEDIA-SEQUENCE", MediaPlaylistTagName::kXMediaSequence),
    TagNameEntry("EXT-X-PART", MediaPlaylistTagName::kXPart),
    TagNameEntry("EXT-X-PART-INF", MediaPlaylistTagName::kXPartInf),
    TagNameEntry("EXT-X-PLAYLIST-TYPE", MediaPlaylistTagName::kXPlaylistType),
    TagNameEntry("EXT-X-PRELOAD-HINT", MediaPlaylistTagName::kXPreloadHint),
    TagNameEntry("EXT-X-PROGRAM-DATE-TIME",
                 MediaPlaylistTagName::kXProgramDateTime),
    TagNameEntry("EXT-X-RENDITION-REPORT",
                 MediaPlaylistTagName::kXRenditionReport),
    TagNameEntry("EXT-X-SERVER-CONTROL", MediaPlaylistTagName::kXServerControl),
    TagNameEntry("EXT-X-SESSION-DATA",
                 MultivariantPlaylistTagName::kXSessionData),
    TagNameEntry("EXT-X-SESSION-KEY",
                 MultivariantPlaylistTagName::kXSessionKey),
    TagNameEntry("EXT-X-SKIP", MediaPlaylistTagName::kXSkip),
    TagNameEntry("EXT-X-START", CommonTagName::kXStart),
    TagNameEntry("EXT-X-STREAM-INF", MultivariantPlaylistTagName::kXStreamInf),
    TagNameEntry("EXT-X-TARGETDURATION",
                 MediaPlaylistTagName::kXTargetDuration),
    TagNameEntry("EXT-X-VERSION", CommonTagName::kXVersion),
    TagNameEntry("EXTINF", MediaPlaylistTagName::kInf),
    TagNameEntry("EXTM3U", CommonTagName::kM3u),
});

// `kTagNames` must cover the entire [kMinTagName, kMaxTagName] range.
static_assert(kTagNames.size() ==
              static_cast<size_t>(kMaxTagName - kMinTagName + 1));

}  // namespace

TagKind GetTagKind(TagName name) {
  if (name <= ToTagName(CommonTagName::kMaxValue)) {
    return TagKind::kCommonTag;
  }
  if (name <= ToTagName(MultivariantPlaylistTagName::kMaxValue)) {
    return TagKind::kMultivariantPlaylistTag;
  }
  if (name <= ToTagName(MediaPlaylistTagName::kMaxValue)) {
    return TagKind::kMediaPlaylistTag;
  }

  NOTREACHED();
}

std::optional<TagName> ParseTagName(std::string_view name) {
  // Search for the tag name
  const auto match = kTagNames.find(name);
  if (match == kTagNames.end()) {
    return std::nullopt;
  }

  return match->second;
}

std::string_view TagNameToString(TagName name) {
  for (auto entry : kTagNames) {
    if (name == entry.second) {
      return entry.first;
    }
  }

  NOTREACHED();
}

}  // namespace media::hls
