// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_
#define MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_

#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_box_writer.h"

// The file contains the box writer of `moov` and its children.
namespace media {

DECLARE_MP4_BOX_WRITER_CLASS(Mp4FileTypeBoxWriter,
                             mp4::writable_boxes::FileType);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieBoxWriter, mp4::writable_boxes::Movie);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieHeaderBoxWriter,
                             mp4::writable_boxes::MovieHeader);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieExtendsBoxWriter,
                             mp4::writable_boxes::MovieExtends);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieTrackExtendsBoxWriter,
                             mp4::writable_boxes::TrackExtends);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieTrackBoxWriter,
                             mp4::writable_boxes::Track);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieTrackHeaderBoxWriter,
                             mp4::writable_boxes::TrackHeader);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieMediaBoxWriter,
                             mp4::writable_boxes::Media);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieMediaHeaderBoxWriter,
                             mp4::writable_boxes::MediaHeader);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieMediaHandlerBoxWriter,
                             mp4::writable_boxes::MediaHandler);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieMediaInformationBoxWriter,
                             mp4::writable_boxes::MediaInformation);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieSampleTableBoxWriter,
                             mp4::writable_boxes::SampleTable);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieSampleDescriptionBoxWriter,
                             mp4::writable_boxes::SampleDescription);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieVideoHeaderBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieSoundHeaderBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieDataInformationBoxWriter,
                             mp4::writable_boxes::DataInformation);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieDataReferenceBoxWriter,
                             mp4::writable_boxes::DataReference);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieDataUrlEntryBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieSampleToChunkBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieDecodingTimeToSampleBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieSampleSizeBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MovieSampleChunkOffsetBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieAudioSampleEntryBoxWriter,
                             mp4::writable_boxes::AudioSampleEntry);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieVisualSampleEntryBoxWriter,
                             mp4::writable_boxes::VisualSampleEntry);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieOpusSpecificBoxWriter,
                             mp4::writable_boxes::OpusSpecificBox);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieVPCodecConfigurationBoxWriter,
                             mp4::writable_boxes::VPCodecConfiguration);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieAV1CodecConfigurationBoxWriter,
                             mp4::writable_boxes::AV1CodecConfiguration);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieAVCDecoderConfigurationBoxWriter,
                             mp4::writable_boxes::AVCDecoderConfiguration);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieElementaryStreamDescriptorBoxWriter,
                             mp4::writable_boxes::ElementaryStreamDescriptor);
#endif

DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4MoviePixelAspectRatioBoxBoxWriter);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieBitRateBoxWriter,
                             mp4::writable_boxes::BitRate);
}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_
