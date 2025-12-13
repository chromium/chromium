// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_

namespace blink {
class ContainerNode;
class ExceptionState;
class ScriptState;
class WritableStream;

// This creates a Writable stream that takes string and inserts them into an
// existing element or shadow root.
class HTMLStream {
 public:
  static WritableStream* Create(ScriptState*,
                                ContainerNode* target,
                                ExceptionState&);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_STREAM_H_
