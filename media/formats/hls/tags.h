// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAGS_H_
#define MEDIA_FORMATS_HLS_TAGS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tag_name.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"

namespace media::hls {

class TagItem;

// All currently implemented HLS tag types.
// For organization, these appear in the same order as in `tag_name.h`.

// Represents the contents of the #EXTM3U tag
struct MEDIA_EXPORT M3uTag {
  static constexpr auto kName = CommonTagName::kM3u;
  static ParseStatus::Or<M3uTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-DEFINE tag
struct MEDIA_EXPORT XDefineTag {
  static constexpr auto kName = CommonTagName::kXDefine;
  static ParseStatus::Or<XDefineTag> Parse(TagItem);

  // Constructs an XDefineTag representing a variable definition.
  static XDefineTag CreateDefinition(types::VariableName name,
                                     std::string_view value);

  // Constructs an XDefineTag representing an imported variable definition.
  static XDefineTag CreateImport(types::VariableName name);

  // The name of the variable being defined.
  types::VariableName name;

  // The value of the variable. If this is `nullopt`, then the value
  // is being IMPORT-ed and must be defined in the parent playlist.
  std::optional<std::string_view> value;
};

// Represents the contents of the #EXT-X-INDEPENDENT-SEGMENTS tag
struct MEDIA_EXPORT XIndependentSegmentsTag {
  static constexpr auto kName = CommonTagName::kXIndependentSegments;
  static ParseStatus::Or<XIndependentSegmentsTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-VERSION tag
struct MEDIA_EXPORT XVersionTag {
  static constexpr auto kName = CommonTagName::kXVersion;
  static ParseStatus::Or<XVersionTag> Parse(TagItem);

  types::DecimalInteger version;
};

enum class MediaType {
  kAudio,
  kVideo,
  kSubtitles,
  kClosedCaptions,
};

// Represents the contents of the #EXT-X-MEDIA tag
struct MEDIA_EXPORT XMediaTag {
  static constexpr auto kName = MultivariantPlaylistTagName::kXMedia;
  static ParseStatus::Or<XMediaTag> Parse(
      TagItem,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer);

  struct CtorArgs;
  explicit XMediaTag(CtorArgs);
  ~XMediaTag();
  XMediaTag(const XMediaTag&);
  XMediaTag(XMediaTag&&);
  XMediaTag& operator=(const XMediaTag&);
  XMediaTag& operator=(XMediaTag&&);

  // The type of media this tag represents.
  MediaType type;

  // The URI of the media playlist for this rendition. This is required if
  // `type` is `kSubtitles`, optional if the type is `kAudio` or `kVideo`,
  // and absent in the case of `kClosedCaptions`. The absence of this value for
  // `kVideo` indicates that the media data is included in the primary rendition
  // of any associated variants, and the absence of this value for `kAudio`
  // indicates that the media data is included in every video rendition of any
  // associated variants.
  std::optional<ResolvedSourceString> uri;

  // For renditions with type `kClosedCaptions`, this specifies a rendition
  // within the segments of an associated media playlist. For all other types
  // this will be empty.
  std::optional<types::InstreamId> instream_id;

  // Indicates the group to which this rendition belongs.
  ResolvedSourceString group_id;

  // This identifies the primary language used in the rendition.
  std::optional<ResolvedSourceString> language;

  // This identifies a language that is associated with the rendition, in a
  // different role than `language`.
  std::optional<ResolvedSourceString> associated_language;

  // A human-readable description of this rendition.
  ResolvedSourceString name;

  // A stable identifier for the URI of this rendition within a multivariant
  // playlist. All renditions with the same URI SHOULD use the same
  // stable-rendition-id.
  std::optional<types::StableId> stable_rendition_id;

  // Indicates whether the client should play this rendition in the absence of
  // information from the user indicating a different choice.
  bool is_default = false;

  // Indicates that the client may choose to play this rendition in the absence
  // of an explicit user preference.
  bool autoselect = false;

  // Indicates that this rendition contains content that is considered essential
  // to play. This will always be false if the type is not `kSubtitles`.
  bool forced = false;

