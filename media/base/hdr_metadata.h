// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_HDR_METADATA_H_
#define MEDIA_BASE_HDR_METADATA_H_

#include "ui/gl/hdr_metadata.h"

namespace media {

// TODO(crbug.com/1122910):Delete this file and switch all instances to the gl
// versions.
using HDRMetadata = gl::HDRMetadata;
using MasteringMetadata = gl::MasteringMetadata;
using HdrMetadataType = gl::HdrMetadataType;

}  // namespace media

#endif  // MEDIA_BASE_HDR_METADATA_H_
