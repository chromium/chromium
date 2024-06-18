// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/headers.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_bytestringbytestringrecord_bytestringsequencesequence.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

Headers::HeadersIterationSource::HeadersIterationSource(Headers* headers)
    : headers_list_(headers->HeaderList()->SortAndCombine()),
      headers_(headers) {}

void Headers::HeadersIterationSource::ResetHeaderList() {
  headers_list_ = headers_->HeaderList()->SortAndCombine();
}

bool Headers::HeadersIterationSource::FetchNextItem(ScriptState* script_state,
                                                    String& key,
                                                    String& value,
                                                    ExceptionState& exception) {
  // This simply advances an index and returns the next value if any;
  if (current_ >= headers_list_.size())
    return false;

  const FetchHeaderList::Header& header = headers_list_.at(current_++);
  key = header.first;
  value = header.second;
  return true;
}

void Headers::HeadersIterationSource::Trace(Visitor* visitor) const {
  visitor->Trace(headers_);
  PairSyncIterable<Headers>::IterationSource::Trace(visitor);
}

Headers::HeadersIterationSource::~HeadersIterationSource() = default;

Headers* Headers::Create(ScriptState* script_state, ExceptionState&) {
  return MakeGarbageCollected<Headers>();
}

Headers* Headers::Create(ScriptState* script_state,
                         const V8HeadersInit* init,
                         ExceptionState& exception_state) {
  // "The Headers(|init|) constructor, when invoked, must run these steps:"
  // "1. Let |headers| be a new Headers object whose guard is "none".
  Headers* headers = Create(script_state, exception_state);
  // "2. If |init| is given, fill headers with |init|. Rethrow any exception."
  headers->FillWith(script_state, init, exception_state);
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

void Headers::append(ScriptState* script_state,
                     const String& name,
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
  // UseCounter for usages of "set-cookie" in kRequestGuard'ed Headers.
  if (guard_ == kRequestGuard && EqualIgnoringASCIICase(name, "set-cookie")) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    UseCounter::Count(execution_context,
                      WebFeature::kFetchSetCookieInRequestGuardedHeaders);
  }
  // "4. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenRequestHeader(name, value))
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
  // "8. If this’s guard is |request-no-cors|, then remove privileged no-CORS
  // request headers from this."
  if (guard_ == kRequestNoCorsGuard)
    RemovePrivilegedNoCorsRequestHeaders();
  // "9. Notify active iterators about the modification."
  for (auto& iter : iterators_) {
    iter->ResetHeaderList();
  }
}

void Headers::remove(ScriptState* script_state,
                     const String& name,
                     ExceptionState& exception_state) {
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
  // UseCounter for usages of "set-cookie" in kRequestGuard'ed Headers.
  if (guard_ == kRequestGuard && EqualIgnoringASCIICase(name, "set-cookie")) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    UseCounter::Count(execution_context,
                      WebFeature::kFetchSetCookieInRequestGuardedHeaders);
  }
  // "3. Otherwise, if guard is |request| and (|name|, '') is a forbidden
  //     request header, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenRequestHeader(name, ""))
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
  // "6. If this’s header list does not contain |name|, then return."
  if (!header_list_->Has(name))
    return;
  // "7. Delete |name| from header list."
  header_list_->Remove(name);
  // "8. If this’s guard is |request-no-cors|, then remove privileged no-CORS
  // request headers from this."
  if (guard_ == kRequestNoCorsGuard)
    RemovePrivilegedNoCorsRequestHeaders();
  // "9. Notify active iterators about the modification."
  for (auto& iter : iterators_) {
    iter->ResetHeaderList();
  }
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

Vector<String> Headers::getSetCookie() {
  return header_list_->GetSetCookie();
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

void Headers::set(ScriptState* script_state,
                  const String& name,
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
  // UseCounter for usages of "set-cookie" in kRequestGuard'ed Headers.
  if (guard_ == kRequestGuard && EqualIgnoringASCIICase(name, "set-cookie")) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    UseCounter::Count(execution_context,
                      WebFeature::kFetchSetCookieInRequestGuardedHeaders);
  }
  // "4. Otherwise, if guard is |request| and (|name|, |value|) is a forbidden
  //     request header, return."
  if (guard_ == kRequestGuard && cors::IsForbiddenRequestHeader(name, value))
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
  // "8. If this’s guard is |request-no-cors|, then remove privileged no-CORS
  // request headers from this."
  if (guard_ == kRequestNoCorsGuard)
    RemovePrivilegedNoCorsRequestHeaders();
  // "9. Notify active iterators about the modification."
  for (auto& iter : iterators_) {
    iter->ResetHeaderList();
  }
}

// This overload is not called directly by Web APIs, but rather by other C++
// classes. For example, when initializing a Request object it is possible that
// a Request's Headers must be filled with an existing Headers object.
void Headers::FillWith(ScriptState* script_state,
                       const Headers* object,
                       ExceptionState& exception_state) {
  DCHECK_EQ(header_list_->size(), 0U);
  for (const auto& header : object->header_list_->List()) {
    append(script_state, header.first, header.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(ScriptState* script_state,
                       const V8HeadersInit* init,
                       ExceptionState& exception_state) {
  DCHECK_EQ(header_list_->size(), 0U);

  if (!init)
    return;

  switch (init->GetContentType()) {
    case V8HeadersInit::ContentType::kByteStringByteStringRecord:
      return FillWith(script_state, init->GetAsByteStringByteStringRecord(),
                      exception_state);
    case V8HeadersInit::ContentType::kByteStringSequenceSequence:
      return FillWith(script_state, init->GetAsByteStringSequenceSequence(),
                      exception_state);
  }

  NOTREACHED_IN_MIGRATION();
}

void Headers::FillWith(ScriptState* script_state,
                       const Vector<Vector<String>>& object,
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
    append(script_state, object[i][0], object[i][1], exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(ScriptState* script_state,
                       const Vector<std::pair<String, String>>& object,
                       ExceptionState& exception_state) {
  DCHECK(!header_list_->size());

  for (const auto& item : object) {
    append(script_state, item.first, item.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::RemovePrivilegedNoCorsRequestHeaders() {
  const Vector<String> privileged_no_cors_header_names =
      cors::PrivilegedNoCorsHeaderNames();
  for (const auto& header : privileged_no_cors_header_names)
    header_list_->Remove(header);
}

Headers::Headers()
    : header_list_(MakeGarbageCollected<FetchHeaderList>()),
      guard_(kNoneGuard) {}

Headers::Headers(FetchHeaderList* header_list)
    : header_list_(header_list), guard_(kNoneGuard) {}

void Headers::Trace(Visitor* visitor) const {
  visitor->Trace(header_list_);
  visitor->Trace(iterators_);
  ScriptWrappable::Trace(visitor);
}

PairSyncIterable<Headers>::IterationSource* Headers::CreateIterationSource(
    ScriptState*,
    ExceptionState&) {
  auto* iter = MakeGarbageCollected<HeadersIterationSource>(this);
  iterators_.insert(iter);
  return iter;
}

}  // namespace blink