  // A sequence of media characteristic tags, indicating a characteristic of the
  // rendition.
  std::vector<std::string> characteristics;

  // Contains channel information for this rendition. The only type with channel
  // information currently defined is `kAudio`, others are ignored for
  // forward-compatibility.
  std::optional<types::AudioChannels> channels;
};

// Represents the contents of the #EXT-X-STREAM-INF tag
struct MEDIA_EXPORT XStreamInfTag {
  static constexpr auto kName = MultivariantPlaylistTagName::kXStreamInf;
  static ParseStatus::Or<XStreamInfTag> Parse(
      TagItem,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer);

  XStreamInfTag();
  ~XStreamInfTag();
  XStreamInfTag(const XStreamInfTag&);
  XStreamInfTag(XStreamInfTag&&);
  XStreamInfTag& operator=(const XStreamInfTag&);
  XStreamInfTag& operator=(XStreamInfTag&&);

  // The peak segment bitrate of the stream this tag applies to, in bits per
  // second.
  types::DecimalInteger bandwidth = 0;

  // The average segment bitrate of the stream this tag applies to, in bits per
  // second.
  std::optional<types::DecimalInteger> average_bandwidth;

  // An abstract, relative measure of the quality-of-experience of the stream
  // this tag applies to. The determination of this number is up to the playlist
  // author, however higher scores must indicate a better playback experience.
  std::optional<types::DecimalFloatingPoint> score;

  // A list of formats, where each format specifies a media
  // sample type that is present is one or more renditions of the variant stream
  // this tag applies to. According to the spec this *should* be present on
  // every instance of this tag, but in practice it's not. It's represented as
  // optional here so that the caller may decide how they wish to handle its
  // absence.
  std::optional<std::vector<std::string>> codecs;

  // The optimal pixel resolution at which to display all video in this variant
  // stream.
  std::optional<types::DecimalResolution> resolution;

  // This describes the maximum framerate for all video in this variant stream.
  std::optional<types::DecimalFloatingPoint> frame_rate;

  // The id of an audio rendition group that should be used when playing this
  // variant.
  std::optional<ResolvedSourceString> audio;

  // The id of a video rendition group that should be used when playing this
  // variant.
  std::optional<ResolvedSourceString> video;
};

// Represents the contents of the #EXTINF tag
struct MEDIA_EXPORT InfTag {
  static constexpr auto kName = MediaPlaylistTagName::kInf;
  static ParseStatus::Or<InfTag> Parse(TagItem);

  // Target duration of the media segment.
  base::TimeDelta duration;

  // Human-readable title of the media segment.
  SourceString title;
};

// Represents the contents of the #EXT-X-BITRATE tag.
struct MEDIA_EXPORT XBitrateTag {
  static constexpr auto kName = MediaPlaylistTagName::kXBitrate;
  static ParseStatus::Or<XBitrateTag> Parse(TagItem);

  // The approximate bitrate of the following media segments, (except those that
  // have the EXT-X-BYTERANGE tag) expressed in kilobits per second. The value
  // must be within +-10% of the actual segment bitrate.
  types::DecimalInteger bitrate;
};

// Represents the contents of the #EXT-X-BYTERANGE tag.
struct MEDIA_EXPORT XByteRangeTag {
  static constexpr auto kName = MediaPlaylistTagName::kXByteRange;
  static ParseStatus::Or<XByteRangeTag> Parse(TagItem);

  types::parsing::ByteRangeExpression range;
};

// Represents the contents of the #EXT-X-DISCONTINUITY tag
struct MEDIA_EXPORT XDiscontinuityTag {
  static constexpr auto kName = MediaPlaylistTagName::kXDiscontinuity;
  static ParseStatus::Or<XDiscontinuityTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-DISCONTINUITY-SEQUENCE tag.
struct MEDIA_EXPORT XDiscontinuitySequenceTag {
  static constexpr auto kName = MediaPlaylistTagName::kXDiscontinuitySequence;
  static ParseStatus::Or<XDiscontinuitySequenceTag> Parse(TagItem);

