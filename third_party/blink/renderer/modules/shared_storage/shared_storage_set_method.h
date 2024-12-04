// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_SET_METHOD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_SET_METHOD_H_

#include "services/network/public/mojom/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class SharedStorageSetMethodOptions;

class MODULES_EXPORT SharedStorageSetMethod : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SharedStorageSetMethod* Create(ScriptState*,
                                        const String& key,
                                        const String& value,
                                        ExceptionState&);
  static SharedStorageSetMethod* Create(ScriptState*,
                                        const String& key,
                                        const String& value,
                                        const SharedStorageSetMethodOptions*,
                                        ExceptionState&);

  SharedStorageSetMethod(ScriptState*,
                         const String& key,
                         const String& value,
                         const SharedStorageSetMethodOptions*,
                         ExceptionState&);

  // Returns std::move(method_with_options_).
  network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr
  TakeMojomMethod();

  void Trace(Visitor*) const override;

 private:
  network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr
      method_with_options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_SET_METHOD_H_
