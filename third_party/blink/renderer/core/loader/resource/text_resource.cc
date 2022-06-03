// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/text_resource.h"

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class SVGDocumentResourceFactory : public ResourceFactory {
 public:
  SVGDocumentResourceFactory()
      : ResourceFactory(ResourceType::kSVGDocument,
                        TextResourceDecoderOptions::kXMLContent) {}

  Resource* Create(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      const TextResourceDecoderOptions& decoder_options) const override {
    return MakeGarbageCollected<TextResource>(
        request, ResourceType::kSVGDocument, options, decoder_options);
  }
};

}  // namespace

TextResource* TextResource::FetchSVGDocument(FetchParameters& params,
                                             ResourceFetcher* fetcher,
                                             ResourceClient* client) {
  return To<TextResource>(
      fetcher->RequestResource(params, SVGDocumentResourceFactory(), client));
}

TextResource::TextResource(const ResourceRequest& resource_request,
                           ResourceType type,
                           const ResourceLoaderOptions& options,
                           const TextResourceDecoderOptions& decoder_options)
    : Resource(resource_request, type, options),
      decoder_(std::make_unique<TextResourceDecoder>(decoder_options)) {}

TextResource::~TextResource() = default;

void TextResource::SetEncoding(const String& chs) {
  decoder_->SetEncoding(WTF::TextEncoding(chs),
                        TextResourceDecoder::kEncodingFromHTTPHeader);
}

WTF::TextEncoding TextResource::Encoding() const {
  return decoder_->Encoding();
}

String TextResource::DecodedText() const {
  DCHECK(Data());

  StringBuilder builder;
  for (const auto& span : *Data())
    builder.Append(decoder_->Decode(span.data(), span.size()));
  builder.Append(decoder_->Flush());
  return builder.ToString();
}

}  // namespace blink
