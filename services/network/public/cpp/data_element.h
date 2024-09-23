// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-forward.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-forward.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace network {

// Represents a part of a request body consisting of bytes.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElementBytes final {
 public:
  // Do NOT use this constructor outside of mojo deserialization context.
  DataElementBytes();

  explicit DataElementBytes(std::vector<uint8_t> bytes);
  DataElementBytes(const DataElementBytes&) = delete;
  DataElementBytes(DataElementBytes&& other);
  DataElementBytes& operator=(const DataElementBytes&) = delete;
  DataElementBytes& operator=(DataElementBytes&& other);
  ~DataElementBytes();

  const std::vector<uint8_t>& bytes() const { return bytes_; }

  std::string_view AsStringPiece() const {
    return std::string_view(reinterpret_cast<const char*>(bytes_.data()),
                            bytes_.size());
  }

  DataElementBytes Clone() const;

 private:
  std::vector<uint8_t> bytes_;
};

// Represents a part of a request body consisting of a data pipe. This is
// typically used for blobs.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElementDataPipe final {
 public:
  // Do NOT use this constructor outside of mojo deserialization context.
  DataElementDataPipe();

  explicit DataElementDataPipe(
      mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter);
  DataElementDataPipe(const DataElementDataPipe&) = delete;
  DataElementDataPipe(DataElementDataPipe&& other);
  DataElementDataPipe& operator=(const DataElementDataPipe&) = delete;
  DataElementDataPipe& operator=(DataElementDataPipe&& other);
  ~DataElementDataPipe();

  mojo::PendingRemote<mojom::DataPipeGetter> ReleaseDataPipeGetter();
  mojo::PendingRemote<mojom::DataPipeGetter> CloneDataPipeGetter() const;

  DataElementDataPipe Clone() const;

 private:
  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_;
};

// Represents a part of a request body consisting of a data pipe without a
// known size.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElementChunkedDataPipe final {
 public:
  using ReadOnlyOnce = base::StrongAlias<class ReadOnlyOnceTag, bool>;

  // Do NOT use this constructor outside of mojo deserialization context.
  DataElementChunkedDataPipe();

  DataElementChunkedDataPipe(
      mojo::PendingRemote<mojom::ChunkedDataPipeGetter> data_pipe_getter,
      ReadOnlyOnce read_only_once);
  DataElementChunkedDataPipe(const DataElementChunkedDataPipe&) = delete;
  DataElementChunkedDataPipe(DataElementChunkedDataPipe&& other);
  DataElementChunkedDataPipe& operator=(const DataElementChunkedDataPipe&) =
      delete;
  DataElementChunkedDataPipe& operator=(DataElementChunkedDataPipe&& other);
  ~DataElementChunkedDataPipe();

  const mojo::PendingRemote<mojom::ChunkedDataPipeGetter>&
  chunked_data_pipe_getter() const {
    return chunked_data_pipe_getter_;
  }
  mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
  ReleaseChunkedDataPipeGetter();

  ReadOnlyOnce read_only_once() const { return read_only_once_; }

 private:
  mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter_;
  ReadOnlyOnce read_only_once_;
};

// Represents a part of a request body consisting of (part of) a file.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElementFile final {
 public:
  // Do NOT use this constructor outside of mojo deserialization context.
  DataElementFile();

  DataElementFile(const base::FilePath& path,
                  uint64_t offset,
                  uint64_t length,
                  base::Time expected_modification_time);
  DataElementFile(const DataElementFile&);
  DataElementFile& operator=(const DataElementFile&);
  DataElementFile(DataElementFile&&);
  DataElementFile& operator=(DataElementFile&&);
  ~DataElementFile();

  const base::FilePath& path() const { return path_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }
  base::Time expected_modification_time() const {
    return expected_modification_time_;
  }

 private:
  base::FilePath path_;
  uint64_t offset_ = 0;
  uint64_t length_ = 0;
  base::Time expected_modification_time_;
};

// Represents part of an upload body. This is a union of various types defined
// above. See them for details.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) DataElement {
 private:
  using Variant = absl::variant<absl::monostate,
                                DataElementBytes,
                                DataElementDataPipe,
                                DataElementChunkedDataPipe,
                                DataElementFile>;

 public:
  using Tag = mojom::DataElementDataView::Tag;

  // Do NOT use this constructor outside of mojo deserialization context. A
  // DataElement created by this constructor should be considered as invalid,
  // and replaced with a valid value as soon as possible.
  DataElement();

  template <typename T,
            typename = std::enable_if_t<std::is_constructible_v<Variant, T>>>
  explicit DataElement(T&& t) : variant_(std::forward<T>(t)) {}
  DataElement(const DataElement&) = delete;
  DataElement& operator=(const DataElement&) = delete;
  DataElement(DataElement&& other);
  DataElement& operator=(DataElement&& other);
  ~DataElement();

  // Returns a cloned element. This is callable only when the type is not
  // `kChunkedDataPipe`.
  DataElement Clone() const;

  Tag type() const {
    switch (variant_.index()) {
      case 0:
        NOTREACHED_IN_MIGRATION();
        return Tag::kBytes;
      case 1:
        return Tag::kBytes;
      case 2:
        return Tag::kDataPipe;
      case 3:
        return Tag::kChunkedDataPipe;
      case 4:
        return Tag::kFile;
      default:
        NOTREACHED_IN_MIGRATION();
        return Tag::kBytes;
    }
  }

  template <typename T>
  const T& As() const {
    return absl::get<T>(variant_);
  }

  template <typename T>
  T& As() {
    return absl::get<T>(variant_);
  }

 private:
  Variant variant_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
