// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_FRAGMENT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CustomLayoutFragmentRequest;
class LayoutBox;
class ScriptState;
class ScriptValue;

// This represents the result of a layout (on a LayoutChild).
//
// The result is stuck in time, e.g. performing another layout of the same
// LayoutChild will not live update the values of this fragment.
//
// The web developer can position this child fragment (setting inlineOffset,
// and blockOffset), which are relative to its parent.
//
// This should eventually mirror the information in a NGFragment, it has the
// additional capability that it is exposed to web developers.
class CustomLayoutFragment : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutFragment(CustomLayoutFragmentRequest*,
                       const LayoutUnit inline_size,
                       const LayoutUnit block_size,
                       v8::Isolate*);
  ~CustomLayoutFragment() override = default;

  double inlineSize() const { return inline_size_; }
  double blockSize() const { return block_size_; }

  double inlineOffset() const { return inline_offset_; }
  double blockOffset() const { return block_offset_; }

  void setInlineOffset(double inline_offset) { inline_offset_ = inline_offset; }
  void setBlockOffset(double block_offset) { block_offset_ = block_offset; }

  ScriptValue data(ScriptState*) const;

  LayoutBox* GetLayoutBox() const;
  bool IsValid() const;

  void Trace(blink::Visitor*) override;

 private:
  // There is complexity around state in the layout tree, e.g. from the web
  // developers perspective:
  //
  // const fragment1 = yield child.layoutNextFragment(options1);
  // const fragment2 = yield child.layoutNextFragment(options2);
  // return { childFragments: [fragment1] };
  //
  // In the above example the LayoutBox representing "child" has the incorrect
  // layout state. As we are processing the returned childFragments we detect
  // that the last layout on the child wasn't with the same inputs, and force a
  // layout again.
  Member<CustomLayoutFragmentRequest> fragment_request_;

  // The inline and block size on this object should never change.
  const double inline_size_;
  const double block_size_;

  // The offset is relative to our parent, and in the parent's writing mode.
  double inline_offset_ = 0;
  double block_offset_ = 0;

  TraceWrapperV8Reference<v8::Value> layout_worklet_world_v8_data_;

  DISALLOW_COPY_AND_ASSIGN(CustomLayoutFragment);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_CUSTOM_LAYOUT_FRAGMENT_H_