  // Indicates the discontinuity sequence number to assign to the first media
  // segment in this playlist. These numbers are useful for synchronizing
  // between variant stream timelines.
  types::DecimalInteger number;
};

// Represents the contents of the #EXT-X-ENDLIST tag
struct MEDIA_EXPORT XEndListTag {
  static constexpr auto kName = MediaPlaylistTagName::kXEndList;
  static ParseStatus::Or<XEndListTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-GAP tag
struct MEDIA_EXPORT XGapTag {
  static constexpr auto kName = MediaPlaylistTagName::kXGap;
  static ParseStatus::Or<XGapTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-I-FRAMES-ONLY tag
struct MEDIA_EXPORT XIFramesOnlyTag {
  static constexpr auto kName = MediaPlaylistTagName::kXIFramesOnly;
  static ParseStatus::Or<XIFramesOnlyTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-MAP tag.
struct MEDIA_EXPORT XMapTag {
  static constexpr auto kName = MediaPlaylistTagName::kXMap;
  static ParseStatus::Or<XMapTag> Parse(
      TagItem,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer);

  // The URI of the resource containing the media initialization section.
  ResolvedSourceString uri;

  // This specifies a byte range into the resource containing the media
  // initialization section.
  std::optional<types::parsing::ByteRangeExpression> byte_range;
};

// Represents the contents of the #EXT-X-MEDIA-SEQUENCE tag.
struct MEDIA_EXPORT XMediaSequenceTag {
  static constexpr auto kName = MediaPlaylistTagName::kXMediaSequence;
  static ParseStatus::Or<XMediaSequenceTag> Parse(TagItem);

  // Indicates the media sequence number to assign to the first media segment in
  // this playlist. These numbers are useful for validating the same media
  // playlist across reloads, but not for synchronizing media segments between
  // playlists.
  types::DecimalInteger number;
};

// Represents the contents of the #EXT-X-PART tag.
struct MEDIA_EXPORT XPartTag {
  static constexpr auto kName = MediaPlaylistTagName::kXPart;
  static ParseStatus::Or<XPartTag> Parse(
      TagItem,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer);

  // The resource URI for the partial segment.
  ResolvedSourceString uri;

  // The duration of the partial segment.
  base::TimeDelta duration;

  // If this partial segment is a subrange of its resource, this defines the
  // subrange.
  std::optional<types::parsing::ByteRangeExpression> byte_range;

  // Whether the partial segment contains an independent frame.
  bool independent = false;

  // Whether this partial segment is unavailable, similar to EXT-X-GAP for media
  // segments.
  bool gap = false;
};

// Represents the contents of the #EXT-PART-INF tag.
struct MEDIA_EXPORT XPartInfTag {
  static constexpr auto kName = MediaPlaylistTagName::kXPartInf;
  static ParseStatus::Or<XPartInfTag> Parse(TagItem);

  // This value indicates the target duration for partial media segments.
  base::TimeDelta target_duration;
};

enum class PlaylistType {
  // Indicates that this playlist may have segments appended upon reloading
  // (until the #EXT-X-ENDLIST tag appears), but segments will not be removed.
  kEvent,

  // Indicates that this playlist is static, and will not have segments appended
  // or removed.
  kVOD,
};

// Represents the contents of the #EXT-X-PLAYLIST-TYPE tag
struct MEDIA_EXPORT XPlaylistTypeTag {
  static constexpr auto kName = MediaPlaylistTagName::kXPlaylistType;
  static ParseStatus::Or<XPlaylistTypeTag> Parse(TagItem);

  PlaylistType type;
};

// Represents the contents of the #EXT-X-SERVER-CONTROL tag.
struct MEDIA_EXPORT XServerControlTag {
  static constexpr auto kName = MediaPlaylistTagName::kXServerControl;
  static ParseStatus::Or<XServerControlTag> Parse(TagItem);

  // This value (given by the 'CAN-SKIP-UNTIL' attribute) represents the
  // distance from the last media segment that the server is able
  // to produce a playlist delta update.
  std::optional<base::TimeDelta> skip_boundary;

