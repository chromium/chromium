// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RADIO_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RADIO_INPUT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

namespace blink {

class AXObjectCacheImpl;
class HTMLInputElement;

class AXRadioInput final : public AXLayoutObject {
 public:
  AXRadioInput(LayoutObject*, AXObjectCacheImpl&);
  ~AXRadioInput() override = default;

  bool IsAXRadioInput() const override { return true; }
  void UpdatePosAndSetSize(int position = 0);
  void RequestUpdateToNextNode(bool forward);
  HTMLInputElement* FindFirstRadioButtonInGroup(
      HTMLInputElement* current) const;

  int PosInSet() const final;
  int SetSize() const final;

 private:
  bool CalculatePosInSet();
  int CountFromFirstElement() const;
  HTMLInputElement* GetInputElement() const;
  int SizeOfRadioGroup() const;

  int pos_in_set_;
  int set_size_;

  DISALLOW_COPY_AND_ASSIGN(AXRadioInput);
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXRadioInput, IsAXRadioInput());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RADIO_INPUT_H_
