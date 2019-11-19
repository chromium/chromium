// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/headers.h"

#include "third_party/blink/renderer/bindings/core/v8/byte_string_sequence_sequence_or_byte_string_byte_string_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/dom/iterator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class HeadersIterationSource final
    : public PairIterable<String, String>::IterationSource {
 public:
  explicit HeadersIterationSource(const FetchHeaderList* headers)
      : headers_(headers->SortAndCombine()), current_(0) {}

  bool Next(ScriptState* script_state,
            String& key,
            String& value,
            ExceptionState& exception) override {
    // This simply advances an index and returns the next value if any; the
    // iterated list is not exposed to script so it will never be mutated
    // during iteration.
    if (current_ >= headers_.size())
      return false;

    const FetchHeaderList::Header& header = headers_.at(current_++);
    key = header.first;
    value = header.second;
    return true;
  }

 private:
  Vector<std::pair<String, String>> headers_;
  wtf_size_t current_;
};

}  // namespace

Headers* Headers::Create(ExceptionState&) {
  return MakeGarbageCollected<Headers>();
}

Headers* Headers::Create(const HeadersInit& init,
                         ExceptionState& exception_state) {
  // "The Headers(|init|) constructor, when invoked, must run these steps:"
  // "1. Let |headers| be a new Headers object whose guard is "none".
  Headers* headers = Create(exception_state);
  // "2. If |init| is given, fill headers with |init|. Rethrow any exception."
  headers->FillWith(init, exception_state);
  // "3. Return |headers|."
  return headers;
}

Headers* Headers::Create(FetchHeaderList* header_list) {
  return MakeGarbageCollected<Headers>(header_list);
}

Headers* Headers::Clone() const {
  FetchHeaderList* header_list = header_list_->Clone();
  Headers* headers = Create(header_list);
  headers->guard_ = guard_;
  return headers;
}

void Headers::append(const String& name,
                     const String& value,
                     ExceptionState& exception_state) {
  // "To append a name/value (|name|/|value|) pair to a Headers object
  // (|headers|), run these steps:"
  // "1. Normalize |value|."
  const String normalized_value = FetchUtils::NormalizeHeaderValue(value);
  // "2. If |name| is not a name or |value| is not a value, throw a
  //     TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  if (!FetchHeaderList::IsValidHeaderValue(normalized_value)) {
    exception_state.ThrowTypeError("Invalid value");
    return;
  }
  // "3. If guard is |immutable|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "4. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenHeaderName(name))
    return;
  // 5. Otherwise, if guard is |request-no-cors|:
  if (guard_ == kRequestNoCorsGuard) {
    // Let |temporaryValue| be the result of getting name from |headers|’s
    // header list.
    String temp;
    header_list_->Get(name, temp);

    // If |temporaryValue| is null, then set |temporaryValue| to |value|.
    // Otherwise, set |temporaryValue| to |temporaryValue|, followed by
    // 0x2C 0x20, followed by |value|.
    if (temp.IsNull()) {
      temp = normalized_value;
    } else {
      temp = temp + ", " + normalized_value;
    }

    // If |name|/|temporaryValue| is not a no-CORS-safelisted request-header,
    // then return.
    if (!cors::IsNoCorsSafelistedHeader(name, temp))
      return;
  }
  // "6. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name)) {
    return;
  }
  // "7. Append |name|/|value| to header list."
  header_list_->Append(name, normalized_value);
}

void Headers::remove(const String& name, ExceptionState& exception_state) {
  // "The delete(|name|) method, when invoked, must run these steps:"
  // "1. If name is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  // "2. If guard is |immutable|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "3. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenHeaderName(name))
    return;
  // "4. Otherwise, if the context object’s guard is |request-no-cors|, |name|
  //     is not a no-CORS-safelisted request-header name, and |name| is not a
  //     privileged no-CORS request-header name, return."
  if (guard_ == kRequestNoCorsGuard &&
      !cors::IsNoCorsSafelistedHeaderName(name) &&
      !cors::IsPrivilegedNoCorsHeaderName(name)) {
    return;
  }
  // "5. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name)) {
    return;
  }
  // "6. Delete |name| from header list."
  header_list_->Remove(name);
}

