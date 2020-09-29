// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-forward.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
class FetchAPIDataElementDataView;
}  // namespace mojom
}  // namespace blink

namespace network {

// Represents part of an upload body. This could be either one of bytes, file or
// blob data.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElement {
 public:
  static const uint64_t kUnknownSize = std::numeric_limits<uint64_t>::max();

  DataElement();
  ~DataElement();

  DataElement(const DataElement&) = delete;
  void operator=(const DataElement&) = delete;
  DataElement(DataElement&& other);
  DataElement& operator=(DataElement&& other);

  mojom::DataElementType type() const { return type_; }
  const char* bytes() const {
    return reinterpret_cast<const char*>(buf_.data());
  }
  const base::FilePath& path() const { return path_; }
  const std::string& blob_uuid() const { return blob_uuid_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }
  const base::Time& expected_modification_time() const {
    return expected_modification_time_;
  }

  // Sets TYPE_BYTES data. This copies the given data into the element.
  void SetToBytes(const char* bytes, int bytes_len) {
    type_ = mojom::DataElementType::kBytes;
    buf_.assign(reinterpret_cast<const uint8_t*>(bytes),
                reinterpret_cast<const uint8_t*>(bytes + bytes_len));
    length_ = buf_.size();
  }

  // Sets TYPE_BYTES data. This moves the given data vector into the element.
  void SetToBytes(std::vector<uint8_t> bytes) {
    type_ = mojom::DataElementType::kBytes;
    buf_ = std::move(bytes);
    length_ = buf_.size();
  }

  // Sets TYPE_BYTES data, and clears the internal bytes buffer.
  // For use with AppendBytes.
  void SetToEmptyBytes() {
    type_ = mojom::DataElementType::kBytes;
    buf_.clear();
    length_ = 0;
  }

  // Copies and appends the given data into the element. SetToEmptyBytes or
  // SetToBytes must be called before this method.
  void AppendBytes(const char* bytes, int bytes_len) {
    DCHECK_EQ(type_, mojom::DataElementType::kBytes);
    DCHECK_NE(length_, std::numeric_limits<uint64_t>::max());
    buf_.insert(buf_.end(), reinterpret_cast<const uint8_t*>(bytes),
                reinterpret_cast<const uint8_t*>(bytes + bytes_len));
    length_ = buf_.size();
  }

  // Sets TYPE_BLOB data.
  void SetToBlob(const std::string& uuid) {
    SetToBlobRange(uuid, 0, std::numeric_limits<uint64_t>::max());
  }

  // Sets TYPE_FILE data with range.
  void SetToFilePathRange(const base::FilePath& path,
                          uint64_t offset,
                          uint64_t length,
                          const base::Time& expected_modification_time);

  // Sets TYPE_BLOB data with range.
  void SetToBlobRange(const std::string& blob_uuid,
                      uint64_t offset,
                      uint64_t length);

  // Sets TYPE_DATA_PIPE data. The data pipe consumer can safely wait for the
  // callback passed to Read() to be invoked before reading the request body.
  void SetToDataPipe(
      mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter);

  // Sets TYPE_CHUNKED_DATA_PIPE data. The data pipe consumer must not wait
  // for the callback passed to GetSize() to be invoked before reading the
  // request body, as the length may not be known until the entire body has been
  // sent. This method triggers a chunked upload, which not all servers may
  // support, so SetToDataPipe should be used instead, unless talking with a
  // server known to support chunked uploads.
  void SetToChunkedDataPipe(mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
                                chunked_data_pipe_getter);
  // Almost same as above except |chunked_data_pipe_getter| is read only once
  // and you must talk with a server supporting chunked upload.
  void SetToReadOnceStream(mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
                               chunked_data_pipe_getter);

  // Takes ownership of the DataPipeGetter, if this is of TYPE_DATA_PIPE.
  mojo::PendingRemote<mojom::DataPipeGetter> ReleaseDataPipeGetter();
  mojo::PendingRemote<mojom::DataPipeGetter> CloneDataPipeGetter() const;

  // Takes ownership of the DataPipeGetter, if this is of
  // TYPE_CHUNKED_DATA_PIPE.
  mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
  ReleaseChunkedDataPipeGetter();

 private:
  FRIEND_TEST_ALL_PREFIXES(BlobAsyncTransportStrategyTest, TestInvalidParams);
  friend void PrintTo(const DataElement& x, ::std::ostream* os);
  friend struct mojo::StructTraits<network::mojom::DataElementDataView,
                                   network::DataElement>;
  friend struct mojo::StructTraits<blink::mojom::FetchAPIDataElementDataView,
                                   network::DataElement>;
  mojom::DataElementType type_;
  // For TYPE_BYTES.
  std::vector<uint8_t> buf_;
  // For TYPE_FILE.
  base::FilePath path_;
  // For TYPE_BLOB.
  std::string blob_uuid_;
  // For TYPE_DATA_PIPE.
  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_;
  // For TYPE_CHUNKED_DATA_PIPE.
  mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter_;
  uint64_t offset_;
  uint64_t length_;
  base::Time expected_modification_time_;
};

COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool operator==(const DataElement& a, const DataElement& b);
COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool operator!=(const DataElement& a, const DataElement& b);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
