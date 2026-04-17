// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRELOAD_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRELOAD_DATA_H_

#include <optional>

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cross_origin_mode.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ScriptState;

// https://github.com/WICG/speculative_load_measurement
class PreloadData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PreloadData(const KURL& url,
              ResourceType resource_type,
              CrossOriginAttributeValue crossorigin,
              std::optional<base::TimeTicks> used_time);

  String url() const { return url_.GetString(); }
  String as() const;
  V8CrossOriginMode crossorigin() const;
  std::optional<double> used(ScriptState*) const;

  void Trace(Visitor*) const override;

 private:
  const KURL url_;
  const ResourceType resource_type_;
  const CrossOriginAttributeValue crossorigin_;
  const std::optional<base::TimeTicks> used_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRELOAD_DATA_H_