String Headers::get(const String& name, ExceptionState& exception_state) {
  // "The get(|name|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return String();
  }
  // "2. If there is no header in header list whose name is |name|,
  //     return null."
  // "3. Return the combined value given |name| and header list."
  String result;
  header_list_->Get(name, result);
  return result;
}

bool Headers::has(const String& name, ExceptionState& exception_state) {
  // "The has(|name|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return false;
  }
  // "2. Return true if there is a header in header list whose name is |name|,
  //     and false otherwise."
  return header_list_->Has(name);
}

void Headers::set(const String& name,
                  const String& value,
                  ExceptionState& exception_state) {
  // "The set(|name|, |value|) method, when invoked, must run these steps:"
  // "1. Normalize |value|."
  const String normalized_value = FetchUtils::NormalizeHeaderValue(value);
  // "2. If |name| is not a name or |value| is not a value, throw a
  //     TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  if (!FetchHeaderList::IsValidHeaderValue(normalized_value)) {
    exception_state.ThrowTypeError("Invalid value");
    return;
  }
  // "3. If guard is |immutable|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "4. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenHeaderName(name))
    return;
  // "5. Otherwise, if guard is |request-no-CORS| and |name|/|value| is not a
  //     no-CORS-safelisted header, return."
  if (guard_ == kRequestNoCorsGuard &&
      !cors::IsNoCorsSafelistedHeader(name, normalized_value)) {
    return;
  }
  // "6. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name)) {
    return;
  }
  // "7. Set |name|/|value| in header list."
  header_list_->Set(name, normalized_value);
}

// This overload is not called directly by Web APIs, but rather by other C++
// classes. For example, when initializing a Request object it is possible that
// a Request's Headers must be filled with an existing Headers object.
void Headers::FillWith(const Headers* object, ExceptionState& exception_state) {
  DCHECK_EQ(header_list_->size(), 0U);
  for (const auto& header : object->header_list_->List()) {
    append(header.first, header.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(const HeadersInit& init,
                       ExceptionState& exception_state) {
  DCHECK_EQ(header_list_->size(), 0U);
  if (init.IsByteStringSequenceSequence()) {
    FillWith(init.GetAsByteStringSequenceSequence(), exception_state);
  } else if (init.IsByteStringByteStringRecord()) {
    FillWith(init.GetAsByteStringByteStringRecord(), exception_state);
  } else {
    NOTREACHED();
  }
}

void Headers::FillWith(const Vector<Vector<String>>& object,
                       ExceptionState& exception_state) {
  DCHECK(!header_list_->size());
  // "1. If |object| is a sequence, then for each |header| in |object|, run
  //     these substeps:
  //     1. If |header| does not contain exactly two items, then throw a
  //        TypeError.
  //     2. Append |header|’s first item/|header|’s second item to |headers|.
  //        Rethrow any exception."
  for (wtf_size_t i = 0; i < object.size(); ++i) {
    if (object[i].size() != 2) {
      exception_state.ThrowTypeError("Invalid value");
      return;
    }
    append(object[i][0], object[i][1], exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(const Vector<std::pair<String, String>>& object,
                       ExceptionState& exception_state) {
  DCHECK(!header_list_->size());

  for (const auto& item : object) {
    append(item.first, item.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

Headers::Headers()
    : header_list_(MakeGarbageCollected<FetchHeaderList>()),
      guard_(kNoneGuard) {}

Headers::Headers(FetchHeaderList* header_list)
    : header_list_(header_list), guard_(kNoneGuard) {}

void Headers::Trace(blink::Visitor* visitor) {
  visitor->Trace(header_list_);
  ScriptWrappable::Trace(visitor);
}

PairIterable<String, String>::IterationSource* Headers::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<HeadersIterationSource>(header_list_);
}

}  // namespace blink