  // This indicates whether the server supports skipping EXT-X-DATERANGE tags
  // older than the skip boundary when producing playlist delta updates.
  bool can_skip_dateranges = false;

  // This indicates the distance from the end of the playlist
  // at which clients should begin playback. This MUST be at least three times
  // the playlist's target duration.
  std::optional<base::TimeDelta> hold_back;

  // This indicates the distance from the end of the playlist
  // at which clients should begin playback when playing in low-latency mode.
  // This value MUST be at least twice the playlist's partial segment target
  // duration, and SHOULD be at least three times that.
  std::optional<base::TimeDelta> part_hold_back;

  // This indicates whether the server supports blocking playlist reloads.
  bool can_block_reload = false;
};

// Represents the contents of the #EXT-X-TARGETDURATION tag.
struct MEDIA_EXPORT XTargetDurationTag {
  static constexpr auto kName = MediaPlaylistTagName::kXTargetDuration;
  static ParseStatus::Or<XTargetDurationTag> Parse(TagItem);

  // The upper bound on the duration of all media segments in the
  // media playlist. The EXTINF duration of each Media Segment in a Playlist
  // file, when rounded to the nearest integer, MUST be less than or equal to
  // this duration.
  base::TimeDelta duration;
};

struct MEDIA_EXPORT XSkipTag {
  static constexpr auto kName = MediaPlaylistTagName::kXSkip;
  static ParseStatus::Or<XSkipTag> Parse(
      TagItem,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer);

  XSkipTag();
  ~XSkipTag();
  XSkipTag(const XSkipTag&);
  XSkipTag(XSkipTag&&);

  // The value is a decimal integer specifying the number of Media Segments
  // replaced by the EXT-X-SKIP tag. This attribute is REQUIRED.
  types::DecimalInteger skipped_segments;

  // The value is a quoted string consisting of a tab delimited list of
  // EXT-X-DATERANGE IDs that have been removed from the playlist recently.
  // This attribute is REQUIRED if the client requested an update that skips
  // EXT-X-DATERANGE tags. The quoted string MAY be empty.
  std::optional<std::vector<std::string>> recently_removed_dateranges =
      std::nullopt;
};

// A server MAY omit adding an attribute to an EXT-X-RENDITION-REPORT tag - even
// a mandatory attribute - if its value is the same as that of the Rendition
// Report of the Media Playlist to which the EXT-X-RENDITION-REPORT tag is being
// added. Doing so reduces the size of the Rendition Report.
struct MEDIA_EXPORT XRenditionReportTag {
  static constexpr auto kName = MediaPlaylistTagName::kXRenditionReport;
  static ParseStatus::Or<XRenditionReportTag> Parse(
      TagItem,
      const VariableDictionary&,
      VariableDictionary::SubstitutionBuffer&);

  // Url for the media playlist of the specified rendition. It MUST be relative
  // to the URI of the media playlist containing the EXT-X-RENDITION-REPORT tag.
  std::optional<ResolvedSourceString> uri;

  // The valid specifying the last media segment's sequence number in the
  // rendition. if the rendition contains partial segments, then this value is
  // the last partial segments media sequence number.
  std::optional<types::DecimalInteger> last_msn;

  // The value is a decimal-integer that indicates the Part Index of the last
  // Partial Segment currently in the specified Rendition whose Media Sequence
  // Number is equal to the LAST-MSN attribute value. This attribute is REQUIRED
  // if the Rendition contains a Partial Segment.
  std::optional<types::DecimalInteger> last_part;
};

// The EXT-X-PROGRAM-DATE-TIME tag associates the first sample of a Media
// Segment with an absolute date and/or time. It applies only to the next
// Media Segment.
struct MEDIA_EXPORT XProgramDateTimeTag {
  static constexpr auto kName = MediaPlaylistTagName::kXProgramDateTime;
  static ParseStatus::Or<XProgramDateTimeTag> Parse(TagItem);

  base::Time time;
};

enum class XKeyTagMethod {
  kNone,

