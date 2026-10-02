// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DICTATION_DICTATION_AGENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DICTATION_DICTATION_AGENT_IMPL_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/dictation/dictation_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class EditContext;
class HTMLElement;
class LocalFrame;

// Per-Document agent used by the browser's dictation feature to run code in the
// renderer. Created lazily when the browser binds the interface.
class MODULES_EXPORT DictationAgentImpl final
    : public GarbageCollected<DictationAgentImpl>,
      public mojom::blink::DictationAgent,
      public Supplement<Document> {
 public:
  using PassKey = base::PassKey<DictationAgentImpl>;

  static const char kSupplementName[];

  // Returns the agent for `document`, creating it if needed. Returns nullptr
  // if the document is inactive.
  static DictationAgentImpl* CreateIfNeeded(Document& document);

  // Registered in the frame's interface registry; binds `receiver` to the
  // agent of the frame's current document.
  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::DictationAgent> receiver);

  // Only created through CreateIfNeeded().
  DictationAgentImpl(Document& document, PassKey);
  DictationAgentImpl(const DictationAgentImpl&) = delete;
  DictationAgentImpl& operator=(const DictationAgentImpl&) = delete;
  ~DictationAgentImpl() override = default;

  void Trace(Visitor* visitor) const override;

  // mojom::blink::DictationAgent:
  void PopulateEditContext(int32_t target_dom_node_id,
                           PopulateEditContextCallback callback) override;

 private:
  void Bind(mojo::PendingReceiver<mojom::blink::DictationAgent> receiver);

  // Returns the target element if it is in this document.
  HTMLElement* GetTarget(int32_t target_dom_node_id) const;

  // Returns `target`'s EditContext if `target` is focused and its EditContext
  // is the frame's active one, or nullptr otherwise.
  EditContext* GetTargetEditContext(HTMLElement& target) const;

  // Only the current dictation session's Target talks to this agent. A new
  // session's bind replaces the previous connection.
  HeapMojoReceiver<mojom::blink::DictationAgent, DictationAgentImpl> receiver_;

  // PopulateEditContext() dispatches its events at most once per document.
  bool did_populate_edit_context_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DICTATION_DICTATION_AGENT_IMPL_H_
