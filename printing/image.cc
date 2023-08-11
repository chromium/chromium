// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include "base/hash/md5.h"
#include "printing/metafile.h"

namespace printing {

Image::Image(const Metafile& metafile) : row_length_(0) {
  LoadMetafile(metafile);
}

Image::Image(const Image& image) = default;

Image::~Image() = default;

std::string Image::checksum() const {
  base::MD5Digest digest;
  base::MD5Sum(&data_[0], data_.size(), &digest);
  return base::MD5DigestToBase16(digest);
}

}  // namespace printing