  // An encryption method of AES-128 signals that Media Segments are
  // completely encrypted using the Advanced Encryption Standard (AES)
  // [AES_128] with a 128-bit key, Cipher Block Chaining (CBC), and
  // Public-Key Cryptography Standards #7 (PKCS7) padding [RFC5652].
  // CBC is restarted on each segment boundary, using either the
  // Initialization Vector (IV) attribute value or the Media Sequence
  // Number as the IV. Sometimes AES-256 is used as well.
  kAES128,
  kAES256,

  // With Sample Encryption, only media sample data - such as audio
  // packets or video frames - is encrypted. The rest of the Media
  // Segment is unencrypted. Sample Encryption allows parts of the
  // Segment to be processed without (or before) decrypting the media
  // itself.
  kSampleAES,

  // An encryption method of SAMPLE-AES-CTR is similar to SAMPLE-AES.
  // However, fMP4 Media Segments are encrypted using the 'cenc' scheme
  // of Common Encryption [COMMON_ENC]. Encryption of other Media
  // Segment formats is not defined for SAMPLE-AES-CTR.  The IV
  // attribute MUST NOT be present
  kSampleAESCTR,
  kSampleAESCENC,

  // TODO: document why and when this is used. This shows up in some sample
  // manifests I've seen.
  kISO230017,
};

enum class XKeyTagKeyFormat {
  kIdentity,
  kClearKey,
  kWidevine,
  kUnsupported,
};

struct MEDIA_EXPORT XKeyTag {
  using IVHex = types::parsing::HexRepr<128>;

  static constexpr auto kName = MediaPlaylistTagName::kXKey;
  static constexpr bool kAllowEmptyMethod = true;
  static ParseStatus::Or<XKeyTag> Parse(
      TagItem,
      const VariableDictionary&,
      VariableDictionary::SubstitutionBuffer&);

  // If the encryption method is NONE, other attributes MUST NOT be
  // present.
  XKeyTagMethod method;

  // The value is a quoted-string containing a URI that specifies how
  // to obtain the key. This attribute is REQUIRED unless the METHOD
  // is NONE.
  std::optional<ResolvedSourceString> uri;

  // The value is a hexadecimal-sequence that specifies a 128-bit
  // unsigned integer Initialization Vector to be used with the key.
  std::optional<IVHex::Container> iv;

  // The value is a quoted-string that specifies how the key is
  // represented in the resource identified by the URI; see Section 5
  // for more detail. This attribute is OPTIONAL; its absence
  // indicates an implicit value of "identity". Use of the KEYFORMAT
  // attribute REQUIRES a compatibility version number of 5 or greater.
  XKeyTagKeyFormat keyformat;

  // The value is a quoted-string containing one or more positive
  // integers separated by the "/" character (for example, "1", "1/2",
  // or "1/2/5"). If more than one version of a particular KEYFORMAT
  // is defined, this attribute can be used to indicate which
  // version(s) this instance complies with. This attribute is
  // OPTIONAL; if it is not present, its value is considered to be "1".
  // Use of the KEYFORMATVERSIONS attribute REQUIRES a compatibility
  // version number of 5 or greater.
  std::optional<ResolvedSourceString> keyformat_versions;
};

struct MEDIA_EXPORT XSessionKeyTag {
  static constexpr auto kName = MultivariantPlaylistTagName::kXSessionKey;
  static constexpr bool kAllowEmptyMethod = false;
  static ParseStatus::Or<XSessionKeyTag> Parse(
      TagItem,
      const VariableDictionary&,
      VariableDictionary::SubstitutionBuffer&);

  // the METHOD attribute MUST NOT be NONE.
  XKeyTagMethod method;

  // If an EXT-X-SESSION-KEY is used, the values of the METHOD, KEYFORMAT, and
  // KEYFORMATVERSIONS attributes MUST match any EXT-X-KEY with the same URI
  // value. These fields match the corresponding ones in XKeyTag.
  ResolvedSourceString uri;
  std::optional<XKeyTag::IVHex::Container> iv;
  XKeyTagKeyFormat keyformat;
  std::optional<ResolvedSourceString> keyformat_versions;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_TAGS_H_
