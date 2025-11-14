// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// static
DocumentFencedFrames* DocumentFencedFrames::Get(Document& document) {
  return document.GetDocumentFencedFrames();
}

// static
DocumentFencedFrames& DocumentFencedFrames::GetOrCreate(Document& document) {
  DocumentFencedFrames* supplement = document.GetDocumentFencedFrames();
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentFencedFrames>(document);
    document.SetDocumentFencedFrames(supplement);
  }
  return *supplement;
}

DocumentFencedFrames::DocumentFencedFrames(Document& document)
    : document_(document) {}

void DocumentFencedFrames::RegisterFencedFrame(
    HTMLFencedFrameElement* fenced_frame) {
  fenced_frames_.push_back(fenced_frame);

  LocalFrame* frame = document_->GetFrame();
  if (!frame)
    return;
  if (Page* page = frame->GetPage())
    page->IncrementSubframeCount();
}

void DocumentFencedFrames::DeregisterFencedFrame(
    HTMLFencedFrameElement* fenced_frame) {
  wtf_size_t index = fenced_frames_.Find(fenced_frame);
  if (index != kNotFound) {
    fenced_frames_.EraseAt(index);
  }

  LocalFrame* frame = document_->GetFrame();
  if (!frame)
    return;
  if (Page* page = frame->GetPage()) {
    page->DecrementSubframeCount();
  }
}

void DocumentFencedFrames::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(fenced_frames_);
}

}  // namespace blink
