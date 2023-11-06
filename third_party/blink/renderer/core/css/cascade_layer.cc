// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cascade_layer.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CascadeLayer* CascadeLayer::FindDirectSubLayer(const AtomicString& name) const {
  // Anonymous layers are all distinct.
  if (name == g_empty_atom) {
    return nullptr;
  }
  for (const auto& sub_layer : direct_sub_layers_) {
    if (sub_layer->GetName() == name) {
      return sub_layer.Get();
    }
  }
  return nullptr;
}

CascadeLayer* CascadeLayer::GetOrAddSubLayer(
    const StyleRuleBase::LayerName& name) {
  CascadeLayer* layer = this;
  for (const AtomicString& name_part : name) {
    CascadeLayer* direct_sub_layer = layer->FindDirectSubLayer(name_part);
    if (!direct_sub_layer) {
      direct_sub_layer = MakeGarbageCollected<CascadeLayer>(name_part);
      layer->direct_sub_layers_.push_back(direct_sub_layer);
    }
    layer = direct_sub_layer;
  }
  return layer;
}

String CascadeLayer::ToStringForTesting() const {
  StringBuilder result;
  ToStringInternal(result, "");
  return result.ReleaseString();
}

void CascadeLayer::ToStringInternal(StringBuilder& result,
                                    const String& prefix) const {
  for (const auto& sub_layer : direct_sub_layers_) {
    String name =
        sub_layer->name_.length() ? sub_layer->name_ : String("(anonymous)");
    if (result.length()) {
      result.Append(",");
    }
    result.Append(prefix);
    result.Append(name);
    sub_layer->ToStringInternal(result, prefix + name + ".");
  }
}

void CascadeLayer::Merge(const CascadeLayer& other, LayerMap& mapping) {
  mapping.insert(&other, this);
  for (CascadeLayer* sub_layer : other.direct_sub_layers_) {
    GetOrAddSubLayer({sub_layer->GetName()})->Merge(*sub_layer, mapping);
  }
}

void CascadeLayer::Trace(blink::Visitor* visitor) const {
  visitor->Trace(direct_sub_layers_);
}

}  // namespace blink
