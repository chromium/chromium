// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CREATE_ELEMENT_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CREATE_ELEMENT_FLAGS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

class CreateElementFlags {
  STACK_ALLOCATED();

 public:
  bool IsCreatedByParser() const { return created_by_parser_; }
  bool IsAsyncCustomElements() const { return async_custom_elements_; }
  bool IsCustomElementsV1() const { return custom_elements_v1_; }
  bool IsCustomElementsV0() const { return custom_elements_v0_; }
  bool WasAlreadyStarted() const { return already_started_; }

  // https://html.spec.whatwg.org/C/#create-an-element-for-the-token
  static CreateElementFlags ByParser() {
    return CreateElementFlags().SetCreatedByParser(true);
  }

  // https://dom.spec.whatwg.org/#concept-node-clone
  static CreateElementFlags ByCloneNode() {
    return CreateElementFlags().SetAsyncCustomElements();
  }

  // https://dom.spec.whatwg.org/#dom-document-createelement
  static CreateElementFlags ByCreateElement() { return CreateElementFlags(); }
  static CreateElementFlags ByCreateElementV1() {
    return CreateElementFlags().SetCustomElementsV1Only();
  }
  static CreateElementFlags ByCreateElementV0() {
    return CreateElementFlags().SetCustomElementsV0Only();
  }

  // https://html.spec.whatwg.org/C/#create-an-element-for-the-token
  static CreateElementFlags ByFragmentParser() {
    return CreateElementFlags()
        .SetCreatedByParser(true)
        .SetAsyncCustomElements();
  }

  // Construct an instance indicating default behavior.
  CreateElementFlags()
      : created_by_parser_(false),
        async_custom_elements_(false),
        custom_elements_v1_(true),
        custom_elements_v0_(true),
        already_started_(false) {}

  CreateElementFlags& SetCreatedByParser(bool flag) {
    created_by_parser_ = flag;
    return *this;
  }

  // For <script>.
  CreateElementFlags& SetAlreadyStarted(bool flag) {
    already_started_ = flag;
    return *this;
  }

 private:
  CreateElementFlags& SetAsyncCustomElements() {
    async_custom_elements_ = true;
    return *this;
  }

  CreateElementFlags& SetCustomElementsV1Only() {
    custom_elements_v1_ = true;
    custom_elements_v0_ = false;
    return *this;
  }

  CreateElementFlags& SetCustomElementsV0Only() {
    custom_elements_v0_ = true;
    custom_elements_v1_ = false;
    return *this;
  }

  bool created_by_parser_ : 1;
  bool async_custom_elements_ : 1;
  bool custom_elements_v1_ : 1;
  bool custom_elements_v0_ : 1;

  bool already_started_ : 1;
};

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CREATE_ELEMENT_FLAGS_H_
