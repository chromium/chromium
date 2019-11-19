/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_TEXT_CONTROL_INNER_ELEMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_TEXT_CONTROL_INNER_ELEMENTS_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class TextControlInnerContainer final : public HTMLDivElement {
 public:
  explicit TextControlInnerContainer(Document&);

 protected:
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool TypeShouldForceLegacyLayout() const final { return true; }
};

class EditingViewPortElement final : public HTMLDivElement {
 public:
  explicit EditingViewPortElement(Document&);

 protected:
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject() override;

 private:
  bool TypeShouldForceLegacyLayout() const final { return true; }
  bool SupportsFocus() const override { return false; }
};

class TextControlInnerEditorElement final : public HTMLDivElement {
 public:
  explicit TextControlInnerEditorElement(Document&);

  void DefaultEventHandler(Event&) override;

  void SetVisibility(bool is_visible);
  scoped_refptr<ComputedStyle> CreateInnerEditorStyle() const;

 private:
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool TypeShouldForceLegacyLayout() const final { return true; }
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject() override;
  bool SupportsFocus() const override { return false; }
  bool is_visible_ = true;
};

class SearchFieldCancelButtonElement final : public HTMLDivElement {
 public:
  explicit SearchFieldCancelButtonElement(Document&);

  void DefaultEventHandler(Event&) override;
  bool WillRespondToMouseClickEvents() override;

 private:
  bool TypeShouldForceLegacyLayout() const final { return true; }
  bool SupportsFocus() const override { return false; }
};

class PasswordRevealButtonElement final : public HTMLDivElement {
 public:
  explicit PasswordRevealButtonElement(Document&);

  void DefaultEventHandler(Event&) override;
  bool WillRespondToMouseClickEvents() override;

 private:
  bool SupportsFocus() const override { return false; }
};

}  // namespace blink

#endif
