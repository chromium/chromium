// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include "media/filters/hls_data_source_provider.h"

namespace media {

class FakeHlsDataSource : public HlsDataSource {
 public:
  FakeHlsDataSource(std::vector<uint8_t> data);
  ~FakeHlsDataSource() override;
  void Read(uint64_t pos,
            size_t size,
            uint8_t* buf,
            HlsDataSource::ReadCb cb) override;
  base::StringPiece GetMimeType() const override;

 protected:
  std::vector<uint8_t> data_;
};

class FileHlsDataSource : public FakeHlsDataSource {
 public:
  FileHlsDataSource(const std::string& filename);
};

class StringHlsDataSource : public FakeHlsDataSource {
 public:
  StringHlsDataSource(base::StringPiece content);
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
