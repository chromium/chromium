/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/radio_button_group_scope.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class RadioButtonGroup : public GarbageCollected<RadioButtonGroup> {
 public:
  RadioButtonGroup();

  bool IsEmpty() const { return members_.IsEmpty(); }
  bool IsRequired() const { return required_count_; }
  HTMLInputElement* CheckedButton() const { return checked_button_; }
  void Add(HTMLInputElement*);
  void UpdateCheckedState(HTMLInputElement*);
  void RequiredAttributeChanged(HTMLInputElement*);
  void Remove(HTMLInputElement*);
  bool Contains(HTMLInputElement*) const;
  unsigned size() const;

  void Trace(Visitor*);

 private:
  void SetNeedsValidityCheckForAllButtons();
  bool IsValid() const;
  void SetCheckedButton(HTMLInputElement*);

  // The map records the 'required' state of each (button) element.
  using Members = HeapHashMap<Member<HTMLInputElement>, bool>;

  using MemberKeyValue = WTF::KeyValuePair<Member<HTMLInputElement>, bool>;

  void UpdateRequiredButton(MemberKeyValue&, bool is_required);

  Members members_;
  Member<HTMLInputElement> checked_button_;
  size_t required_count_;
};

RadioButtonGroup::RadioButtonGroup()
    : checked_button_(nullptr), required_count_(0) {}

inline bool RadioButtonGroup::IsValid() const {
  return !IsRequired() || checked_button_;
}

void RadioButtonGroup::SetCheckedButton(HTMLInputElement* button) {
  HTMLInputElement* old_checked_button = checked_button_;
  if (old_checked_button == button)
    return;
  checked_button_ = button;
  if (old_checked_button)
    old_checked_button->setChecked(false);
}

void RadioButtonGroup::UpdateRequiredButton(MemberKeyValue& it,
                                            bool is_required) {
  if (it.value == is_required)
    return;

  it.value = is_required;
  if (is_required) {
    required_count_++;
  } else {
    DCHECK_GT(required_count_, 0u);
    required_count_--;
  }
}

void RadioButtonGroup::Add(HTMLInputElement* button) {
  DCHECK_EQ(button->type(), input_type_names::kRadio);
  auto add_result = members_.insert(button, false);
  if (!add_result.is_new_entry)
    return;
  bool group_was_valid = IsValid();
  UpdateRequiredButton(*add_result.stored_value, button->IsRequired());
  if (button->checked())
    SetCheckedButton(button);

  bool group_is_valid = IsValid();
  if (group_was_valid != group_is_valid) {
    SetNeedsValidityCheckForAllButtons();
  } else if (!group_is_valid) {
    // A radio button not in a group is always valid. We need to make it
    // invalid only if the group is invalid.
    button->SetNeedsValidityCheck();
  }
}

void RadioButtonGroup::UpdateCheckedState(HTMLInputElement* button) {
  DCHECK_EQ(button->type(), input_type_names::kRadio);
  DCHECK(members_.Contains(button));
  bool was_valid = IsValid();
  if (button->checked()) {
    SetCheckedButton(button);
  } else {
    if (checked_button_ == button)
      checked_button_ = nullptr;
  }
  if (was_valid != IsValid())
    SetNeedsValidityCheckForAllButtons();
  for (auto& member : members_) {
    HTMLInputElement* const input_element = member.key;
    input_element->PseudoStateChanged(CSSSelector::kPseudoIndeterminate);
  }
}

void RadioButtonGroup::RequiredAttributeChanged(HTMLInputElement* button) {
  DCHECK_EQ(button->type(), input_type_names::kRadio);
  auto it = members_.find(button);
  DCHECK_NE(it, members_.end());
  bool was_valid = IsValid();
  // Synchronize the 'required' flag for the button, along with
  // updating the overall count.
  UpdateRequiredButton(*it, button->IsRequired());
  if (was_valid != IsValid())
    SetNeedsValidityCheckForAllButtons();
}

void RadioButtonGroup::Remove(HTMLInputElement* button) {
  DCHECK_EQ(button->type(), input_type_names::kRadio);
  auto it = members_.find(button);
  if (it == members_.end())
    return;
  bool was_valid = IsValid();
  DCHECK_EQ(it->value, button->IsRequired());
  UpdateRequiredButton(*it, false);
  members_.erase(it);
  if (checked_button_ == button)
    checked_button_ = nullptr;

  if (members_.IsEmpty()) {
    DCHECK(!required_count_);
    DCHECK(!checked_button_);
  } else if (was_valid != IsValid()) {
    SetNeedsValidityCheckForAllButtons();
  }
  if (!was_valid) {
    // A radio button not in a group is always valid. We need to make it
    // valid only if the group was invalid.
    button->SetNeedsValidityCheck();
  }

  // Send notification to update AX attributes for AXObjects which radiobutton
  // group has.
  if (!members_.IsEmpty()) {
    HTMLInputElement* input = members_.begin()->key;
    if (AXObjectCache* cache = input->GetDocument().ExistingAXObjectCache())
      cache->RadiobuttonRemovedFromGroup(input);
  }
}

