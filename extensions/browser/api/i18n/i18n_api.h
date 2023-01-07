// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_I18N_I18N_API_H_
#define EXTENSIONS_BROWSER_API_I18N_I18N_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class I18nGetAcceptLanguagesFunction : public ExtensionFunction {
  ~I18nGetAcceptLanguagesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("i18n.getAcceptLanguages", I18N_GETACCEPTLANGUAGES)
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_I18N_I18N_API_H_
