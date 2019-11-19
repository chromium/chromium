/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class Document;
class ExceptionState;
class QualifiedName;

class HTMLDialogElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLDialogElement(Document&);

  void close(const String& return_value = String());
  void show();
  void showModal(ExceptionState&);
  void RemovedFrom(ContainerNode&) override;

  // NotCentered means do not center the dialog. Centered means the dialog has
  // been centered and centeredPosition() is set. NeedsCentering means attempt
  // to center on the next layout, then set to Centered or NotCentered.
  enum CenteringMode { kNotCentered, kCentered, kNeedsCentering };
  CenteringMode GetCenteringMode() const { return centering_mode_; }
  LayoutUnit CenteredPosition() const {
    DCHECK_EQ(centering_mode_, kCentered);
    return centered_position_;
  }
  void SetCentered(LayoutUnit centered_position);
  void SetNotCentered();

  String returnValue() const { return return_value_; }
  void setReturnValue(const String& return_value) {
    return_value_ = return_value;
  }

 private:
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void DefaultEventHandler(Event&) override;

  void ForceLayoutForCentering();

  void ScheduleCloseEvent();

  CenteringMode centering_mode_;
  LayoutUnit centered_position_;
  String return_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_