void RadioButtonGroup::SetNeedsValidityCheckForAllButtons() {
  for (auto& element : members_) {
    HTMLInputElement* const button = element.key;
    DCHECK_EQ(button->type(), input_type_names::kRadio);
    button->SetNeedsValidityCheck();
  }
}

bool RadioButtonGroup::Contains(HTMLInputElement* button) const {
  return members_.Contains(button);
}

unsigned RadioButtonGroup::size() const {
  return members_.size();
}

void RadioButtonGroup::Trace(Visitor* visitor) {
  visitor->Trace(members_);
  visitor->Trace(checked_button_);
}

// ----------------------------------------------------------------

// Explicity define empty constructor and destructor in order to prevent the
// compiler from generating them as inlines. So we don't need to to define
// RadioButtonGroup in the header.
RadioButtonGroupScope::RadioButtonGroupScope() = default;

RadioButtonGroupScope::~RadioButtonGroupScope() = default;

void RadioButtonGroupScope::AddButton(HTMLInputElement* element) {
  DCHECK_EQ(element->type(), input_type_names::kRadio);
  if (element->GetName().IsEmpty())
    return;

  if (!name_to_group_map_)
    name_to_group_map_ = MakeGarbageCollected<NameToGroupMap>();

  auto* key_value =
      name_to_group_map_->insert(element->GetName(), nullptr).stored_value;
  if (!key_value->value)
    key_value->value = MakeGarbageCollected<RadioButtonGroup>();
  key_value->value->Add(element);
}

void RadioButtonGroupScope::UpdateCheckedState(HTMLInputElement* element) {
  DCHECK_EQ(element->type(), input_type_names::kRadio);
  if (element->GetName().IsEmpty())
    return;
  DCHECK(name_to_group_map_);
  if (!name_to_group_map_)
    return;
  RadioButtonGroup* group = name_to_group_map_->at(element->GetName());
  DCHECK(group);
  group->UpdateCheckedState(element);
}

void RadioButtonGroupScope::RequiredAttributeChanged(
    HTMLInputElement* element) {
  DCHECK_EQ(element->type(), input_type_names::kRadio);
  if (element->GetName().IsEmpty())
    return;
  DCHECK(name_to_group_map_);
  if (!name_to_group_map_)
    return;
  RadioButtonGroup* group = name_to_group_map_->at(element->GetName());
  DCHECK(group);
  group->RequiredAttributeChanged(element);
}

HTMLInputElement* RadioButtonGroupScope::CheckedButtonForGroup(
    const AtomicString& name) const {
  if (!name_to_group_map_)
    return nullptr;
  RadioButtonGroup* group = name_to_group_map_->at(name);
  return group ? group->CheckedButton() : nullptr;
}

bool RadioButtonGroupScope::IsInRequiredGroup(HTMLInputElement* element) const {
  DCHECK_EQ(element->type(), input_type_names::kRadio);
  if (element->GetName().IsEmpty())
    return false;
  if (!name_to_group_map_)
    return false;
  RadioButtonGroup* group = name_to_group_map_->at(element->GetName());
  return group && group->IsRequired() && group->Contains(element);
}

unsigned RadioButtonGroupScope::GroupSizeFor(
    const HTMLInputElement* element) const {
  if (!name_to_group_map_)
    return 0;

  RadioButtonGroup* group = name_to_group_map_->at(element->GetName());
  if (!group)
    return 0;
  return group->size();
}

void RadioButtonGroupScope::RemoveButton(HTMLInputElement* element) {
  DCHECK_EQ(element->type(), input_type_names::kRadio);
  if (element->GetName().IsEmpty())
    return;
  if (!name_to_group_map_)
    return;

  RadioButtonGroup* group = name_to_group_map_->at(element->GetName());
  if (!group)
    return;
  group->Remove(element);
  if (group->IsEmpty()) {
    // We don't remove an empty RadioButtonGroup from name_to_group_map_ for
    // better performance.
    DCHECK(!group->IsRequired());
    SECURITY_DCHECK(!group->CheckedButton());
  }
}

void RadioButtonGroupScope::Trace(Visitor* visitor) {
  visitor->Trace(name_to_group_map_);
}

}  // namespace blink
