// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
#define MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/strings/string_piece_forward.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media {

class HlsDemuxer;

// Interface which can provide HlsDemuxer with data, respecting byterange
// boundaries.
class MEDIA_EXPORT HlsDataSource {
 public:
  enum class ReadStatusCodes : StatusCodeType {
    kError,
    kAborted,
  };
  struct ReadStatusTraits {
    using Codes = ReadStatusCodes;
    static constexpr StatusGroupType Group() {
      return "HlsDataSource::ReadStatus";
    }
  };
  using ReadStatus = TypedStatus<ReadStatusTraits>;
  using ReadCb = base::OnceCallback<void(ReadStatus::Or<size_t>)>;

  explicit HlsDataSource(absl::optional<uint64_t> size) : size_(size) {}
  virtual ~HlsDataSource();

  // Issues a read to the underlying data source, writing the results to
  // `buffer` and running `callback` once that's completed. `pos` is a 0-based
  // starting byte to read from, which will be mapped to the correct byterange
  // within the underlying data source. `size` is the maximum number of bytes
  // that may be written into `buffer`, and must be greater than 0. If an error
  // occurred, `callback` will be run with the error status. Otherwise, it's run
  // with the number of bytes read into `buffer`. If the number of bytes read is
  // 0, there is no more data left in the data source.
  virtual void Read(uint64_t pos,
                    size_t size,
                    uint8_t* buffer,
                    ReadCb callback) = 0;

  // Returns the MIME type of the underlying data source.
  virtual base::StringPiece GetMimeType() const = 0;

  // Returns the size of the underlying data source. If the size is unknown,
  // returns `absl::nullopt`.
  absl::optional<size_t> GetSize() const { return size_; }

 protected:
  const absl::optional<size_t> size_;
};

// Interface which can provide the HlsDemuxer with data sources, given a URI and
// an optional byterange. This interface should be used via
// `base::SequenceBound` to proxy requests across the media thread and the main
// thread.
class MEDIA_EXPORT HlsDataSourceProvider {
 public:
  virtual ~HlsDataSourceProvider();

  // Sets the owning HlsDemuxer for this HlsDataSourceProvider.
  // This may only be called once.
  virtual void SetOwner(HlsDemuxer*) = 0;

  // API allowing an HlsDemuxer to make requests for external data.
  using RequestCb = base::OnceCallback<void(std::unique_ptr<HlsDataSource>)>;
  virtual void RequestDataSource(GURL uri,
                                 absl::optional<hls::types::ByteRange> range,
                                 RequestCb) = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
