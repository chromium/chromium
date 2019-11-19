// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_API_H_
#define EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/virtual_keyboard.h"

namespace extensions {

class VirtualKeyboardRestrictFeaturesFunction : public ExtensionFunction {
 public:
  VirtualKeyboardRestrictFeaturesFunction();

  DECLARE_EXTENSION_FUNCTION("virtualKeyboard.restrictFeatures",
                             VIRTUALKEYBOARD_RESTRICTFEATURES)

 protected:
  ~VirtualKeyboardRestrictFeaturesFunction() override = default;
  // ExtensionFunction override:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardRestrictFeaturesFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_API_H_
