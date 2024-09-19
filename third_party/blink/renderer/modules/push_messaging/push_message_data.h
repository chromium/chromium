// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGE_DATA_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class ExceptionState;
class ScriptState;
class V8UnionArrayBufferOrArrayBufferViewOrUSVString;

class MODULES_EXPORT PushMessageData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushMessageData* Create(const String& data);
  static PushMessageData* Create(
      const V8UnionArrayBufferOrArrayBufferViewOrUSVString* data);

  explicit PushMessageData(base::span<const uint8_t> data);
  ~PushMessageData() override;

  DOMArrayBuffer* arrayBuffer() const;
  Blob* blob() const;
  ScriptValue json(ScriptState* script_state,
                   ExceptionState& exception_state) const;
  String text() const;

 private:
  Vector<uint8_t> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGE_DATA_H_
