// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_api_request_body_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace mojo {

// static
WTF::Vector<network::DataElement>
StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
             blink::ResourceRequestBody>::elements(blink::ResourceRequestBody&
                                                       mutable_body) {
  scoped_refptr<network::ResourceRequestBody> network_body;
  if (auto form_body = mutable_body.FormBody()) {
    DUMP_WILL_BE_CHECK_NE(blink::EncodedFormData::FormDataType::kInvalid,
                          form_body->GetType());
    // Here we need to keep the original body, because other members such as
    // `identifier` are on the form body.
    network_body =
        NetworkResourceRequestBodyFor(blink::ResourceRequestBody(form_body));
  } else if (mutable_body.StreamBody()) {
    // Here we don't need to keep the original body (and it's impossible to do
    // so, because the streaming body is not copyable).
    network_body = NetworkResourceRequestBodyFor(std::move(mutable_body));
  }
  if (!network_body) {
    return WTF::Vector<network::DataElement>();
  }
  WTF::Vector<network::DataElement> out_elements;
  DCHECK(network_body->elements_mutable());
  for (auto& element : *network_body->elements_mutable()) {
    out_elements.emplace_back(std::move(element));
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

  mojo::ArrayDataView<network::mojom::DataElementDataView> elements_view;
  in.GetElementsDataView(&elements_view);
  if (elements_view.size() == 1) {
    network::mojom::DataElementDataView view;
    elements_view.GetDataView(0, &view);

    DCHECK(!view.is_null());
    if (view.tag() == network::DataElement::Tag::kChunkedDataPipe) {
      network::DataElement element;
      if (!elements_view.Read(0, &element)) {
        return false;
      }
      auto& chunked_data_pipe =
          element.As<network::DataElementChunkedDataPipe>();
      *out = blink::ResourceRequestBody(blink::ToCrossVariantMojoType(
          chunked_data_pipe.ReleaseChunkedDataPipeGetter()));
      return true;
    }
  }
  auto form_data = blink::EncodedFormData::Create();
  for (size_t i = 0; i < elements_view.size(); ++i) {
    network::DataElement element;
    if (!elements_view.Read(i, &element)) {
      return false;
    }

    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        const auto& bytes = element.As<network::DataElementBytes>();
        form_data->AppendData(
            bytes.bytes().data(),
            base::checked_cast<wtf_size_t>(bytes.bytes().size()));
        break;
      }
      case network::DataElement::Tag::kFile: {
        const auto& file = element.As<network::DataElementFile>();
        std::optional<base::Time> expected_modification_time;
        if (!file.expected_modification_time().is_null()) {
          expected_modification_time = file.expected_modification_time();
        }
        form_data->AppendFileRange(blink::FilePathToString(file.path()),
                                   file.offset(), file.length(),
                                   expected_modification_time);
        break;
      }
      case network::DataElement::Tag::kDataPipe: {
        auto& datapipe = element.As<network::DataElementDataPipe>();
        form_data->AppendDataPipe(
            base::MakeRefCounted<blink::WrappedDataPipeGetter>(
                blink::ToCrossVariantMojoType(
                    datapipe.ReleaseDataPipeGetter())));
        break;
      }
      case network::DataElement::Tag::kChunkedDataPipe:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }

  DUMP_WILL_BE_CHECK_NE(blink::EncodedFormData::FormDataType::kInvalid,
                        form_data->GetType());
  form_data->identifier_ = in.identifier();
  form_data->contains_password_data_ = in.contains_sensitive_info();
  form_data->SetBoundary(
      blink::FormDataEncoder::GenerateUniqueBoundaryString());
  *out = blink::ResourceRequestBody(std::move(form_data));
  return true;
}

}  // namespace mojo
