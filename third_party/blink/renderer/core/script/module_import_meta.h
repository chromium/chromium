// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_IMPORT_META_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_IMPORT_META_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Represents import.meta data structure, which is the return value of
// https://html.spec.whatwg.org/C/#hostgetimportmetaproperties
class CORE_EXPORT ModuleImportMeta final {
  STACK_ALLOCATED();

 public:
  explicit ModuleImportMeta(const String& url) : url_(url) {}

  const String& Url() const { return url_; }

 private:
  const String url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_IMPORT_META_H_
