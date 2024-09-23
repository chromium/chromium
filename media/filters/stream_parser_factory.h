// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_
#define MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/mime_util.h"
#include "media/base/stream_parser.h"

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

// These entries represent mime types which can be created with a "relaxed"
// parser, which is one which does not verify expected codecs, and instead
// discovered all available ones.
enum class RelaxedParserSupportedType {
  kMP2T,
  kMP4,
  kAAC,
};

class MEDIA_EXPORT StreamParserFactory {
 public:
  // Types of parsers supported for non-strict parsing.

  // Checks to see if the specified |type| and |codecs| list are supported.
  // Returns one of the following SupportsType values:
  // kNotSupported indicates definitive lack of support.
  // kSupported indicates the mime type is supported, any non-empty codecs
  // requirement is met for the mime type, and all of the passed codecs are
  // supported for the mime type.
  // kMaybeSupported indicates the mime type is supported, but the mime type
  // requires a codecs parameter that is missing.
  static SupportsType IsTypeSupported(std::string_view type,
                                      base::span<const std::string> codecs);

  // Creates a new StreamParser object if the specified |type| and |codecs| list
  // are supported. |media_log| can be used to report errors if there is
  // something wrong with |type| or the codec IDs in |codecs|.
  // Returns a new StreamParser object if |type| and all codecs listed in
  //   |codecs| are supported.
  // Returns NULL otherwise.
  // The |audio_config| and |video_config| overloads behave similarly, except
  // the caller must provide a valid, supported decoder config; those overloads'
  // usage indicates that we intend to buffer WebCodecs encoded audio or video
  // chunks with this parser's ProcessChunks() method. Note that
  // these overloads do not check support, unlike the |type| and |codecs|
  // version. Support checking for WebCodecs-originated decoder configs could be
  // async, and should be done by the caller if necessary as part of the decoder
  // config creation rather than relying upon parser creation to do this
  // potentially expensive step (this step is typically done in a synchronous
  // API call by the web app, such as addSourceBuffer().) Like |type| and
  // |codecs| versions, basic IsValidConfig() is done on configs emitted from
  // the parser. Failing that catching an unsupported config, eventual pipeline
  // error should occur for unsupported or invalid decoder configs during
  // attempted decode.
  static std::unique_ptr<StreamParser> Create(
      std::string_view type,
      base::span<const std::string> codecs,
      MediaLog* media_log);
  static std::unique_ptr<StreamParser> Create(
      std::unique_ptr<AudioDecoderConfig> audio_config);
  static std::unique_ptr<StreamParser> Create(
      std::unique_ptr<VideoDecoderConfig> video_config);

  // Creates a parser used for determining which codecs are present in a media
  // file. This parser won't log to media log because failure to parse is not
  // itself an error in hls media playback. This function may return null if
  // the mime type isn't supported.

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  static std::unique_ptr<StreamParser> CreateRelaxedParser(
      RelaxedParserSupportedType mime);
#endif
};

}  // namespace media

#endif  // MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_
