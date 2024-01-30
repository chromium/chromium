// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_COLOUR_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_COLOUR_PARSER_H_

#include <optional>

#include "media/base/video_color_space.h"
#include "media/formats/webm/webm_parser.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

// WebM color information, containing HDR metadata:
// http://www.webmproject.org/docs/container/#Colour
struct MEDIA_EXPORT WebMColorMetadata {
  unsigned BitsPerChannel = 0;
  unsigned ChromaSubsamplingHorz = 0;
  unsigned ChromaSubsamplingVert = 0;
  unsigned CbSubsamplingHorz = 0;
  unsigned CbSubsamplingVert = 0;
  unsigned ChromaSitingHorz = 0;
  unsigned ChromaSitingVert = 0;

  VideoColorSpace color_space;

  std::optional<gfx::HDRMetadata> hdr_metadata;

  WebMColorMetadata();
  WebMColorMetadata(const WebMColorMetadata& rhs);
};

// Parser for WebM MasteringMetadata within Colour element:
// http://www.webmproject.org/docs/container/#MasteringMetadata
class WebMColorVolumeMetadataParser : public WebMParserClient {
 public:
  WebMColorVolumeMetadataParser();

  WebMColorVolumeMetadataParser(const WebMColorVolumeMetadataParser&) = delete;
  WebMColorVolumeMetadataParser& operator=(
      const WebMColorVolumeMetadataParser&) = delete;

  ~WebMColorVolumeMetadataParser() override;

  gfx::HdrMetadataSmpteSt2086 GetColorVolumeMetadata() const {
    return smpte_st_2086_;
  }

 private:
  // WebMParserClient implementation.
  bool OnFloat(int id, double val) override;

  gfx::HdrMetadataSmpteSt2086 smpte_st_2086_;
};

// Parser for WebM Colour element:
// http://www.webmproject.org/docs/container/#colour
class WebMColourParser : public WebMParserClient {
 public:
  WebMColourParser();

  WebMColourParser(const WebMColourParser&) = delete;
  WebMColourParser& operator=(const WebMColourParser&) = delete;

  ~WebMColourParser() override;

  void Reset();

  WebMColorMetadata GetWebMColorMetadata() const;

 private:
  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;

  int64_t matrix_coefficients_;
  int64_t bits_per_channel_;
  int64_t chroma_subsampling_horz_;
  int64_t chroma_subsampling_vert_;
  int64_t cb_subsampling_horz_;
  int64_t cb_subsampling_vert_;
  int64_t chroma_siting_horz_;
  int64_t chroma_siting_vert_;
  int64_t range_;
  int64_t transfer_characteristics_;
  int64_t primaries_;
  int64_t max_content_light_level_;
  int64_t max_frame_average_light_level_;

  WebMColorVolumeMetadataParser color_volume_metadata_parser_;
  bool color_volume_metadata_parsed_ = false;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_COLOUR_PARSER_H_
