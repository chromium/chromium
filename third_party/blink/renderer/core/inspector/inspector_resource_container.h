// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_RESOURCE_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_RESOURCE_CONTAINER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectedFrames;
class LocalFrame;

class CORE_EXPORT InspectorResourceContainer final
    : public GarbageCollected<InspectorResourceContainer> {
 public:
  explicit InspectorResourceContainer(InspectedFrames*);
  ~InspectorResourceContainer();
  void Trace(blink::Visitor*);

  void DidCommitLoadForLocalFrame(LocalFrame*);

  void StoreStyleSheetContent(const String& url, const String& content);
  bool LoadStyleSheetContent(const String& url, String* content);

  void StoreStyleElementContent(DOMNodeId backend_node_id,
                                const String& content);
  bool LoadStyleElementContent(DOMNodeId backend_node_id, String* content);
  void EraseStyleElementContent(DOMNodeId backend_node_id);

 private:
  Member<InspectedFrames> inspected_frames_;
  HashMap<String, String> style_sheet_contents_;
  HashMap<DOMNodeId, String> style_element_contents_;
  DISALLOW_COPY_AND_ASSIGN(InspectorResourceContainer);
};

}  // namespace blink

#endif  // !defined(InspectorResourceContainer_h)
