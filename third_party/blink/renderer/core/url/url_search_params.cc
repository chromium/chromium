// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/url_search_params.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequencesequence_usvstringusvstringrecord.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

class URLSearchParamsIterationSource final
    : public PairSyncIterable<URLSearchParams>::IterationSource {
 public:
  explicit URLSearchParamsIterationSource(URLSearchParams* params)
      : params_(params), current_(0) {}

  bool FetchNextItem(ScriptState*,
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

  void Trace(Visitor* visitor) const override {
    visitor->Trace(params_);
    PairSyncIterable<URLSearchParams>::IterationSource::Trace(visitor);
  }

 private:
  Member<URLSearchParams> params_;
  wtf_size_t current_;
};

bool CompareParams(const std::pair<String, String>& a,
                   const std::pair<String, String>& b) {
  return WTF::CodeUnitCompareLessThan(a.first, b.first);
}

}  // namespace

URLSearchParams* URLSearchParams::Create(const URLSearchParamsInit* init,
                                         ExceptionState& exception_state) {
  DCHECK(init);
  switch (init->GetContentType()) {
    case URLSearchParamsInit::ContentType::kUSVString: {
      const String& query_string = init->GetAsUSVString();
      if (query_string.StartsWith('?'))
        return MakeGarbageCollected<URLSearchParams>(query_string.Substring(1));
      return MakeGarbageCollected<URLSearchParams>(query_string);
    }
    case URLSearchParamsInit::ContentType::kUSVStringSequenceSequence:
      return URLSearchParams::Create(init->GetAsUSVStringSequenceSequence(),
                                     exception_state);
    case URLSearchParamsInit::ContentType::kUSVStringUSVStringRecord:
      return URLSearchParams::Create(init->GetAsUSVStringUSVStringRecord(),
                                     exception_state);
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
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
  if (!query_string.empty())
    SetInputWithoutUpdate(query_string);
}

URLSearchParams* URLSearchParams::Create(
    const Vector<std::pair<String, String>>& init,
    ExceptionState& exception_state) {
  URLSearchParams* instance = MakeGarbageCollected<URLSearchParams>(String());
  if (init.empty())
    return instance;
  for (const auto& item : init)
    instance->AppendWithoutUpdate(item.first, item.second);
  return instance;
}

URLSearchParams::~URLSearchParams() = default;

void URLSearchParams::Trace(Visitor* visitor) const {
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

  wtf_size_t start = 0;
  wtf_size_t query_string_length = query_string.length();
  while (start < query_string_length) {
    wtf_size_t name_start = start;
    wtf_size_t name_value_end = query_string.find('&', start);
    if (name_value_end == kNotFound)
      name_value_end = query_string_length;
    if (name_value_end > start) {
      wtf_size_t end_of_name = query_string.find('=', start);
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

uint32_t URLSearchParams::size() const {
  return params_.size();
}

void URLSearchParams::AppendWithoutUpdate(const String& name,
                                          const String& value) {
  params_.push_back(std::make_pair(name, value));
}

void URLSearchParams::append(const String& name, const String& value) {
  AppendWithoutUpdate(name, value);
  RunUpdateSteps();
}

void URLSearchParams::deleteAllWithNameOrTuple(
    ExecutionContext* execution_context,
    const String& name) {
  deleteAllWithNameOrTuple(execution_context, name, String());
}

void URLSearchParams::deleteAllWithNameOrTuple(
    ExecutionContext* execution_context,
    const String& name,
    const String& val) {
  String value = val;
  if (!RuntimeEnabledFeatures::
          URLSearchParamsHasAndDeleteMultipleArgsEnabled()) {
    value = String();
  }
  // TODO(debadree333): Remove the code to count
  // kURLSearchParamsDeleteFnBehaviourDiverged in October 2023.
  Vector<wtf_size_t, 1u> indices_to_remove_with_name_value;
  Vector<wtf_size_t, 1u> indices_to_remove_with_name;

  for (wtf_size_t i = 0; i < params_.size(); i++) {
    if (params_[i].first == name) {
      indices_to_remove_with_name.push_back(i);
      if (params_[i].second == value || value.IsNull()) {
        indices_to_remove_with_name_value.push_back(i);
      }
    }
  }

  if (indices_to_remove_with_name_value != indices_to_remove_with_name) {
    UseCounter::Count(execution_context,
                      WebFeature::kURLSearchParamsDeleteFnBehaviourDiverged);
  }

  for (auto it = indices_to_remove_with_name_value.rbegin();
       it != indices_to_remove_with_name_value.rend(); ++it) {
    params_.EraseAt(*it);
  }

  RunUpdateSteps();
}

String URLSearchParams::get(const String& name) const {
  for (const auto& param : params_) {
    if (param.first == name) {
      return param.second;
    }
  }
  return String();
}

Vector<String> URLSearchParams::getAll(const String& name) const {
  Vector<String> result;
  for (const auto& param : params_) {
    if (param.first == name) {
      result.push_back(param.second);
    }
  }
  return result;
}

bool URLSearchParams::has(ExecutionContext* execution_context,
                          const String& name) const {
  return has(execution_context, name, String());
}

bool URLSearchParams::has(ExecutionContext* execution_context,
                          const String& name,
                          const String& val) const {
  String value = val;
  if (!RuntimeEnabledFeatures::
          URLSearchParamsHasAndDeleteMultipleArgsEnabled()) {
    value = String();
  }
  // TODO(debadree333): Remove the code to count
  // kURLSearchParamsHasFnBehaviourDiverged in October 2023.
  bool found_match_using_name_and_value = false;
  bool found_match_using_name = false;
  for (const auto& param : params_) {
    const bool name_matched = (param.first == name);
    if (name_matched) {
      found_match_using_name = true;
    }
    if (name_matched && (value.IsNull() || param.second == value)) {
      found_match_using_name_and_value = true;
      break;
    }
  }

  if (found_match_using_name_and_value != found_match_using_name) {
    UseCounter::Count(execution_context,
                      WebFeature::kURLSearchParamsHasFnBehaviourDiverged);
  }
  return found_match_using_name_and_value;
}

void URLSearchParams::set(const String& name, const String& value) {
  bool found_match = false;
  for (wtf_size_t i = 0; i < params_.size();) {
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
  if (!found_match) {
    append(name, value);
  } else {
    RunUpdateSteps();
  }
}

void URLSearchParams::sort() {
  std::stable_sort(params_.begin(), params_.end(), CompareParams);
  RunUpdateSteps();
}

void URLSearchParams::EncodeAsFormData(Vector<char>& encoded_data) const {
  for (const auto& param : params_) {
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, param.first.Utf8(), param.second.Utf8(),
        EncodedFormData::kFormURLEncoded, FormDataEncoder::kDoNotNormalizeCRLF);
  }
}

scoped_refptr<EncodedFormData> URLSearchParams::ToEncodedFormData() const {
  Vector<char> encoded_data;
  EncodeAsFormData(encoded_data);
  return EncodedFormData::Create(encoded_data.data(), encoded_data.size());
}

PairSyncIterable<URLSearchParams>::IterationSource*
URLSearchParams::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<URLSearchParamsIterationSource>(this);
}

}  // namespace blink
