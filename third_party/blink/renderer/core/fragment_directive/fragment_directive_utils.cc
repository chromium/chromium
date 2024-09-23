// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"

#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

// static
void FragmentDirectiveUtils::RemoveSelectorsFromUrl(LocalFrame* frame) {
  auto* document_loader = frame->Loader().GetDocumentLoader();

  // This method can be called while the receiving frame is partway through a
  // load which hasn't committed yet. If that happens, the frame might not have
  // a DocumentLoader yet, or might not have created a HistoryItem. Either way,
  // it's not safe to continue and in any case there's no URL to remove
  // selectors from (yet), so do nothing.
  if (!document_loader || !document_loader->GetHistoryItem()) {
    return;
  }

  KURL url(shared_highlighting::RemoveFragmentSelectorDirectives(
      GURL(document_loader->GetHistoryItem()->Url())));

  // Replace the current history entry with the new url, so that the text
  // fragment shown in the URL matches the state of the highlight on the page.
  // This is equivalent to history.replaceState in javascript.
  frame->DomWindow()->document()->Loader()->RunURLAndHistoryUpdateSteps(
      url, nullptr, mojom::blink::SameDocumentNavigationType::kFragment,
      /*data=*/nullptr, WebFrameLoadType::kReplaceCurrentItem,
      FirePopstate::kYes);
}

}  // namespace blink
