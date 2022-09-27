// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_OVERCONSTRAINED_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_OVERCONSTRAINED_ERROR_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class OverconstrainedError final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OverconstrainedError* Create(const String& constraint,
                                      const String& message);

  OverconstrainedError(const String& constraint, const String& message);

  String name() const { return "OverconstrainedError"; }
  const String& constraint() const { return constraint_; }
  const String& message() const { return message_; }

 private:
  String constraint_;
  String message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_OVERCONSTRAINED_ERROR_H_
