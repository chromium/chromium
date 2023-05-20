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

}  // namespace media::mp4::writable_boxes
