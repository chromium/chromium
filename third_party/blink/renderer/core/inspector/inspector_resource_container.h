// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_RESOURCE_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_RESOURCE_CONTAINER_H_

#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id_traits.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
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
  InspectorResourceContainer(const InspectorResourceContainer&) = delete;
  InspectorResourceContainer& operator=(const InspectorResourceContainer&) =
      delete;
  ~InspectorResourceContainer();
  void Trace(Visitor*) const;

  void DidCommitLoadForLocalFrame(LocalFrame*);

  void StoreStyleSheetContent(const String& url, const String& content);
  bool LoadStyleSheetContent(const String& url, String* content);

  void StoreStyleElementContent(DOMNodeIdType backend_node_id,
                                const String& content);
  bool LoadStyleElementContent(DOMNodeIdType backend_node_id, String* content);
  void EraseStyleElementContent(DOMNodeIdType backend_node_id);

 private:
  Member<InspectedFrames> inspected_frames_;
  HashMap<String, String> style_sheet_contents_;
  HashMap<DOMNodeIdType, String> style_element_contents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_RESOURCE_CONTAINER_H_
