// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ANNOTATION_AGENT_H_
#define PDF_PDF_ANNOTATION_AGENT_H_

#include <ostream>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/cxx23_to_underlying.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace chrome_pdf {

// Represents a single instance of text fragment annotation in a PDF. It's
// uniquely owned by the `PdfViewWebPlugin`. For each annotation, a new instance
// is created to replace the previous instance.
class PdfAnnotationAgent : public blink::mojom::AnnotationAgent {
 public:
  // The name "Container" is to respect the mojom naming convention: an
  // AnnotationAgentContainer uniquely owns one or more AnnotationAgent.
  class Container {
   public:
    // Attempts to find and highlight all the `text_fragments` in the PDF.
    // Returns true if any of the fragments is found.
    virtual bool FindAndHighlightTextFragments(
        base::span<const std::string> text_fragments) = 0;

    // Scrolls the found text fragment into the viewport.
    virtual void ScrollTextFragmentIntoView() = 0;

    // Removes the found text fragment, and its highlight.
    virtual void RemoveTextFragments() = 0;

   protected:
    ~Container() = default;
  };

  PdfAnnotationAgent(
      Container* container,
      blink::mojom::AnnotationType type,
      blink::mojom::SelectorPtr selector,
      mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote,
      mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver);
  ~PdfAnnotationAgent() override;

  // `blink::mojom::AnnotationAgent`:
  void ScrollIntoView(bool applies_focus) override;

  // Remove the text fragments from the PDF document and set `state_` to
  // `kHighlightDropped`.
  void RemoveTextFragments();

 private:
  enum class State {
    // The initial state.
    kInitial = 0,

    // When `this` has found the text fragment and has dispatched
    // DidFinishAttachment() to the AnnotationAgentHost.
    kActive,

    // When `this` has not found the text fragment and has dispatched
    // DidFinishAttachment(). This is a terminal case.
    kFailure,

    // When the search results are invalidated by another search client, or the
    // IPC is disconnected. This is a terminal case.
    kHighlightDropped,
  };

  friend std::ostream& operator<<(std::ostream& o, State state) {
    o << base::to_underlying(state);
    return o;
  }

  void SetState(State new_state);

  // Owns `this`.
  const raw_ptr<Container> container_;

  State state_ = State::kInitial;

  mojo::Remote<blink::mojom::AnnotationAgentHost> agent_host_;
  mojo::Receiver<blink::mojom::AnnotationAgent> receiver_{this};

  base::WeakPtrFactory<PdfAnnotationAgent> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ANNOTATION_AGENT_H_
