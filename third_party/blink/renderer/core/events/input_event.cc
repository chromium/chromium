// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/input_event.h"

#include <algorithm>
#include <array>
#include <type_traits>

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/commands/editing_command_type.h"

namespace blink {

namespace {

struct InputTypeStringNameMapEntry {
  InputEvent::InputType input_type;
  const char* string_name;
};

const std::array<InputTypeStringNameMapEntry,
                 static_cast<size_t>(
                     InputEvent::InputType::kNumberOfInputTypes)>
    kInputTypeStringNameMap{{
        {InputEvent::InputType::kNone, ""},
        {InputEvent::InputType::kInsertText, "insertText"},
        {InputEvent::InputType::kInsertLineBreak, "insertLineBreak"},
        {InputEvent::InputType::kInsertParagraph, "insertParagraph"},
        {InputEvent::InputType::kInsertOrderedList, "insertOrderedList"},
        {InputEvent::InputType::kInsertUnorderedList, "insertUnorderedList"},
        {InputEvent::InputType::kInsertHorizontalRule, "insertHorizontalRule"},
        {InputEvent::InputType::kInsertFromPaste, "insertFromPaste"},
        {InputEvent::InputType::kInsertFromDrop, "insertFromDrop"},
        {InputEvent::InputType::kInsertFromYank, "insertFromYank"},
        {InputEvent::InputType::kInsertTranspose, "insertTranspose"},
        {InputEvent::InputType::kInsertReplacementText,
         "insertReplacementText"},
        {InputEvent::InputType::kInsertCompositionText,
         "insertCompositionText"},
        {InputEvent::InputType::kInsertLink, "insertLink"},
        {InputEvent::InputType::kDeleteWordBackward, "deleteWordBackward"},
        {InputEvent::InputType::kDeleteWordForward, "deleteWordForward"},
        {InputEvent::InputType::kDeleteSoftLineBackward,
         "deleteSoftLineBackward"},
        {InputEvent::InputType::kDeleteSoftLineForward,
         "deleteSoftLineForward"},
        {InputEvent::InputType::kDeleteHardLineBackward,
         "deleteHardLineBackward"},
        {InputEvent::InputType::kDeleteHardLineForward,
         "deleteHardLineForward"},
        {InputEvent::InputType::kDeleteContentBackward,
         "deleteContentBackward"},
        {InputEvent::InputType::kDeleteContentForward, "deleteContentForward"},
        {InputEvent::InputType::kDeleteByCut, "deleteByCut"},
        {InputEvent::InputType::kDeleteByDrag, "deleteByDrag"},
        {InputEvent::InputType::kHistoryUndo, "historyUndo"},
        {InputEvent::InputType::kHistoryRedo, "historyRedo"},
        {InputEvent::InputType::kFormatBold, "formatBold"},
        {InputEvent::InputType::kFormatItalic, "formatItalic"},
        {InputEvent::InputType::kFormatUnderline, "formatUnderline"},
        {InputEvent::InputType::kFormatStrikeThrough, "formatStrikeThrough"},
        {InputEvent::InputType::kFormatSuperscript, "formatSuperscript"},
        {InputEvent::InputType::kFormatSubscript, "formatSubscript"},
        {InputEvent::InputType::kFormatJustifyCenter, "formatJustifyCenter"},
        {InputEvent::InputType::kFormatJustifyFull, "formatJustifyFull"},
        {InputEvent::InputType::kFormatJustifyRight, "formatJustifyRight"},
        {InputEvent::InputType::kFormatJustifyLeft, "formatJustifyLeft"},
        {InputEvent::InputType::kFormatIndent, "formatIndent"},
        {InputEvent::InputType::kFormatOutdent, "formatOutdent"},
        {InputEvent::InputType::kFormatRemove, "formatRemove"},
        {InputEvent::InputType::kFormatSetBlockTextDirection,
         "formatSetBlockTextDirection"},
    }};

static_assert(
    std::size(kInputTypeStringNameMap) ==
        static_cast<size_t>(InputEvent::InputType::kNumberOfInputTypes),
    "must handle all InputEvent::InputType");

String ConvertInputTypeToString(InputEvent::InputType input_type) {
  using IntegerInputType = std::underlying_type_t<InputEvent::InputType>;
  const auto numeric_input_type = static_cast<IntegerInputType>(input_type);
  if (numeric_input_type >= 0 &&
      numeric_input_type <
          static_cast<IntegerInputType>(kInputTypeStringNameMap.size())) {
    return AtomicString(
        kInputTypeStringNameMap[numeric_input_type].string_name);
  }
  return g_empty_string;
}

InputEvent::InputType ConvertStringToInputType(const String& string_name) {
  // TODO(input-dev): Use binary search if the map goes larger.
  for (const auto& entry : kInputTypeStringNameMap) {
    if (string_name == entry.string_name)
      return entry.input_type;
  }
  return InputEvent::InputType::kNone;
}

bool InputTypeIsCancelable(InputEvent::InputType input_type) {
  return input_type != InputEvent::InputType::kInsertCompositionText;
}

}  // anonymous namespace

InputEvent::InputEvent(const AtomicString& type,
                       const InputEventInit* initializer)
    : UIEvent(type, initializer) {
  // TODO(ojan): We should find a way to prevent conversion like
  // String->enum->String just in order to use initializer.
  // See InputEvent::createBeforeInput() for the first conversion.
  if (initializer->hasInputType())
    input_type_ = ConvertStringToInputType(initializer->inputType());
  if (initializer->hasData())
    data_ = initializer->data();
  if (initializer->hasDataTransfer())
    data_transfer_ = initializer->dataTransfer();
  if (initializer->hasIsComposing())
    is_composing_ = initializer->isComposing();
  if (!initializer->hasTargetRanges())
    return;
  for (const auto& range : initializer->targetRanges())
    ranges_.push_back(range->toRange());
}

InputEvent::InputEvent(const AtomicString& type,
                       const UIEventInit& init,
                       InputType input_type,
                       const String& data,
                       DataTransfer* data_transfer,
                       EventIsComposing is_composing,
                       const StaticRangeVector* ranges)
    : UIEvent(type, &init),
      input_type_(input_type),
      data_(data),
      data_transfer_(data_transfer),
      is_composing_(is_composing == kIsComposing) {
  if (ranges) {
    for (const auto& range : *ranges) {
      ranges_.push_back(range->toRange());
    }
  }
}

/* static */
InputEvent* InputEvent::CreateBeforeInput(InputType input_type,
                                          const String& data,
                                          EventIsComposing is_composing,
                                          const StaticRangeVector* ranges) {
  auto* event_init = UIEventInit::Create();
  event_init->setBubbles(true);
  event_init->setCancelable(InputTypeIsCancelable(input_type));
  event_init->setComposed(true);
  return MakeGarbageCollected<InputEvent>(event_type_names::kBeforeinput,
                                          *event_init, input_type, data,
                                          nullptr, is_composing, ranges);
}

/* static */
InputEvent* InputEvent::CreateBeforeInput(InputType input_type,
                                          DataTransfer* data_transfer,
                                          EventIsComposing is_composing,
                                          const StaticRangeVector* ranges) {
  auto* event_init = UIEventInit::Create();
  event_init->setBubbles(true);
  event_init->setCancelable(InputTypeIsCancelable(input_type));
  event_init->setComposed(true);
  return MakeGarbageCollected<InputEvent>(event_type_names::kBeforeinput,
                                          *event_init, input_type, String(),
                                          data_transfer, is_composing, ranges);
}

/* static */
InputEvent* InputEvent::CreateInput(InputType input_type,
                                    const String& data,
                                    EventIsComposing is_composing,
                                    const StaticRangeVector* ranges) {
  auto* event_init = UIEventInit::Create();
  event_init->setBubbles(true);
  event_init->setCancelable(false);
  event_init->setComposed(true);
  return MakeGarbageCollected<InputEvent>(event_type_names::kInput, *event_init,
                                          input_type, data, nullptr,
                                          is_composing, ranges);
}

String InputEvent::inputType() const {
  return ConvertInputTypeToString(input_type_);
}

StaticRangeVector InputEvent::getTargetRanges() const {
  StaticRangeVector static_ranges;
  for (const auto& range : ranges_)
    static_ranges.push_back(StaticRange::Create(range));
  return static_ranges;
}

bool InputEvent::IsInputEvent() const {
  return true;
}

void InputEvent::Trace(Visitor* visitor) const {
  UIEvent::Trace(visitor);
  visitor->Trace(data_transfer_);
  visitor->Trace(ranges_);
}

DispatchEventResult InputEvent::DispatchEvent(EventDispatcher& dispatcher) {
  DispatchEventResult result = dispatcher.Dispatch();
  // It's weird to hold and clear live |Range| objects internally, and only
  // expose |StaticRange| through |getTargetRanges()|. However there is no
  // better solutions due to the following issues:
  //   1. We don't want to expose live |Range| objects for the author to hold as
  //      it will slow down all DOM operations. So we just expose |StaticRange|.
  //   2. Event handlers in chain might modify DOM, which means we have to keep
  //      a copy of live |Range| internally and return snapshots.
  //   3. We don't want authors to hold live |Range| indefinitely by holding
  //      |InputEvent|, so we clear them after dispatch.
  // Authors should explicitly call |getTargetRanges()|->|toRange()| if they
  // want to keep a copy of |Range|.  See Editing TF meeting notes:
  // https://docs.google.com/document/d/1hCj6QX77NYIVY0RWrMHT1Yra6t8_Qu8PopaWLG0AM58/edit?usp=sharing
  //
  // This is the only Event::DispatchEvent() that modifies the event.
  ranges_.clear();
  return result;
}

}  // namespace blink
