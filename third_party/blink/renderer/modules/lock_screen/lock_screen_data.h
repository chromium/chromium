// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;

class LockScreenData final : public ScriptWrappable,
                             public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  explicit LockScreenData(LocalDOMWindow&);
  ~LockScreenData() override;
  static ScriptPromise<LockScreenData> getLockScreenData(ScriptState*,
                                                         LocalDOMWindow&);

  // IDL Interface:
  ScriptPromise<IDLSequence<IDLString>> getKeys(ScriptState*);
  ScriptPromise<IDLAny> getData(ScriptState*, const String& key);
  ScriptPromise<IDLUndefined> setData(ScriptState*,
                                      const String& key,
                                      const String& data);
  ScriptPromise<IDLUndefined> deleteData(ScriptState*, const String& key);

  void Trace(Visitor* visitor) const override;

 private:
  // Fake data store for use in testing before implementation is complete.
  HashMap<String, String> fake_data_store_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_
