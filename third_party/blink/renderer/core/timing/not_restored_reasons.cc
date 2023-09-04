// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/not_restored_reasons.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

NotRestoredReasons::NotRestoredReasons(String prevented,
                                       String src,
                                       String id,
                                       String name,
                                       String url,
                                       Vector<String>* reasons,
                                       HeapVector<NotRestoredReasons>* children)
    : prevented_(prevented), src_(src), id_(id), name_(name), url_(url) {
  if (reasons) {
    for (auto reason : *reasons) {
      reasons_.push_back(reason);
    }
  }
  if (children) {
    for (NotRestoredReasons& child : *children) {
      children_.push_back(child);
    }
  }
}

void NotRestoredReasons::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  ScriptWrappable::Trace(visitor);
}

NotRestoredReasons::NotRestoredReasons(const NotRestoredReasons& other)
    : prevented_(other.prevented_),
      src_(other.src_),
      id_(other.id_),
      name_(other.name_),
      url_(other.url_),
      reasons_(other.reasons_),
      children_(other.children_) {}

const absl::optional<Vector<String>> NotRestoredReasons::reasons() const {
  if (!url_) {
    // If `url_` is null, this is for cross-origin and reasons should be masked.
    return absl::nullopt;
  }
  return reasons_;
}

const absl::optional<HeapVector<Member<NotRestoredReasons>>>
NotRestoredReasons::children() const {
  if (!url_) {
    // If `url_` is null, this is for cross-origin and children should be
    // masked.
    return absl::nullopt;
  }
  return children_;
}

ScriptValue NotRestoredReasons::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);

  builder.AddString("preventedBackForwardCache", preventedBackForwardCache());
  builder.AddStringOrNull("src", src());
  builder.AddStringOrNull("id", id());
  builder.AddStringOrNull("url", url());
  builder.AddStringOrNull("name", name());
  if (reasons().has_value()) {
    Vector<AtomicString> reason_strings;
    for (const auto& reason : reasons_) {
      reason_strings.push_back(reason);
    }
    builder.Add("reasons", reason_strings);
  } else {
    builder.AddNull("reasons");
  }
  if (children().has_value()) {
    Vector<v8::Local<v8::Value>> children_result;
    for (Member<NotRestoredReasons> child : children_) {
      children_result.push_back(child->toJSON(script_state).V8Value());
    }
    builder.Add("children", children_result);
  } else {
    builder.AddNull("children");
  }

  return builder.GetScriptValue();
}

}  // namespace blink
