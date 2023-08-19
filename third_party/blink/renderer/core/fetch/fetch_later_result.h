// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_RESULT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_fetch_later_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT FetchLaterResult final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FetchLaterResult(bool sent = false);

  // From fetch_later.idl:
  bool sent() const;

 private:
  const bool sent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_LATER_RESULT_H_
