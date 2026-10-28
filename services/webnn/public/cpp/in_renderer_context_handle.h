// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_CONTEXT_HANDLE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_CONTEXT_HANDLE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

// A thread-safe side channel letting Blink's `MLContext` invoke
// `WebNNContext` operations directly on the in-renderer backend's owning task
// runner, bypassing mojo.
//
// Mojo delivery isn't FIFO-ordered with raw `PostTask`s to the same task
// runner, so once `writeTensor`/`readTensor` bypass mojo (see
// `InRendererTensorHandle`), every op ordered relative to them
// (write/read/dispatch/destroy) must bypass it too, to preserve program
// order. Ops only issued after observing a mojo reply (`CreateTensor`,
// `CreateGraph`) are already causally ordered and stay on mojo.
class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) InRendererContextHandle
    : public base::RefCountedDeleteOnSequence<InRendererContextHandle> {
 public:
  virtual void Dispatch(
      const blink::WebNNGraphToken& graph_token,
      std::vector<std::pair<std::string, blink::WebNNTensorToken>> named_inputs,
      std::vector<std::pair<std::string, blink::WebNNTensorToken>>
          named_outputs) = 0;

  virtual void DestroyGraph(const blink::WebNNGraphToken& graph_token) = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<InRendererContextHandle>;
  friend class base::DeleteHelper<InRendererContextHandle>;

  explicit InRendererContextHandle(
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner);
  virtual ~InRendererContextHandle();
};

// Process-wide registry mapping `blink::WebNNContextToken` to the fast-path
// context handle exposed by the in-renderer backend. Populated by the
// service side at context creation time, looked up by Blink after
// `CreateWebNNContext` completes. All methods are thread-safe.
class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) InRendererContextRegistry {
 public:
  static void Register(const blink::WebNNContextToken& token,
                       scoped_refptr<InRendererContextHandle> handle);
  static void Unregister(const blink::WebNNContextToken& token);
  static scoped_refptr<InRendererContextHandle> Get(
      const blink::WebNNContextToken& token);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_IN_RENDERER_CONTEXT_HANDLE_H_
