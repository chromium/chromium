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
    DataView input,
    blink::FrameToken* output) {
  switch (input.tag()) {
    case DataView::Tag::kLocalFrameToken: {
      blink::LocalFrameToken token;
      bool ret = input.ReadLocalFrameToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kRemoteFrameToken: {
      blink::RemoteFrameToken token;
      bool ret = input.ReadRemoteFrameToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WORKER TOKENS

//////////////
// WorkerToken

// static
bool UnionTraits<blink::mojom::WorkerTokenDataView, blink::WorkerToken>::Read(
    DataView input,
    blink::WorkerToken* output) {
  switch (input.tag()) {
    case DataView::Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kServiceWorkerToken: {
      blink::ServiceWorkerToken token;
      bool ret = input.ReadServiceWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kSharedWorkerToken: {
      blink::SharedWorkerToken token;
      bool ret = input.ReadSharedWorkerToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WORKLET TOKENS

//////////////
// WorkletToken

// static
bool UnionTraits<blink::mojom::WorkletTokenDataView, blink::WorkletToken>::Read(
    DataView input,
    blink::WorkletToken* output) {
  switch (input.tag()) {
    case DataView::Tag::kAnimationWorkletToken: {
      blink::AnimationWorkletToken token;
      bool ret = input.ReadAnimationWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kAudioWorkletToken: {
      blink::AudioWorkletToken token;
      bool ret = input.ReadAudioWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kLayoutWorkletToken: {
      blink::LayoutWorkletToken token;
      bool ret = input.ReadLayoutWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kPaintWorkletToken: {
      blink::PaintWorkletToken token;
      bool ret = input.ReadPaintWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kSharedStorageWorkletToken: {
      blink::SharedStorageWorkletToken token;
      bool ret = input.ReadSharedStorageWorkletToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
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
bool UnionTraits<
    blink::mojom::ExecutionContextTokenDataView,
    blink::ExecutionContextToken>::Read(DataView input,
                                        blink::ExecutionContextToken* output) {
  switch (input.tag()) {
    case DataView::Tag::kLocalFrameToken: {
      blink::LocalFrameToken token;
      bool ret = input.ReadLocalFrameToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kServiceWorkerToken: {
      blink::ServiceWorkerToken token;
      bool ret = input.ReadServiceWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kSharedWorkerToken: {
      blink::SharedWorkerToken token;
      bool ret = input.ReadSharedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kAnimationWorkletToken: {
      blink::AnimationWorkletToken token;
      bool ret = input.ReadAnimationWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kAudioWorkletToken: {
      blink::AudioWorkletToken token;
      bool ret = input.ReadAudioWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kLayoutWorkletToken: {
      blink::LayoutWorkletToken token;
      bool ret = input.ReadLayoutWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kPaintWorkletToken: {
      blink::PaintWorkletToken token;
      bool ret = input.ReadPaintWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kSharedStorageWorkletToken: {
      blink::SharedStorageWorkletToken token;
      bool ret = input.ReadSharedStorageWorkletToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kShadowRealmToken: {
      blink::ShadowRealmToken token;
      bool ret = input.ReadShadowRealmToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

// static
bool UnionTraits<blink::mojom::WebGPUExecutionContextTokenDataView,
                 blink::WebGPUExecutionContextToken>::
    Read(DataView input, blink::WebGPUExecutionContextToken* output) {
  switch (input.tag()) {
    case DataView::Tag::kDocumentToken: {
      blink::DocumentToken token;
      bool ret = input.ReadDocumentToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kDedicatedWorkerToken: {
      blink::DedicatedWorkerToken token;
      bool ret = input.ReadDedicatedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kSharedWorkerToken: {
      blink::SharedWorkerToken token;
      bool ret = input.ReadSharedWorkerToken(&token);
      *output = token;
      return ret;
    }
    case DataView::Tag::kServiceWorkerToken: {
      blink::ServiceWorkerToken token;
      bool ret = input.ReadServiceWorkerToken(&token);
      *output = token;
      return ret;
    }
  }
  return false;
}

}  // namespace mojo
