// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_

#include "third_party/blink/renderer/extensions/chromeos/extensions_chromeos_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class EXTENSIONS_CHROMEOS_EXPORT ChromeOS : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool myEmbedderFunction(bool testArg);

 private:
  ChromeOS() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_
