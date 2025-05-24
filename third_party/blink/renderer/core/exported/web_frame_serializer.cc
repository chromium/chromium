/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_frame_serializer.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_serializer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_frame_serializer_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

void ContinueGenerateMHTMLParts(
    const WebString& boundary,
    const blink::LocalFrameToken& frame_token,
    MHTMLArchive::EncodingPolicy encoding_policy,
    base::OnceCallback<void(WebThreadSafeData)> callback,
    Deque<SerializedResource> resources) {
  WebFrame* web_frame = WebLocalFrame::FromFrameToken(frame_token);
  LocalFrame* frame =
      web_frame ? To<WebLocalFrameImpl>(web_frame)->GetFrame() : nullptr;

  TRACE_EVENT_END1("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts serializing",
                   "resource count", static_cast<uint64_t>(resources.size()));

  // There was an error serializing the frame (e.g. of an image resource).
  if (resources.empty() || !frame) {
    std::move(callback).Run(WebThreadSafeData());
    return;
  }

  // Encode serialized resources as MHTML.
  scoped_refptr<RawData> output = RawData::Create();
  {
    // Frame is the 1st resource (see FrameSerializer::serializeFrame doc
    // comment). Frames get a Content-ID header.
    MHTMLArchive::GenerateMHTMLPart(
        boundary, FrameSerializer::GetContentID(frame), encoding_policy,
        resources.TakeFirst(), *output->MutableData());
    while (!resources.empty()) {
      TRACE_EVENT0("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts encoding");
      MHTMLArchive::GenerateMHTMLPart(boundary, String(), encoding_policy,
                                      resources.TakeFirst(),
                                      *output->MutableData());
    }
  }
  std::move(callback).Run(WebThreadSafeData(output));
}

}  // namespace

WebThreadSafeData WebFrameSerializer::GenerateMHTMLHeader(
    const WebString& boundary,
    WebLocalFrame* frame,
    MHTMLPartsGenerationDelegate* delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLHeader");
  DCHECK(frame);
  DCHECK(delegate);

  auto* web_local_frame = To<WebLocalFrameImpl>(frame);

  Document* document = web_local_frame->GetFrame()->GetDocument();

  scoped_refptr<RawData> buffer = RawData::Create();
  MHTMLArchive::GenerateMHTMLHeader(
      boundary, document->Url(), document->title(),
      document->SuggestedMIMEType(), base::Time::Now(), *buffer->MutableData());
  return WebThreadSafeData(buffer);
}

void WebFrameSerializer::GenerateMHTMLParts(
    const WebString& boundary,
    WebLocalFrame* web_frame,
    MHTMLPartsGenerationDelegate* web_delegate,
    base::OnceCallback<void(WebThreadSafeData)> callback) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLParts");
  DCHECK(web_frame);
  DCHECK(web_delegate);

  // Translate arguments from public to internal blink APIs.
  LocalFrame* frame = To<WebLocalFrameImpl>(web_frame)->GetFrame();
  MHTMLArchive::EncodingPolicy encoding_policy =
      web_delegate->UseBinaryEncoding()
          ? MHTMLArchive::EncodingPolicy::kUseBinaryEncoding
          : MHTMLArchive::EncodingPolicy::kUseDefaultEncoding;

  // Serialize.
  TRACE_EVENT_BEGIN0("page-serialization",
                     "WebFrameSerializer::generateMHTMLParts serializing");
  Deque<SerializedResource> resources;
  FrameSerializer::SerializeFrame(
      *web_delegate, *frame,
      WTF::BindOnce(&ContinueGenerateMHTMLParts, boundary,
                    web_frame->GetLocalFrameToken(), encoding_policy,
                    std::move(callback)));
}

bool WebFrameSerializer::Serialize(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate,
    bool save_with_empty_url) {
  WebFrameSerializerImpl serializer_impl(frame, client, delegate,
                                         save_with_empty_url);
  return serializer_impl.Serialize();
}

WebString WebFrameSerializer::GenerateMetaCharsetDeclaration(
    const WebString& charset) {
  // TODO(yosin) We should call |FrameSerializer::metaCharsetDeclarationOf()|.
  String charset_string =
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" +
      static_cast<const String&>(charset) + "\">";
  return charset_string;
}

WebString WebFrameSerializer::GenerateMarkOfTheWebDeclaration(
    const WebURL& url) {
  StringBuilder builder;
  builder.Append("\n<!-- ");
  builder.Append(FrameSerializer::MarkOfTheWebDeclaration(url));
  builder.Append(" -->\n");
  return builder.ToString();
}

}  // namespace blink
