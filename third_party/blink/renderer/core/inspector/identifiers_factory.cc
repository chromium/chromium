/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"

#include "base/atomic_sequence_num.h"
#include "base/process/process_handle.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// static
String IdentifiersFactory::CreateIdentifier() {
  static base::AtomicSequenceNumber last_used_identifier;
  return AddProcessIdPrefixTo(last_used_identifier.GetNext());
}

// static
String IdentifiersFactory::RequestId(DocumentLoader* loader,
                                     uint64_t identifier) {
  if (!identifier)
    return String();
  if (loader && loader->MainResourceIdentifier() == identifier)
    return LoaderId(loader);
  return AddProcessIdPrefixTo(identifier);
}

// static
String IdentifiersFactory::SubresourceRequestId(uint64_t identifier) {
  return RequestId(nullptr, identifier);
}

// static
String IdentifiersFactory::FrameId(Frame* frame) {
  // Note: this should be equal to ToTraceValue(frame).
  return String(ToTraceValue(frame).data());
}

// static
LocalFrame* IdentifiersFactory::FrameById(InspectedFrames* inspected_frames,
                                          const String& frame_id) {
  for (auto* frame : *inspected_frames) {
    if (frame->Client() &&
        frame_id == IdFromToken(frame->GetDevToolsFrameToken())) {
      return frame;
    }
  }
  return nullptr;
}

// static
String IdentifiersFactory::LoaderId(DocumentLoader* loader) {
  if (!loader)
    return g_empty_string;
  const base::UnguessableToken& token = loader->GetDevToolsNavigationToken();
  // token.ToString() is latin1.
  return String(token.ToString().c_str());
}

// static
String IdentifiersFactory::IdFromToken(const base::UnguessableToken& token) {
  if (token.is_empty())
    return g_empty_string;
  // token.ToString() is latin1.
  return String(token.ToString().c_str());
}

// static
int IdentifiersFactory::IntIdForNode(Node* node) {
  return static_cast<int>(DOMNodeIds::IdForNode(node));
}

// static
String IdentifiersFactory::AddProcessIdPrefixTo(uint64_t id) {
  uint32_t process_id = base::GetUniqueIdForProcess().GetUnsafeValue();

  StringBuilder builder;

  builder.AppendNumber(process_id);
  builder.Append('.');
  builder.AppendNumber(id);

  return builder.ToString();
}

}  // namespace blink
