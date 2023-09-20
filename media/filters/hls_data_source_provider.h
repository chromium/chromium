// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
#define MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/id_type.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media {

namespace {

// A small-ish size that it should probably be able to get most manifests in
// a single chunk. Chosen somewhat arbitrarily otherwise.
constexpr size_t kDefaultReadSize = 1024 * 16;

}  // namespace

// Interface which can provide data, respecting byterange boundaries.
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

  explicit HlsDataSource(absl::optional<size_t> size) : size_(size) {}
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

  // Aborts and stops the underlying multibuffer data source. After aborting,
  // All calls to `::Read` should respond with kAborted. Accessing previously
  // fetched data is ok.
  virtual void Stop() = 0;

  // Returns the size of the underlying data source. If the size is unknown,
  // returns `absl::nullopt`.
  absl::optional<size_t> GetSize() const { return size_; }

 protected:
  const absl::optional<size_t> size_;
};

// Interface which can provide data sources, given a URI and an optional
// byterange. This interface should be used via `base::SequenceBound` to proxy
// requests across the media thread and the main thread.
class MEDIA_EXPORT HlsDataSourceProvider {
 public:
  virtual ~HlsDataSourceProvider();
  using RequestCb = base::OnceCallback<void(std::unique_ptr<HlsDataSource>)>;
  virtual void RequestDataSource(GURL uri,
                                 absl::optional<hls::types::ByteRange> range,
                                 RequestCb) = 0;
};

// Forward Declare manager.
class HlsManifestDemuxerEngine;

// A buffer-owning wrapper for an HlsDataSource which can be instructed to
// read an entire data source, or to retrieve it in chunks.
class MEDIA_EXPORT HlsDataSourceStream {
 public:
  // The response to a stream read includes a raw pointer back to the stream
  // which allows accessing the data from a read as well as caching a partially
  // read stream handle for continued downloading.
  using StreamId = base::IdType32<HlsDataSourceStream>;

  HlsDataSourceStream(std::unique_ptr<HlsDataSource> data_source);
  ~HlsDataSourceStream();

  // Helpers for checking the internal state of the stream.
  bool CanReadMore() const;
  size_t BytesInBuffer() const;

  // Helpers for accessing the buffer.
  base::StringPiece AsStringPiece() const;
  const uint8_t* AsRawData() const;

  // Reset the internal buffer.
  void Flush();

  void ReadChunkForTesting(HlsDataSource::ReadCb cb,
                           size_t read_size = kDefaultReadSize);
  void ReadChunk(base::PassKey<HlsManifestDemuxerEngine>,
                 HlsDataSource::ReadCb cb);

  void UpdateBytes(size_t original_size, size_t bytes_read);

 private:
  // Read data in chunks.
  void ReadChunkInternal(HlsDataSource::ReadCb cb,
                         size_t read_size = kDefaultReadSize);

  // The data source to read from.
  std::unique_ptr<HlsDataSource> data_source_;

  // the buffer of data to read into.
  std::vector<uint8_t> buffer_;

  // The total number of bytes read. Not affected by |Flush|.
  size_t total_bytes_read_ = 0;

  base::WeakPtrFactory<HlsDataSourceStream> weak_factory_;
};

// A HlsDataSourceStreamManager must own all instances of HlsDataSourceStream
// while those streams have pending network requests, so that they can be
// canceled as part of deletion.
class MEDIA_EXPORT HlsDataSourceStreamManager {
 public:
  using ReadResult =
      HlsDataSource::ReadStatus::Or<std::unique_ptr<HlsDataSourceStream>>;
  using ReadCb = base::OnceCallback<void(ReadResult)>;

  virtual ~HlsDataSourceStreamManager() = 0;

  // `ReadCb` is bound and posted on to run on the thread where `ReadStream` is
  // called.
  virtual void ReadStream(std::unique_ptr<HlsDataSourceStream>, ReadCb) = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
