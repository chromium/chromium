// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/url_search_params.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

class URLSearchParamsIterationSource final
    : public PairIterable<String, String>::IterationSource {
 public:
  explicit URLSearchParamsIterationSource(URLSearchParams* params)
      : params_(params), current_(0) {}

  bool Next(ScriptState*,
            String& key,
            String& value,
            ExceptionState&) override {
    if (current_ >= params_->Params().size())
      return false;

    key = params_->Params()[current_].first;
    value = params_->Params()[current_].second;
    current_++;
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(params_);
    PairIterable<String, String>::IterationSource::Trace(visitor);
  }

 private:
  Member<URLSearchParams> params_;
  size_t current_;
};

bool CompareParams(const std::pair<String, String>& a,
                   const std::pair<String, String>& b) {
  return WTF::CodeUnitCompareLessThan(a.first, b.first);
}

}  // namespace

URLSearchParams* URLSearchParams::Create(const URLSearchParamsInit& init,
                                         ExceptionState& exception_state) {
  if (init.IsUSVString()) {
    const String& query_string = init.GetAsUSVString();
    if (query_string.StartsWith('?'))
      return MakeGarbageCollected<URLSearchParams>(query_string.Substring(1));
    return MakeGarbageCollected<URLSearchParams>(query_string);
  }
  if (init.IsUSVStringUSVStringRecord()) {
    return URLSearchParams::Create(init.GetAsUSVStringUSVStringRecord(),
                                   exception_state);
  }
  if (init.IsUSVStringSequenceSequence()) {
    return URLSearchParams::Create(init.GetAsUSVStringSequenceSequence(),
                                   exception_state);
  }

  DCHECK(init.IsNull());
  return MakeGarbageCollected<URLSearchParams>(String());
}

URLSearchParams* URLSearchParams::Create(const Vector<Vector<String>>& init,
                                         ExceptionState& exception_state) {
  URLSearchParams* instance = MakeGarbageCollected<URLSearchParams>(String());
  if (!init.size())
    return instance;
  for (unsigned i = 0; i < init.size(); ++i) {
    const Vector<String>& pair = init[i];
    if (pair.size() != 2) {
      exception_state.ThrowTypeError(ExceptionMessages::FailedToConstruct(
          "URLSearchParams",
          "Sequence initializer must only contain pair elements"));
      return nullptr;
    }
    instance->AppendWithoutUpdate(pair[0], pair[1]);
  }
  return instance;
}

URLSearchParams::URLSearchParams(const String& query_string, DOMURL* url_object)
    : url_object_(url_object) {
  if (!query_string.IsEmpty())
    SetInputWithoutUpdate(query_string);
}

URLSearchParams* URLSearchParams::Create(
    const Vector<std::pair<String, String>>& init,
    ExceptionState& exception_state) {
  URLSearchParams* instance = MakeGarbageCollected<URLSearchParams>(String());
  if (init.IsEmpty())
    return instance;
  for (const auto& item : init)
    instance->AppendWithoutUpdate(item.first, item.second);
  return instance;
}

URLSearchParams::~URLSearchParams() = default;

void URLSearchParams::Trace(blink::Visitor* visitor) {
  visitor->Trace(url_object_);
  ScriptWrappable::Trace(visitor);
}

#if DCHECK_IS_ON()
DOMURL* URLSearchParams::UrlObject() const {
  return url_object_;
}
#endif

void URLSearchParams::RunUpdateSteps() {
  if (!url_object_)
    return;

  if (url_object_->IsInUpdate())
    return;

  url_object_->SetSearchInternal(toString());
}

static String DecodeString(String input) {
  // |DecodeURLMode::kUTF8| is used because "UTF-8 decode without BOM" should
  // be performed (see https://url.spec.whatwg.org/#concept-urlencoded-parser).
  return DecodeURLEscapeSequences(input.Replace('+', ' '),
                                  DecodeURLMode::kUTF8);
}

void URLSearchParams::SetInputWithoutUpdate(const String& query_string) {
  params_.clear();

  size_t start = 0;
  size_t query_string_length = query_string.length();
  while (start < query_string_length) {
    size_t name_start = start;
    size_t name_value_end = query_string.find('&', start);
    if (name_value_end == kNotFound)
      name_value_end = query_string_length;
    if (name_value_end > start) {
      size_t end_of_name = query_string.find('=', start);
      if (end_of_name == kNotFound || end_of_name > name_value_end)
        end_of_name = name_value_end;
      String name = DecodeString(
          query_string.Substring(name_start, end_of_name - name_start));
      String value;
      if (end_of_name != name_value_end)
        value = DecodeString(query_string.Substring(
            end_of_name + 1, name_value_end - end_of_name - 1));
      if (value.IsNull())
        value = "";
      AppendWithoutUpdate(name, value);
    }
    start = name_value_end + 1;
  }
}

String URLSearchParams::toString() const {
  Vector<char> encoded_data;
  EncodeAsFormData(encoded_data);
  return String(encoded_data.data(), encoded_data.size());
}

void URLSearchParams::AppendWithoutUpdate(const String& name,
                                          const String& value) {
  params_.push_back(std::make_pair(name, value));
}

void URLSearchParams::append(const String& name, const String& value) {
  AppendWithoutUpdate(name, value);
  RunUpdateSteps();
}

void URLSearchParams::deleteAllWithName(const String& name) {
  for (size_t i = 0; i < params_.size();) {
    if (params_[i].first == name)
      params_.EraseAt(i);
    else
      i++;
  }
  RunUpdateSteps();
}

String URLSearchParams::get(const String& name) const {
  for (const auto& param : params_) {
    if (param.first == name)
      return param.second;
  }
  return String();
}

Vector<String> URLSearchParams::getAll(const String& name) const {
  Vector<String> result;
  for (const auto& param : params_) {
    if (param.first == name)
      result.push_back(param.second);
  }
  return result;
}

bool URLSearchParams::has(const String& name) const {
  for (const auto& param : params_) {
    if (param.first == name)
      return true;
  }
  return false;
}

void URLSearchParams::set(const String& name, const String& value) {
  bool found_match = false;
  for (size_t i = 0; i < params_.size();) {
    // If there are any name-value whose name is 'name', set
    // the value of the first such name-value pair to 'value'
    // and remove the others.
    if (params_[i].first == name) {
      if (!found_match) {
        params_[i++].second = value;
        found_match = true;
      } else {
        params_.EraseAt(i);
      }
    } else {
      i++;
    }
  }
  // Otherwise, append a new name-value pair to the list.
  if (!found_match)
    append(name, value);
  else
    RunUpdateSteps();
}

void URLSearchParams::sort() {
  std::stable_sort(params_.begin(), params_.end(), CompareParams);
  RunUpdateSteps();
}

void URLSearchParams::EncodeAsFormData(Vector<char>& encoded_data) const {
  for (const auto& param : params_)
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, param.first.Utf8(), param.second.Utf8(),
        EncodedFormData::kFormURLEncoded, FormDataEncoder::kDoNotNormalizeCRLF);
}

scoped_refptr<EncodedFormData> URLSearchParams::ToEncodedFormData() const {
  Vector<char> encoded_data;
  EncodeAsFormData(encoded_data);
  return EncodedFormData::Create(encoded_data.data(), encoded_data.size());
}

PairIterable<String, String>::IterationSource* URLSearchParams::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<URLSearchParamsIterationSource>(this);
}

}  // namespace blink
