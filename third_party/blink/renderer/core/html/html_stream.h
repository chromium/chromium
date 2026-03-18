// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_

#include "base/functional/callback.h"

namespace blink {
class AtomicString;
class FragmentParserOptions;
class ContainerNode;
class ExceptionState;
class Node;
class ScriptState;
class WritableStream;

// This creates a Writable stream that takes string and inserts them into an
// existing element or shadow root.
class HTMLStream {
 public:
  static WritableStream* Create(ScriptState*,
                                ContainerNode* target,
                                Node* ref_node,
                                const FragmentParserOptions& options,
                                const AtomicString& interface_name,
                                const AtomicString& property_name,
                                ExceptionState&);

  template <typename T>
  static WritableStream* Create(ScriptState* script_state,
                                ContainerNode* target,
                                Node* ref_node,
                                const FragmentParserOptions& options,
                                const AtomicString& interface_name,
                                const AtomicString& property_name,
                                ExceptionState& exception_state,
                                T on_start) {
    auto* stream = Create(script_state, target, ref_node, options,
                          interface_name, property_name, exception_state);
    if (stream) {
      on_start();
    }
    return stream;
  }
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_
