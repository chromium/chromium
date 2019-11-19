/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/page/page_popup_supplement.h"

#include "third_party/blink/renderer/core/page/page_popup_controller.h"

namespace blink {

PagePopupSupplement::PagePopupSupplement(LocalFrame& frame,
                                         PagePopup& popup,
                                         PagePopupClient* popup_client)
    : Supplement<LocalFrame>(frame),
      controller_(
          MakeGarbageCollected<PagePopupController>(popup, popup_client)) {
  DCHECK(popup_client);
}

const char PagePopupSupplement::kSupplementName[] = "PagePopupSupplement";

PagePopupSupplement& PagePopupSupplement::From(LocalFrame& frame) {
  PagePopupSupplement* supplement =
      Supplement<LocalFrame>::From<PagePopupSupplement>(&frame);
  DCHECK(supplement);
  return *supplement;
}

PagePopupController* PagePopupSupplement::GetPagePopupController() const {
  return controller_;
}

void PagePopupSupplement::Dispose() {
  controller_->ClearPagePopupClient();
}

void PagePopupSupplement::Install(LocalFrame& frame,
                                  PagePopup& popup,
                                  PagePopupClient* popup_client) {
  DCHECK(popup_client);
  ProvideTo(frame, MakeGarbageCollected<PagePopupSupplement>(frame, popup,
                                                             popup_client));
}

void PagePopupSupplement::Uninstall(LocalFrame& frame) {
  PagePopupSupplement::From(frame).Dispose();
  frame.RemoveSupplement<PagePopupSupplement>();
}

void PagePopupSupplement::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  Supplement<LocalFrame>::Trace(visitor);
}

}  // namespace blink
