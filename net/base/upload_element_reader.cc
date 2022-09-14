// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_element_reader.h"

namespace net {

const UploadBytesElementReader* UploadElementReader::AsBytesReader() const {
  return nullptr;
}

const UploadFileElementReader* UploadElementReader::AsFileReader() const {
  return nullptr;
}

bool UploadElementReader::IsInMemory() const {
  return false;
}

}  // namespace net
