// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/platform/network/encoded_form_data_element_mojom_traits.h"

#include "base/feature_list.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace mojo {

// static
network::mojom::DataElementType
StructTraits<blink::mojom::FetchAPIDataElementDataView,
             blink::FormDataElement>::type(const blink::FormDataElement& data) {
  switch (data.type_) {
    case blink::FormDataElement::kData:
      return network::mojom::DataElementType::kBytes;
    case blink::FormDataElement::kEncodedFile:
      return network::mojom::DataElementType::kFile;
    case blink::FormDataElement::kEncodedBlob: {
      if (data.optional_blob_data_handle_)
        return network::mojom::DataElementType::kDataPipe;
      return network::mojom::DataElementType::kBlob;
    }
    case blink::FormDataElement::kDataPipe:
      return network::mojom::DataElementType::kDataPipe;
  }
  NOTREACHED();
  return network::mojom::DataElementType::kUnknown;
}

// static
base::span<const uint8_t>
StructTraits<blink::mojom::FetchAPIDataElementDataView,
             blink::FormDataElement>::buf(const blink::FormDataElement& data) {
  return base::make_span(reinterpret_cast<const uint8_t*>(data.data_.data()),
                         data.data_.size());
}

// static
base::File
StructTraits<blink::mojom::FetchAPIDataElementDataView,
             blink::FormDataElement>::file(const blink::FormDataElement& data) {
  return base::File();
}

// static
base::FilePath
StructTraits<blink::mojom::FetchAPIDataElementDataView,
             blink::FormDataElement>::path(const blink::FormDataElement& data) {
  return base::FilePath::FromUTF8Unsafe(data.filename_.Utf8());
}

// static
mojo::PendingRemote<network::mojom::blink::DataPipeGetter> StructTraits<
    blink::mojom::FetchAPIDataElementDataView,
    blink::FormDataElement>::data_pipe_getter(const blink::FormDataElement&
                                                  data) {
  if (data.type_ == blink::FormDataElement::kDataPipe) {
    if (!data.data_pipe_getter_)
      return mojo::NullRemote();
    mojo::PendingRemote<network::mojom::blink::DataPipeGetter> data_pipe_getter;
    data.data_pipe_getter_->GetDataPipeGetter()->Clone(
        data_pipe_getter.InitWithNewPipeAndPassReceiver());
    return data_pipe_getter;
  }
  if (data.type_ == blink::FormDataElement::kEncodedBlob) {
    if (data.optional_blob_data_handle_) {
      mojo::Remote<blink::mojom::blink::Blob> blob_remote(
          mojo::PendingRemote<blink::mojom::blink::Blob>(
              data.optional_blob_data_handle_->CloneBlobRemote().PassPipe(),
              blink::mojom::blink::Blob::Version_));
      mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
          data_pipe_getter_remote;
      blob_remote->AsDataPipeGetter(
          data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
      return data_pipe_getter_remote;
    }
  }
  return mojo::NullRemote();
}

// static
base::Time StructTraits<blink::mojom::FetchAPIDataElementDataView,
                        blink::FormDataElement>::
    expected_modification_time(const blink::FormDataElement& data) {
  if (data.type_ == blink::FormDataElement::kEncodedFile)
    return base::Time::FromDoubleT(data.expected_file_modification_time_);
  return base::Time();
}

// static
bool StructTraits<blink::mojom::FetchAPIDataElementDataView,
                  blink::FormDataElement>::
    Read(blink::mojom::FetchAPIDataElementDataView data,
         blink::FormDataElement* out) {
  network::mojom::DataElementType data_type;
  if (!data.ReadType(&data_type)) {
    return false;
  }
  out->file_start_ = data.offset();
  out->file_length_ = data.length();

  switch (data_type) {
    case network::mojom::DataElementType::kBytes: {
      out->type_ = blink::FormDataElement::kData;
      // TODO(richard.li): Delete this workaround when type of
      // blink::FormDataElement::data_ is changed to WTF::Vector<uint8_t>
      WTF::Vector<uint8_t> buf;
      if (!data.ReadBuf(&buf)) {
        return false;
      }
      out->data_.AppendRange(buf.begin(), buf.end());
      break;
    }
    case network::mojom::DataElementType::kFile: {
      out->type_ = blink::FormDataElement::kEncodedFile;
      base::FilePath file_path;
      base::Time expected_time;
      if (!data.ReadPath(&file_path) ||
          !data.ReadExpectedModificationTime(&expected_time)) {
        return false;
      }
      out->expected_file_modification_time_ = expected_time.ToDoubleT();
      out->filename_ = blink::FilePathToString(file_path);
      break;
    }
    case network::mojom::DataElementType::kDataPipe: {
      out->type_ = blink::FormDataElement::kDataPipe;
      auto data_pipe_ptr_remote = data.TakeDataPipeGetter<
          mojo::PendingRemote<network::mojom::blink::DataPipeGetter>>();
      DCHECK(data_pipe_ptr_remote.is_valid());

      out->data_pipe_getter_ =
          base::MakeRefCounted<blink::WrappedDataPipeGetter>(
              std::move(data_pipe_ptr_remote));
      break;
    }
    case network::mojom::DataElementType::kBlob:
    case network::mojom::DataElementType::kUnknown:
    case network::mojom::DataElementType::kChunkedDataPipe:
    case network::mojom::DataElementType::kRawFile:
      NOTREACHED();
      return false;
  }
  return true;
}

}  // namespace mojo
