// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_

#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class LayoutIFrame;
class LayoutObject;
class LocalFrame;

// AIPageContent is responsible for handling requests for inner-text. It calls
// to InnerTextBuilder to handle building of the text.
class MODULES_EXPORT AIPageContentAgent final
    : public GarbageCollected<AIPageContentAgent>,
      public mojom::blink::AIPageContentAgent,
      public Supplement<Document>,
      public LocalFrameView::LifecycleNotificationObserver {
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
  void GetAIPageContent(mojom::blink::AIPageContentOptionsPtr options,
                        GetAIPageContentCallback callback) override;

  // public for testing.
  mojom::blink::AIPageContentPtr GetAIPageContentInternal(
      const mojom::blink::AIPageContentOptions& options) const;
  // LocalFrameView::LifecycleNotificationObserver overrides.
  void DidFinishPostLifecycleSteps(const LocalFrameView&) override;

 private:
  void GetAIPageContentSync(mojom::blink::AIPageContentOptionsPtr options,
                            GetAIPageContentCallback callback,
                            base::TimeTicks start_time) const;
  // Runs tasks if the document lifecycle is at least as advanced as the
  // associated vector.
  void RunTasksIfReady();

  // Synchronously services a single request.
  class ContentBuilder {
   public:
    ContentBuilder(const mojom::blink::AIPageContentOptions& options);
    ~ContentBuilder();

    mojom::blink::AIPageContentPtr Build(LocalFrame& frame);

   private:
    // Returns true if any descendant of `object` has a computed value of
    // visible for `visibility`.
    bool WalkChildren(const LayoutObject& object,
                      mojom::blink::AIPageContentNode& content_node,
                      const ComputedStyle& document_style) const;
    void ProcessIframe(const LayoutIFrame& object,
                       mojom::blink::AIPageContentNode& content_node) const;
    mojom::blink::AIPageContentNodePtr MaybeGenerateContentNode(
        const LayoutObject& object,
        const ComputedStyle& document_style) const;
    std::optional<DOMNodeId> AddNodeId(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;
    void AddNodeGeometry(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;
    void AddNodeInteractionInfo(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const;

    const raw_ref<const mojom::blink::AIPageContentOptions> options_;
  };

  void Bind(mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver);

  HeapMojoReceiverSet<mojom::blink::AIPageContentAgent, AIPageContentAgent>
      receiver_set_;
  // Already registered for lifetime notifications.
  bool is_registered_ = false;
  // Tasks to run when geometry is needed.
  WTF::Vector<base::OnceClosure> geometry_tasks_;
  // Tasks to run when geometry is not needed.
  WTF::Vector<base::OnceClosure> layout_clean_tasks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_AGENT_H_
