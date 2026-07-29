// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_picture.h"

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace media {
AV1Picture::AV1Picture() = default;
AV1Picture::~AV1Picture() = default;

scoped_refptr<AV1Picture> AV1Picture::Duplicate() {
  scoped_refptr<AV1Picture> dup_pic = CreateDuplicate();
  if (!dup_pic)
    return nullptr;
  dup_pic->CopyCommonFieldsFrom(*this);

  // Copy members of AV1Picture.
  dup_pic->frame_header = frame_header;
  return dup_pic;
}

scoped_refptr<AV1Picture> AV1Picture::CreateDuplicate() {
  return base::MakeRefCounted<AV1Picture>();
}

}  // namespace media
