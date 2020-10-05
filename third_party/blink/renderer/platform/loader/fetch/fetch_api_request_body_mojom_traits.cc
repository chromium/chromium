// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_api_request_body_mojom_traits.h"

#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace mojo {

// static
WTF::Vector<blink::mojom::blink::FetchAPIDataElementPtr>
StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
             blink::ResourceRequestBody>::elements(blink::ResourceRequestBody&
                                                       mutable_body) {
  WTF::Vector<blink::mojom::blink::FetchAPIDataElementPtr> out_elements;
  const auto& body = mutable_body;
  if (body.IsEmpty()) {
    return out_elements;
  }

  if (mutable_body.StreamBody()) {
    auto out = blink::mojom::blink::FetchAPIDataElement::New();
    out->type = network::mojom::DataElementType::kReadOnceStream;
    out->chunked_data_pipe_getter = mutable_body.TakeStreamBody();
    out_elements.push_back(std::move(out));
    return out_elements;
  }

  DCHECK(body.FormBody());
  for (const auto& element : body.FormBody()->elements_) {
    auto out = blink::mojom::blink::FetchAPIDataElement::New();
    switch (element.type_) {
      case blink::FormDataElement::kData:
        out->type = network::mojom::DataElementType::kBytes;
        out->buf.ReserveCapacity(element.data_.size());
        for (const char c : element.data_) {
          out->buf.push_back(static_cast<uint8_t>(c));
        }
        break;
      case blink::FormDataElement::kEncodedFile:
        out->type = network::mojom::DataElementType::kFile;
        out->path = base::FilePath::FromUTF8Unsafe(element.filename_.Utf8());
        out->offset = element.file_start_;
        out->length = element.file_length_;
        out->expected_modification_time =
            element.expected_file_modification_time_.value_or(base::Time());
        break;
      case blink::FormDataElement::kEncodedBlob: {
        out->type = network::mojom::DataElementType::kDataPipe;
        out->length = element.optional_blob_data_handle_->size();

        mojo::Remote<blink::mojom::blink::Blob> blob_remote(
            mojo::PendingRemote<blink::mojom::blink::Blob>(
                element.optional_blob_data_handle_->CloneBlobRemote()
                    .PassPipe(),
                blink::mojom::blink::Blob::Version_));
        blob_remote->AsDataPipeGetter(
            out->data_pipe_getter.InitWithNewPipeAndPassReceiver());
        break;
      }
      case blink::FormDataElement::kDataPipe:
        out->type = network::mojom::DataElementType::kDataPipe;
        if (element.data_pipe_getter_) {
          element.data_pipe_getter_->GetDataPipeGetter()->Clone(
              out->data_pipe_getter.InitWithNewPipeAndPassReceiver());
        }
        break;
    }
    out_elements.push_back(std::move(out));
  }
  return out_elements;
}

// static
bool StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                  blink::ResourceRequestBody>::
    Read(blink::mojom::FetchAPIRequestBodyDataView in,
         blink::ResourceRequestBody* out) {
  if (in.is_null()) {
    *out = blink::ResourceRequestBody();
    return true;
  }

  mojo::ArrayDataView<blink::mojom::FetchAPIDataElementDataView> elements_view;
  in.GetElementsDataView(&elements_view);
  if (elements_view.size() == 1) {
    blink::mojom::FetchAPIDataElementDataView view;
    elements_view.GetDataView(0, &view);

    network::mojom::DataElementType type;
    if (!view.ReadType(&type)) {
      return false;
    }
    if (type == network::mojom::DataElementType::kReadOnceStream) {
      auto chunked_data_pipe_getter = view.TakeChunkedDataPipeGetter<
          mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>>();
      *out = blink::ResourceRequestBody(std::move(chunked_data_pipe_getter));
      return true;
    }
  }
  auto form_data = blink::EncodedFormData::Create();
  for (size_t i = 0; i < elements_view.size(); ++i) {
    blink::mojom::FetchAPIDataElementDataView view;
    elements_view.GetDataView(i, &view);

    network::mojom::DataElementType type;
    if (!view.ReadType(&type)) {
      return false;
    }
    switch (type) {
      case network::mojom::DataElementType::kBytes: {
        // TODO(richard.li): Delete this workaround when type of
        // blink::FormDataElement::data_ is changed to WTF::Vector<uint8_t>
        WTF::Vector<uint8_t> buf;
        if (!view.ReadBuf(&buf)) {
          return false;
        }
        form_data->AppendData(buf.data(), buf.size());
        break;
      }
      case network::mojom::DataElementType::kFile: {
        base::FilePath file_path;
        base::Time expected_time;
        if (!view.ReadPath(&file_path) ||
            !view.ReadExpectedModificationTime(&expected_time)) {
          return false;
        }
        base::Optional<base::Time> expected_file_modification_time;
        if (!expected_time.is_null()) {
          expected_file_modification_time = expected_time;
        }
        form_data->AppendFileRange(blink::FilePathToString(file_path),
                                   view.offset(), view.length(),
                                   expected_file_modification_time);
        break;
      }
      case network::mojom::DataElementType::kDataPipe: {
        auto data_pipe_ptr_remote = view.TakeDataPipeGetter<
            mojo::PendingRemote<network::mojom::blink::DataPipeGetter>>();
        DCHECK(data_pipe_ptr_remote.is_valid());

        form_data->AppendDataPipe(
            base::MakeRefCounted<blink::WrappedDataPipeGetter>(
                std::move(data_pipe_ptr_remote)));

        break;
      }
      case network::mojom::DataElementType::kUnknown:
      case network::mojom::DataElementType::kChunkedDataPipe:
      case network::mojom::DataElementType::kReadOnceStream:
        NOTREACHED();
        return false;
    }
  }

  form_data->identifier_ = in.identifier();
  form_data->contains_password_data_ = in.contains_sensitive_info();
  form_data->SetBoundary(
      blink::FormDataEncoder::GenerateUniqueBoundaryString());
  *out = blink::ResourceRequestBody(std::move(form_data));
  return true;
}

}  // namespace mojo
