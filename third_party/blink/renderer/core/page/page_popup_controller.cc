/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/page_popup_controller.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

const char PagePopupController::kSupplementName[] = "PagePopupController";

PagePopupController* PagePopupController::From(Page& page) {
  return Supplement<Page>::From<PagePopupController>(page);
}

PagePopupController::PagePopupController(Page& page,
                                         PagePopup& popup,
                                         PagePopupClient* client)
    : Supplement(page), popup_(popup), popup_client_(client) {
  DCHECK(client);
  ProvideTo(page, this);
}

void PagePopupController::setValueAndClosePopup(int num_value,
                                                const String& string_value) {
  if (popup_client_)
    popup_client_->SetValueAndClosePopup(num_value, string_value);
}

void PagePopupController::setValue(const String& value) {
  if (popup_client_)
    popup_client_->SetValue(value);
}

void PagePopupController::closePopup() {
  if (popup_client_)
    popup_client_->CancelPopup();
}

String PagePopupController::localizeNumberString(const String& number_string) {
  if (popup_client_)
    return popup_client_->GetLocale().ConvertToLocalizedNumber(number_string);
  return number_string;
}

String PagePopupController::formatMonth(int year, int zero_base_month) {
  if (!popup_client_)
    return g_empty_string;
  DateComponents date;
  date.SetMonthsSinceEpoch((year - 1970) * 12.0 + zero_base_month);
  return popup_client_->GetLocale().FormatDateTime(date,
                                                   Locale::kFormatTypeMedium);
}

String PagePopupController::formatShortMonth(int year, int zero_base_month) {
  if (!popup_client_)
    return g_empty_string;
  DateComponents date;
  date.SetMonthsSinceEpoch((year - 1970) * 12.0 + zero_base_month);
  return popup_client_->GetLocale().FormatDateTime(date,
                                                   Locale::kFormatTypeShort);
}

String PagePopupController::formatWeek(int year,
                                       int week_number,
                                       const String& localized_date_string) {
  if (!popup_client_)
    return g_empty_string;
  DateComponents week;
  bool set_week_result = week.SetWeek(year, week_number);
  DCHECK(set_week_result);
  String localized_week = popup_client_->GetLocale().FormatDateTime(week);
  return popup_client_->GetLocale().QueryString(
      IDS_AX_CALENDAR_WEEK_DESCRIPTION, localized_week, localized_date_string);
}

void PagePopupController::ClearPagePopupClient() {
  popup_client_ = nullptr;
  popup_origin_.reset();
}

void PagePopupController::setWindowRect(int x, int y, int width, int height) {
  popup_.SetWindowRect(gfx::Rect(x, y, width, height));

  popup_origin_ = gfx::Point(x, y);
  popup_client_->SetMenuListOptionsBoundsInAXTree(options_bounds_,
                                                  *popup_origin_);
}

void PagePopupController::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Page>::Trace(visitor);
}

void PagePopupController::setMenuListOptionsBoundsInAXTree(
    HeapVector<Member<DOMRect>>& options_bounds,
    bool children_updated) {
  options_bounds_.clear();
  for (auto option_bounds : options_bounds) {
    options_bounds_.emplace_back(
        gfx::Rect(option_bounds->x(), option_bounds->y(),
                  option_bounds->width(), option_bounds->height()));
  }

  // On the first layout, setWindowRect handles the first call to set the bounds
  // in the tree. If there is a second layout (this happens when there are too
  // many children to process in one layout), the updated bounds are sent to the
  // tree here.
  if (popup_origin_ && children_updated) {
    popup_client_->SetMenuListOptionsBoundsInAXTree(options_bounds_,
                                                    *popup_origin_);
  }
}

// static
CSSFontSelector* PagePopupController::CreateCSSFontSelector(
    Document& popup_document) {
  LocalFrame* frame = popup_document.GetFrame();
  DCHECK(frame);
  DCHECK(frame->PagePopupOwner());

  auto* controller = PagePopupController::From(*frame->GetPage());

  DCHECK(controller->popup_client_);
  return controller->popup_client_->CreateCSSFontSelector(popup_document);
}

}  // namespace blink
