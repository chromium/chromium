// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_

#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class LayoutIFrame;
class LayoutObject;
class LayoutTable;
class LayoutTableCaption;
class LayoutTableSection;
class LayoutTableRow;
class LocalFrame;

// AIPageContent is responsible for handling requests for inner-text. It calls
// to InnerTextBuilder to handle building of the text.
class MODULES_EXPORT AIPageContentAgent final
    : public GarbageCollected<AIPageContentAgent>,
      public mojom::blink::AIPageContentAgent,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static AIPageContentAgent* From(Document&);
  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  static AIPageContentAgent* GetOrCreateForTesting(Document&);

  AIPageContentAgent(base::PassKey<AIPageContentAgent>, LocalFrame&);
  AIPageContentAgent(const AIPageContentAgent&) = delete;
  AIPageContentAgent& operator=(const AIPageContentAgent&) = delete;
  ~AIPageContentAgent() override;

  void Trace(Visitor* visitor) const override;

  // mojom::blink::AIPageContentAgent overrides.
  void GetAIPageContent(GetAIPageContentCallback callback) override;

  mojom::blink::AIPageContentPtr GetAIPageContentSync() const;

 private:
  void Bind(mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  void ProcessNode(const LayoutObject& object,
                   mojom::blink::AIPageContentNode& content_node,
                   const ComputedStyle& document_style) const;
  void ProcessIframe(const LayoutIFrame& object,
                     mojom::blink::AIPageContentNode& content_node) const;
  mojom::blink::AIPageContentNodePtr MaybeGenerateContentNode(
      const LayoutObject& object) const;
  void MaybeAddNodeContent(const LayoutObject& object,
                           mojom::blink::AIPageContentAttributes& attributes,
                           const ComputedStyle& document_style) const;
  void AddNodeId(const LayoutObject& object,
                 mojom::blink::AIPageContentAttributes& attributes) const;
  void AddNodeGeometry(const LayoutObject& object,
                       mojom::blink::AIPageContentGeometry& geometry) const;
  void ProcessTable(const LayoutTable& object,
                    mojom::blink::AIPageContentNode& content_node,
                    const ComputedStyle& document_style) const;
  void ProcessTableCaption(
      const LayoutTableCaption& object,
      mojom::blink::AIPageContentTableData& table_data) const;
  void ProcessTableSection(const LayoutTableSection& object,
                           mojom::blink::AIPageContentTableData& table_data,
                           const ComputedStyle& document_style) const;
  void ProcessTableRow(const LayoutTableRow& object,
                       mojom::blink::AIPageContentTableRow& table_row,
                       const ComputedStyle& document_style) const;

  HeapMojoReceiverSet<mojom::blink::AIPageContentAgent, AIPageContentAgent>
      receiver_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
