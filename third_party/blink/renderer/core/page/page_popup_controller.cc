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
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

PagePopupController::PagePopupController(PagePopup& popup,
                                         PagePopupClient* client)
    : popup_(popup), popup_client_(client) {
  DCHECK(client);
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

void PagePopupController::selectFontsFromOwnerDocument(
    Document* target_document) {
  DCHECK(target_document);
  if (popup_client_)
    popup_client_->SelectFontsFromOwnerDocument(*target_document);
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
}

void PagePopupController::setWindowRect(int x, int y, int width, int height) {
  popup_.SetWindowRect(IntRect(x, y, width, height));
}

}  // namespace blink
