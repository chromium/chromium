// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NOT_RESTORED_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NOT_RESTORED_REASONS_H_

#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_not_restored_reasons.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_string.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT NotRestoredReasons : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Make this protected. Always create with a Build.. method
  explicit NotRestoredReasons(String prevented,
                              String src,
                              String id,
                              String name,
                              String url,
                              Vector<String>* reasons,
                              HeapVector<NotRestoredReasons>* children);

  NotRestoredReasons(const NotRestoredReasons&);

  const String preventedBackForwardCache() const { return prevented_; }

  const String src() const { return src_; }

  const String id() const { return id_; }

  const String name() const { return name_; }

  const String url() const { return url_; }

  const V8ObservableArrayString* reasons() const { return reasons_; }

  const V8ObservableArrayNotRestoredReasons* children() const {
    return children_;
  }

  ScriptValue toJSON(ScriptState* script_state) const;

  void Trace(Visitor* visitor) const override;

 private:
  String prevented_;
  String src_;
  String id_;
  String name_;
  String url_;
  Member<V8ObservableArrayString> reasons_;
  Member<V8ObservableArrayNotRestoredReasons> children_;
};

}  // namespace blink

#endif  // #define
        // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NOT_RESTORED_REASONS_H_
