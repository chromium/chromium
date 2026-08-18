// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_EXTERNAL_PATCH_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_EXTERNAL_PATCH_LOADER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
class DocumentParser;
class HTMLTemplateElement;
class Patch;

class ExternalPatchLoader : public GarbageCollected<ExternalPatchLoader>,
                            public RawResourceClient {
 public:
  ExternalPatchLoader(Patch* owner,
                      HTMLTemplateElement* template_element,
                      const AtomicString& src_attr);

  void Cancel();
  void Trace(Visitor* visitor) const override;

  // RawResourceClient implementation:
  void ResponseReceived(Resource*, const ResourceResponse&) override;
  void DataReceived(Resource*, base::span<const char>) override;
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "ExternalPatchLoader"; }

 private:
  Member<Patch> owner_;
  Member<HTMLTemplateElement> template_;
  Member<RawResource> resource_;
  Member<DocumentParser> parser_;
  enum ReadyState { kWaitingForResource, kReady, kErrorOccurred };
  ReadyState ready_state_ = kReady;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_EXTERNAL_PATCH_LOADER_H_
