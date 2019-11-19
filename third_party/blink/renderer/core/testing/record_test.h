// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_RECORD_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_RECORD_TEST_H_

#include <utility>
#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/boolean_or_byte_string_byte_string_record.h"
#include "third_party/blink/renderer/bindings/core/v8/float_or_string_element_record.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class RecordTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RecordTest();
  ~RecordTest() override;

  void setStringLongRecord(const Vector<std::pair<String, int32_t>>& arg);
  Vector<std::pair<String, int32_t>> getStringLongRecord();

  void setNullableStringLongRecord(
      const base::Optional<Vector<std::pair<String, int32_t>>>& arg);
  base::Optional<Vector<std::pair<String, int32_t>>>
  getNullableStringLongRecord();

  Vector<std::pair<String, String>> GetByteStringByteStringRecord();
  void setByteStringByteStringRecord(
      const Vector<std::pair<String, String>>& arg);

  void setStringElementRecord(
      const HeapVector<std::pair<String, Member<Element>>>& arg);
  HeapVector<std::pair<String, Member<Element>>> getStringElementRecord();

  using NestedRecordType =
      Vector<std::pair<String, Vector<std::pair<String, bool>>>>;
  void setUSVStringUSVStringBooleanRecordRecord(const NestedRecordType& arg);
  NestedRecordType getUSVStringUSVStringBooleanRecordRecord();

  Vector<std::pair<String, Vector<String>>>
  returnStringByteStringSequenceRecord();

  bool unionReceivedARecord(const BooleanOrByteStringByteStringRecord& arg);

  void setFloatOrStringElementRecord(const FloatOrStringElementRecord&) {}

  void Trace(blink::Visitor*) override;

 private:
  Vector<std::pair<String, int32_t>> string_long_record_;
  base::Optional<Vector<std::pair<String, int32_t>>>
      nullable_string_long_record_;
  Vector<std::pair<String, String>> byte_string_byte_string_record_;
  HeapVector<std::pair<String, Member<Element>>> string_element_record_;
  NestedRecordType usv_string_usv_string_boolean_record_record_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_RECORD_TEST_H_
