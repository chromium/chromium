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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_SERIALIZER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_SERIALIZER_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class Element;
class Frame;
class LocalFrame;
class Node;
class TextEncoding;

// Responsible for serializing the specified frame into html
// (replacing links with paths to local files).
class CORE_EXPORT WebFrameSerializerImpl {
  STACK_ALLOCATED();

 public:
  // Serializes the frame and sends HTML chunks via `handler_remote_`
  // (`DidReceiveData()` followed by `Done()`). If the target frame does not
  // have a valid URL, sends an empty chunk followed by `Done()`.
  void Serialize();

  WebFrameSerializerImpl(
      LocalFrame* frame,
      const HashMap<KURL, base::FilePath>& url_to_local_path,
      const HashMap<FrameToken, base::FilePath>& frame_token_to_local_path,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::blink::FrameHTMLSerializerHandler>
          handler_remote);

 private:
  // Specified frame which needs to be serialized.
  LocalFrame* frame_;
  // Maps from original resource URLs / child frame tokens to relative local
  // file paths used when rewriting link attributes.
  const HashMap<KURL, base::FilePath>& url_to_local_path_;
  const HashMap<FrameToken, base::FilePath>& frame_token_to_local_path_;
  mojo::Remote<mojom::blink::FrameHTMLSerializerHandler> handler_remote_;
  // Data buffer for saving result of serialized DOM data.
  StringBuilder data_buffer_;
  // Whether frame url should be shown in MOTW in the serialized html.
  bool save_with_empty_url_;

  class SerializeDomParam {
    STACK_ALLOCATED();

   public:
    SerializeDomParam(const KURL&, const TextEncoding&, Document*);

    const KURL& url;
    const TextEncoding& text_encoding;
    Document* document;
    bool is_html_document;  // document.isHTMLDocument()
    bool have_seen_doc_type;
    bool have_added_charset_declaration;
    // This meta element need to be skipped when serializing DOM.
    const Element* skip_meta_element;
    bool have_added_xml_processing_directive;
    // Flag indicates whether we have added additional contents before end tag.
    // This flag will be re-assigned in each call of function
    // PostActionAfterSerializeOpenTag and it could be changed in function
    // PreActionBeforeSerializeEndTag if the function adds new contents into
    // serialization stream.
    bool have_added_contents_before_end;
  };

  // Before we begin serializing open tag of a element, we give the target
  // element a chance to do some work prior to add some additional data.
  String PreActionBeforeSerializeOpenTag(const Element*,
                                         SerializeDomParam*,
                                         bool* need_skip);

  // After we finish serializing open tag of a element, we give the target
  // element a chance to do some post work to add some additional data.
  String PostActionAfterSerializeOpenTag(const Element*, SerializeDomParam*);

  // Before we begin serializing end tag of a element, we give the target
  // element a chance to do some work prior to add some additional data.
  String PreActionBeforeSerializeEndTag(const Element*,
                                        SerializeDomParam*,
                                        bool* need_skip);

  // After we finish serializing end tag of a element, we give the target
  // element a chance to do some post work to add some additional data.
  String PostActionAfterSerializeEndTag(const Element*, SerializeDomParam*);

  // Save generated html content to data buffer.
  void SaveHTMLContentToBuffer(const StringView& content, SerializeDomParam*);

  enum FlushOption {
    kForceFlush,
    kDoNotForceFlush,
  };

  // Flushes the content buffer by encoding and sending the content via
  // `handler_remote_`. Content is not flushed if the buffer is not full unless
  // `flush_option` is `kForceFlush`.
  void EncodeAndFlushBuffer(bool is_finished, SerializeDomParam*, FlushOption);

  void SendSerializedData(const std::string& data, bool is_finished);

  bool RewriteFrameSource(Frame* frame, String* rewritten_link) const;
  bool RewriteLink(const KURL& url, String* rewritten_link) const;

  // Serialize the open tag of a ShadowRoot as a <template>
  void ShadowRootTagToString(ShadowRoot*, SerializeDomParam*);

  // Serialize open tag of an specified element.
  void OpenTagToString(Element*, SerializeDomParam*);

  // Serialize end tag of an specified element.
  void EndTagToString(Element*, SerializeDomParam*);

  // Build content for a specified node
  void BuildContentForNode(Node*, SerializeDomParam*);

  // Appends attrName="escapedAttrValue" to result.
  void AppendAttribute(StringBuilder& result,
                       bool is_html_document,
                       const QualifiedName& attr_name,
                       const String& attr_value);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_SERIALIZER_IMPL_H_
