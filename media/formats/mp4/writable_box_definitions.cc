// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/writable_box_definitions.h"

namespace media::mp4::writable_boxes {

Movie::Movie() = default;
Movie::~Movie() = default;

MovieExtends::MovieExtends() = default;
MovieExtends::~MovieExtends() = default;

MovieHeader::MovieHeader() = default;
MovieHeader::~MovieHeader() = default;

DataReference::DataReference() = default;
DataReference::~DataReference() = default;
DataReference::DataReference(const DataReference&) = default;
DataReference& DataReference::operator=(const DataReference&) = default;

SampleDescription::SampleDescription() = default;
SampleDescription::~SampleDescription() = default;
SampleDescription::SampleDescription(const SampleDescription&) = default;
SampleDescription& SampleDescription::operator=(const SampleDescription&) =
    default;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
ElementaryStreamDescriptor::ElementaryStreamDescriptor() = default;
ElementaryStreamDescriptor::~ElementaryStreamDescriptor() = default;
ElementaryStreamDescriptor::ElementaryStreamDescriptor(
    const ElementaryStreamDescriptor&) = default;
ElementaryStreamDescriptor& ElementaryStreamDescriptor::operator=(
    const ElementaryStreamDescriptor&) = default;
#endif

TrackFragmentRun::TrackFragmentRun() = default;
TrackFragmentRun::~TrackFragmentRun() = default;
TrackFragmentRun::TrackFragmentRun(const TrackFragmentRun&) = default;
TrackFragmentRun& TrackFragmentRun::operator=(const TrackFragmentRun&) =
    default;

MovieFragment::MovieFragment() = default;
MovieFragment::~MovieFragment() = default;
MovieFragment::MovieFragment(const MovieFragment&) = default;
MovieFragment& MovieFragment::operator=(const MovieFragment&) = default;

MediaData::MediaData() = default;
MediaData::~MediaData() = default;
MediaData::MediaData(const MediaData&) = default;
MediaData& MediaData::operator=(const MediaData&) = default;

}  // namespace media::mp4::writable_boxes
