// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

FencedFrameMPArchDelegate::FencedFrameMPArchDelegate(
    HTMLFencedFrameElement* outer_element)
    : HTMLFencedFrameElement::FencedFrameDelegate(outer_element),
      remote_(GetElement().GetDocument().GetExecutionContext()) {
  DocumentFencedFrames::GetOrCreate(GetElement().GetDocument())
      .RegisterFencedFrame(&GetElement());
  mojo::PendingAssociatedRemote<mojom::blink::FencedFrameOwnerHost> remote;
  mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost> receiver =
      remote.InitWithNewEndpointAndPassReceiver();
  auto task_runner =
      GetElement().GetDocument().GetTaskRunner(TaskType::kInternalDefault);
  remote_.Bind(std::move(remote), task_runner);

  RemoteFrame* remote_frame =
      GetElement().GetDocument().GetFrame()->Client()->CreateFencedFrame(
          &GetElement(), std::move(receiver));
  DCHECK_EQ(remote_frame, GetElement().ContentFrame());
}

void FencedFrameMPArchDelegate::Navigate(
    const KURL& url,
    const String& embedder_shared_storage_context) {
  DCHECK(remote_.get());
  const auto navigation_start_time = base::TimeTicks::Now();
  remote_->Navigate(url, navigation_start_time,
                    embedder_shared_storage_context);
}

void FencedFrameMPArchDelegate::Dispose() {
  DCHECK(remote_.get());
  remote_.reset();
  auto* fenced_frames = DocumentFencedFrames::Get(GetElement().GetDocument());
  DCHECK(fenced_frames);
  fenced_frames->DeregisterFencedFrame(&GetElement());
}

void FencedFrameMPArchDelegate::AttachLayoutTree() {
  if (GetElement().GetLayoutEmbeddedContent() && GetElement().ContentFrame()) {
    GetElement().SetEmbeddedContentView(GetElement().ContentFrame()->View());
  }
}

bool FencedFrameMPArchDelegate::SupportsFocus() {
  return true;
}

void FencedFrameMPArchDelegate::MarkFrozenFrameSizeStale() {
  RemoteFrameView* view =
      DynamicTo<RemoteFrameView>(GetElement().OwnedEmbeddedContentView());
  if (view) {
    view->ResetFrozenSize();
  }
  if (auto* layout_object = GetElement().GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        "Froze fenced frame content size");
  }
}

void FencedFrameMPArchDelegate::MarkContainerSizeStale() {
  if (auto* layout_object = GetElement().GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        "Stored fenced frame container size");
  }
}

void FencedFrameMPArchDelegate::DidChangeFramePolicy(
    const FramePolicy& frame_policy) {
  DCHECK(remote_.get());
  remote_->DidChangeFramePolicy(frame_policy);
}

void FencedFrameMPArchDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  HTMLFencedFrameElement::FencedFrameDelegate::Trace(visitor);
}

}  // namespace blink
