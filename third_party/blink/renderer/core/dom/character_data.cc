/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All
 * rights reserved.
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
 */

#include "third_party/blink/renderer/core/dom/character_data.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/text_diff_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/mutation_event.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void CharacterData::MakeParkable() {
  if (is_parkable_) {
    return;
  }

  auto released = data_.ReleaseImpl();
  data_.~String();
  new (&parkable_data_) ParkableString(std::move(released));
  is_parkable_ = true;
}

void CharacterData::setData(const String& data) {
  unsigned old_length = length();

  SetDataAndUpdate(data, TextDiffRange::Replace(0, old_length, data.length()),
                   kUpdateFromNonParser);
  GetDocument().DidRemoveText(*this, 0, old_length);
}

String CharacterData::substringData(unsigned offset,
                                    unsigned count,
                                    ExceptionState& exception_state) {
  if (offset > length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The offset " + String::Number(offset) +
            " is greater than the node's length (" + String::Number(length()) +
            ").");
    return String();
  }

  return data().Substring(offset, count);
}

void CharacterData::ParserAppendData(const String& data) {
  String new_str = this->data() + data;

  SetDataAndUpdate(new_str,
                   TextDiffRange::Insert(this->data().length(), data.length()),
                   kUpdateFromParser);
}

void CharacterData::appendData(const String& data) {
  String new_str = this->data() + data;

  SetDataAndUpdate(new_str,
                   TextDiffRange::Insert(this->data().length(), data.length()),
                   kUpdateFromNonParser);

  // FIXME: Should we call textInserted here?
}

void CharacterData::insertData(unsigned offset,
                               const String& data,
                               ExceptionState& exception_state) {
  if (offset > length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The offset " + String::Number(offset) +
            " is greater than the node's length (" + String::Number(length()) +
            ").");
    return;
  }

  String current_data = this->data();
  StringBuilder new_str;
  new_str.ReserveCapacity(data.length() + current_data.length());
  new_str.Append(StringView(current_data, 0, offset));
  new_str.Append(data);
  new_str.Append(StringView(current_data, offset));

  SetDataAndUpdate(new_str.ReleaseString(),
                   TextDiffRange::Insert(offset, data.length()),
                   kUpdateFromNonParser);

  GetDocument().DidInsertText(*this, offset, data.length());
}

static bool ValidateOffsetCount(unsigned offset,
                                unsigned count,
                                unsigned length,
                                unsigned& real_count,
                                ExceptionState& exception_state) {
  if (offset > length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The offset " + String::Number(offset) +
            " is greater than the node's length (" + String::Number(length) +
            ").");
    return false;
  }

  base::CheckedNumeric<unsigned> offset_count = offset;
  offset_count += count;

  if (!offset_count.IsValid() || offset + count > length)
    real_count = length - offset;
  else
    real_count = count;

  return true;
}

void CharacterData::deleteData(unsigned offset,
                               unsigned count,
                               ExceptionState& exception_state) {
  unsigned real_count = 0;
  if (!ValidateOffsetCount(offset, count, length(), real_count,
                           exception_state))
    return;

  String current_data = this->data();
  StringBuilder new_str;
  new_str.ReserveCapacity(current_data.length() - real_count);
  new_str.Append(StringView(current_data, 0, offset));
  new_str.Append(StringView(current_data, offset + real_count));
  SetDataAndUpdate(new_str.ReleaseString(),
                   TextDiffRange::Delete(offset, real_count),
                   kUpdateFromNonParser);

  GetDocument().DidRemoveText(*this, offset, real_count);
}

