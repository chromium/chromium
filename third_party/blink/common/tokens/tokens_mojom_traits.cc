// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// FRAME TOKENS

/////////////
// FrameToken

// static
bool UnionTraits<blink::mojom::FrameTokenDataView, blink::FrameToken>::Read(
    blink::mojom::FrameTokenDataView input,
    blink::FrameToken* output) {
  using Tag = blink::mojom::FrameTokenDataView::Tag;
  switch (input.tag()) {
    case Tag::kLocalFrameToken: {
      blink::LocalFrameToken token;
      bool ret = input.ReadLocalFrameToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kRemoteFrameToken: {
      blink::RemoteFrameToken token;
      bool ret = input.ReadRemoteFrameToken(&token);
      *output = token;
      return ret;
    }
  }
}

// static
blink::mojom::FrameTokenDataView::Tag
UnionTraits<blink::mojom::FrameTokenDataView, blink::FrameToken>::GetTag(
    const blink::FrameToken& token) {
  using Tag = blink::mojom::FrameTokenDataView::Tag;
  if (token.Is<blink::LocalFrameToken>())
    return Tag::kLocalFrameToken;
  DCHECK(token.Is<blink::RemoteFrameToken>());
  return Tag::kRemoteFrameToken;
}

// static
blink::LocalFrameToken UnionTraits<
    blink::mojom::FrameTokenDataView,
    blink::FrameToken>::local_frame_token(const blink::FrameToken& token) {
  return token.GetAs<blink::LocalFrameToken>();
}

// static
blink::RemoteFrameToken UnionTraits<
    blink::mojom::FrameTokenDataView,
    blink::FrameToken>::remote_frame_token(const blink::FrameToken& token) {
  return token.GetAs<blink::RemoteFrameToken>();
}

////////////////////////////////////////////////////////////////////////////////
// WORKER TOKENS

//////////////
// WorkerToken

// static
bool UnionTraits<blink::mojom::WorkerTokenDataView, blink::WorkerToken>::Read(
    blink::mojom::WorkerTokenDataView input,
    blink::WorkerToken* output) {
  using Tag = blink::mojom::WorkerTokenDataView::Tag;
  switch (input.tag()) {
    case Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kServiceWorkerToken: {
      blink::ServiceWorkerToken token;
      bool ret = input.ReadServiceWorkerToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kSharedWorkerToken: {
      blink::SharedWorkerToken token;
      bool ret = input.ReadSharedWorkerToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

// static
blink::mojom::WorkerTokenDataView::Tag
UnionTraits<blink::mojom::WorkerTokenDataView, blink::WorkerToken>::GetTag(
    const blink::WorkerToken& token) {
  using Tag = blink::mojom::WorkerTokenDataView::Tag;
  if (token.Is<blink::DedicatedWorkerToken>())
    return Tag::kDedicatedWorkerToken;
  if (token.Is<blink::ServiceWorkerToken>())
    return Tag::kServiceWorkerToken;
  DCHECK(token.Is<blink::SharedWorkerToken>());
  return Tag::kSharedWorkerToken;
}

// static
blink::DedicatedWorkerToken
UnionTraits<blink::mojom::WorkerTokenDataView, blink::WorkerToken>::
    dedicated_worker_token(const blink::WorkerToken& token) {
  return token.GetAs<blink::DedicatedWorkerToken>();
}

// static
blink::ServiceWorkerToken UnionTraits<
    blink::mojom::WorkerTokenDataView,
    blink::WorkerToken>::service_worker_token(const blink::WorkerToken& token) {
  return token.GetAs<blink::ServiceWorkerToken>();
}

// static
blink::SharedWorkerToken UnionTraits<
    blink::mojom::WorkerTokenDataView,
    blink::WorkerToken>::shared_worker_token(const blink::WorkerToken& token) {
  return token.GetAs<blink::SharedWorkerToken>();
}

////////////////////////////////////////////////////////////////////////////////
// WORKLET TOKENS

//////////////
// WorkletToken

// static
bool UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::Read(
    blink::mojom::WorkletTokenDataView input,
    blink::WorkletToken* output) {
  using Tag = blink::mojom::WorkletTokenDataView::Tag;
  switch (input.tag()) {
    case Tag::kAnimationWorkletToken: {
      blink::AnimationWorkletToken token;
      bool ret = input.ReadAnimationWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kAudioWorkletToken: {
      blink::AudioWorkletToken token;
      bool ret = input.ReadAudioWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kLayoutWorkletToken: {
      blink::LayoutWorkletToken token;
      bool ret = input.ReadLayoutWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kPaintWorkletToken: {
      blink::PaintWorkletToken token;
      bool ret = input.ReadPaintWorkletToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

// static
blink::mojom::WorkletTokenDataView::Tag
UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::GetTag(
    const blink::WorkletToken& token) {
  using Tag = blink::mojom::WorkletTokenDataView::Tag;
  if (token.Is<blink::AnimationWorkletToken>())
    return Tag::kAnimationWorkletToken;
  if (token.Is<blink::AudioWorkletToken>())
    return Tag::kAudioWorkletToken;
  if (token.Is<blink::LayoutWorkletToken>())
    return Tag::kLayoutWorkletToken;
  DCHECK(token.Is<blink::PaintWorkletToken>());
  return Tag::kPaintWorkletToken;
}

// static
blink::AnimationWorkletToken
UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::
    animation_worklet_token(const blink::WorkletToken& token) {
  return token.GetAs<blink::AnimationWorkletToken>();
}

// static
blink::AudioWorkletToken
UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::
    audio_worklet_token(const blink::WorkletToken& token) {
  return token.GetAs<blink::AudioWorkletToken>();
}

// static
blink::LayoutWorkletToken
UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::
    layout_worklet_token(const blink::WorkletToken& token) {
  return token.GetAs<blink::LayoutWorkletToken>();
}

// static
blink::PaintWorkletToken
UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::
    paint_worklet_token(const blink::WorkletToken& token) {
  return token.GetAs<blink::PaintWorkletToken>();
}

////////////////////////////////////////////////////////////////////////////////
// OTHER TOKENS
//
// Keep this section last.
//
// If you have multiple tokens that make a thematic group, please lift them to
// their own section, in alphabetical order. If adding a new token here, please
// keep the following list in alphabetic order.

///////////////////////////////////
// ExecutionContextToken

// static
bool UnionTraits<blink::mojom::ExecutionContextTokenDataView,
                 blink::ExecutionContextToken>::
    Read(blink::mojom::ExecutionContextTokenDataView input,
         blink::ExecutionContextToken* output) {
  using Tag = blink::mojom::ExecutionContextTokenDataView::Tag;
  switch (input.tag()) {
    case Tag::kLocalFrameToken: {
      blink::LocalFrameToken token;
      bool ret = input.ReadLocalFrameToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kServiceWorkerToken: {
      blink::ServiceWorkerToken token;
      bool ret = input.ReadServiceWorkerToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kSharedWorkerToken: {
      blink::SharedWorkerToken token;
      bool ret = input.ReadSharedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kAnimationWorkletToken: {
      blink::AnimationWorkletToken token;
      bool ret = input.ReadAnimationWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kAudioWorkletToken: {
      blink::AudioWorkletToken token;
      bool ret = input.ReadAudioWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kLayoutWorkletToken: {
      blink::LayoutWorkletToken token;
      bool ret = input.ReadLayoutWorkletToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kPaintWorkletToken: {
      blink::PaintWorkletToken token;
      bool ret = input.ReadPaintWorkletToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

// static
blink::mojom::ExecutionContextTokenDataView::Tag UnionTraits<
    blink::mojom::ExecutionContextTokenDataView,
    blink::ExecutionContextToken>::GetTag(const blink::ExecutionContextToken&
                                              token) {
  using Tag = blink::mojom::ExecutionContextTokenDataView::Tag;
  if (token.Is<blink::LocalFrameToken>())
    return Tag::kLocalFrameToken;
  if (token.Is<blink::DedicatedWorkerToken>())
    return Tag::kDedicatedWorkerToken;
  if (token.Is<blink::ServiceWorkerToken>())
    return Tag::kServiceWorkerToken;
  if (token.Is<blink::SharedWorkerToken>())
    return Tag::kSharedWorkerToken;
  if (token.Is<blink::AnimationWorkletToken>())
    return Tag::kAnimationWorkletToken;
  if (token.Is<blink::AudioWorkletToken>())
    return Tag::kAudioWorkletToken;
  if (token.Is<blink::LayoutWorkletToken>())
    return Tag::kLayoutWorkletToken;
  DCHECK(token.Is<blink::PaintWorkletToken>());
  return Tag::kPaintWorkletToken;
}

// static
blink::LocalFrameToken UnionTraits<blink::mojom::ExecutionContextTokenDataView,
                                   blink::ExecutionContextToken>::
    local_frame_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::LocalFrameToken>();
}

// static
blink::DedicatedWorkerToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    dedicated_worker_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::DedicatedWorkerToken>();
}

// static
blink::ServiceWorkerToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    service_worker_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::ServiceWorkerToken>();
}

// static
blink::SharedWorkerToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    shared_worker_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::SharedWorkerToken>();
}

// static
blink::AnimationWorkletToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    animation_worklet_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::AnimationWorkletToken>();
}

// static
blink::AudioWorkletToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    audio_worklet_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::AudioWorkletToken>();
}

// static
blink::LayoutWorkletToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    layout_worklet_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::LayoutWorkletToken>();
}

// static
blink::PaintWorkletToken
UnionTraits<blink::mojom::ExecutionContextTokenDataView,
            blink::ExecutionContextToken>::
    paint_worklet_token(const blink::ExecutionContextToken& token) {
  return token.GetAs<blink::PaintWorkletToken>();
}

// static
bool UnionTraits<blink::mojom::WebGPUExecutionContextTokenDataView,
                 blink::WebGPUExecutionContextToken>::
    Read(blink::mojom::WebGPUExecutionContextTokenDataView input,
         blink::WebGPUExecutionContextToken* output) {
  using Tag = blink::mojom::WebGPUExecutionContextTokenDataView::Tag;
  switch (input.tag()) {
    case Tag::kDocumentToken: {
      blink::DocumentToken token;
      bool ret = input.ReadDocumentToken(&token);
      *output = token;
      return ret;
    }
    case Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

// static
blink::mojom::WebGPUExecutionContextTokenDataView::Tag
UnionTraits<blink::mojom::WebGPUExecutionContextTokenDataView,
            blink::WebGPUExecutionContextToken>::
    GetTag(const blink::WebGPUExecutionContextToken& token) {
  using Tag = blink::mojom::WebGPUExecutionContextTokenDataView::Tag;
  if (token.Is<blink::DocumentToken>())
    return Tag::kDocumentToken;
  DCHECK(token.Is<blink::DedicatedWorkerToken>());
  return Tag::kDedicatedWorkerToken;
}

// static
blink::DocumentToken
UnionTraits<blink::mojom::WebGPUExecutionContextTokenDataView,
            blink::WebGPUExecutionContextToken>::
    document_token(const blink::WebGPUExecutionContextToken& token) {
  return token.GetAs<blink::DocumentToken>();
}

// static
blink::DedicatedWorkerToken
UnionTraits<blink::mojom::WebGPUExecutionContextTokenDataView,
            blink::WebGPUExecutionContextToken>::
    dedicated_worker_token(const blink::WebGPUExecutionContextToken& token) {
  return token.GetAs<blink::DedicatedWorkerToken>();
}

}  // namespace mojo
