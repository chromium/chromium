// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_NAVIGATION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_NAVIGATION_DATA_H_

#include <optional>

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_speculation_eagerness_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_speculation_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// https://github.com/WICG/speculative_load_measurement
class SpeculationNavigationData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SpeculationNavigationData(
      mojom::blink::SpeculationAction action,
      const KURL& url,
      const std::optional<Vector<String>>& tags,
      std::optional<mojom::blink::SpeculationEagerness> eagerness);

  V8SpeculationNavigationType type() const;
  String url() const { return url_.GetString(); }
  const std::optional<Vector<String>>& tags() const { return tags_; }
  std::optional<V8SpeculationEagernessValue> eagerness() const;

  void Trace(Visitor*) const override;

 private:
  const mojom::blink::SpeculationAction action_;
  const KURL url_;
  const std::optional<Vector<String>> tags_;
  const std::optional<mojom::blink::SpeculationEagerness> eagerness_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_NAVIGATION_DATA_H_
