// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementRegistry;

// Implement CustomElementDefinitionBuilder to provide
// technology-specific steps for CustomElementRegistry.define.
// https://html.spec.whatwg.org/C/#dom-customelementsregistry-define
class CORE_EXPORT CustomElementDefinitionBuilder {
  STACK_ALLOCATED();

 public:
  // This API necessarily sounds JavaScript specific; this implements
  // some steps of the CustomElementRegistry.define process, which
  // are defined in terms of JavaScript.

  // Check the constructor is valid. Return false if processing
  // should not proceed.
  virtual bool CheckConstructorIntrinsics() = 0;

  // Check the constructor is not already registered in the calling
  // registry. Return false if processing should not proceed.
  virtual bool CheckConstructorNotRegistered() = 0;

  // Cache properties for build to use. Return false if processing
  // should not proceed.
  virtual bool RememberOriginalProperties() = 0;

  // Produce the definition. This must produce a definition.
  virtual CustomElementDefinition* Build(const CustomElementDescriptor&,
                                         CustomElementDefinition::Id) = 0;

 protected:
  CustomElementDefinitionBuilder() = default;

  DISALLOW_COPY_AND_ASSIGN(CustomElementDefinitionBuilder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_