void CharacterData::replaceData(unsigned offset,
                                unsigned count,
                                const String& data,
                                ExceptionState& exception_state) {
  unsigned real_count = 0;
  if (!ValidateOffsetCount(offset, count, length(), real_count,
                           exception_state))
    return;

  String current_data = this->data();
  StringBuilder new_str;
  new_str.ReserveCapacity(data.length() + current_data.length() - real_count);
  new_str.Append(StringView(current_data, 0, offset));
  new_str.Append(data);
  new_str.Append(StringView(current_data, offset + real_count));

  SetDataAndUpdate(new_str.ReleaseString(),
                   TextDiffRange::Replace(offset, real_count, data.length()),
                   kUpdateFromNonParser);

  // update DOM ranges
  GetDocument().DidRemoveText(*this, offset, real_count);
  GetDocument().DidInsertText(*this, offset, data.length());
}

String CharacterData::nodeValue() const {
  return data();
}

bool CharacterData::ContainsOnlyWhitespaceOrEmpty() const {
  return data().ContainsOnlyWhitespaceOrEmpty();
}

void CharacterData::setNodeValue(const String& node_value, ExceptionState&) {
  setData(!node_value.IsNull() ? node_value : g_empty_string);
}

void CharacterData::SetDataAndUpdate(const String& new_data,
                                     const TextDiffRange& diff,
                                     UpdateSource source) {
  String old_data = this->data();
  diff.CheckValid(old_data, new_data);
  SetDataWithoutUpdate(new_data);

  DCHECK(!GetLayoutObject() || IsTextNode());
  if (auto* text_node = DynamicTo<Text>(this))
    text_node->UpdateTextLayoutObject(diff);

  if (source != kUpdateFromParser) {
    if (auto* processing_instruction_node =
            DynamicTo<ProcessingInstruction>(this))
      processing_instruction_node->DidAttributeChanged();

    GetDocument().NotifyUpdateCharacterData(this, diff);
  }

  GetDocument().IncDOMTreeVersion();
  DidModifyData(old_data, source);
}

void CharacterData::DidModifyData(const String& old_data, UpdateSource source) {
  if (MutationObserverInterestGroup* mutation_recipients =
          MutationObserverInterestGroup::CreateForCharacterDataMutation(*this))
    mutation_recipients->EnqueueMutationRecord(
        MutationRecord::CreateCharacterData(this, old_data));

  if (parentNode()) {
    ContainerNode::ChildrenChange change = {
        .type = ContainerNode::ChildrenChangeType::kTextChanged,
        .by_parser = source == kUpdateFromParser
                         ? ContainerNode::ChildrenChangeSource::kParser
                         : ContainerNode::ChildrenChangeSource::kAPI,
        .affects_elements = ContainerNode::ChildrenChangeAffectsElements::kNo,
        .sibling_changed = this,
        .sibling_before_change = previousSibling(),
        .sibling_after_change = nextSibling(),
        .old_text = &old_data};
    parentNode()->ChildrenChanged(change);
  }

  // Skip DOM mutation events if the modification is from parser.
  // Note that mutation observer events will still fire.
  // Spec: https://html.spec.whatwg.org/C/#insert-a-character
  if (source != kUpdateFromParser && !IsInShadowTree() &&
      !GetDocument().ShouldSuppressMutationEvents()) {
    if (GetDocument().HasListenerType(
            Document::kDOMCharacterDataModifiedListener)) {
      DispatchScopedEvent(*MutationEvent::Create(
          event_type_names::kDOMCharacterDataModified, Event::Bubbles::kYes,
          nullptr, old_data, data()));
    }
    DispatchSubtreeModifiedEvent();
  }
  probe::CharacterDataModified(this);
}

Node* CharacterData::Clone(Document& factory,
                           NodeCloningData& cloning_data,
                           ContainerNode* append_to,
                           ExceptionState& append_exception_state) const {
  CharacterData* clone = CloneWithData(factory, data());
  if (cloning_data.Has(CloneOption::kPreserveDOMPartsMinimalAPI) &&
      HasNodePart()) {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
    clone->SetHasNodePart();
  } else if (cloning_data.Has(CloneOption::kPreserveDOMParts)) {
    PartRoot::CloneParts(*this, *clone, cloning_data);
  }
  if (append_to) {
    append_to->AppendChild(clone, append_exception_state);
  }
  return clone;
}

}  // namespace blink
