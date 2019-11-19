// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_ARRAY_H_

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRInputSourceArray : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  unsigned length() const { return input_sources_.size(); }
  XRInputSource* AnonymousIndexedGetter(unsigned index) const;

  XRInputSource* operator[](unsigned index) const;

  XRInputSource* GetWithSourceId(uint32_t source_id);
  void RemoveWithSourceId(uint32_t source_id);
  void SetWithSourceId(uint32_t source_id, XRInputSource* input_source);

  void Trace(blink::Visitor*) override;

 private:
  HeapHashMap<uint32_t, Member<XRInputSource>> input_sources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_ARRAY_H_
