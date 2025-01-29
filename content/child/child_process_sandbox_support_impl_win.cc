// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "content/public/child/child_thread.h"

namespace content {

WebSandboxSupportWin::WebSandboxSupportWin() {
  // Not available in some browser tests.
  auto* child_thread = ChildThread::Get();
  if (child_thread) {
    child_thread->BindHostReceiver(
        sandbox_support_.BindNewPipeAndPassReceiver());
  }
}

WebSandboxSupportWin::~WebSandboxSupportWin() = default;

bool WebSandboxSupportWin::IsLocaleProxyEnabled() {
  return base::FeatureList::IsEnabled(content::mojom::kWinSboxProxyLocale);
}

std::pair<LCID, unsigned> WebSandboxSupportWin::LcidAndFirstDayOfWeek(
    blink::WebString locale,
    blink::WebString default_language,
    bool force_defaults) {
  uint32_t lcid;
  uint32_t first_day_of_week;
  CHECK(sandbox_support_->LcidAndFirstDayOfWeek(
      locale.Utf16(), default_language.Utf16(), force_defaults, &lcid,
      &first_day_of_week));
  return {lcid, first_day_of_week};
}

std::unique_ptr<blink::WebSandboxSupport::LocaleInitData>
WebSandboxSupportWin::DigitsAndSigns(LCID lcid, bool force_defaults) {
  auto init_data = std::make_unique<blink::WebSandboxSupport::LocaleInitData>();
  uint32_t digit_substitution = 0;
  std::u16string digits;
  std::u16string decimal;
  std::u16string thousand;
  std::u16string negative_sign;
  uint32_t negnumber = 0;

  CHECK(sandbox_support_->DigitsAndSigns(
      lcid, force_defaults, &digit_substitution, &digits, &decimal, &thousand,
      &negative_sign, &negnumber));
  init_data->digit_substitution = digit_substitution;
  init_data->digits = blink::WebString(digits);
  init_data->decimal = blink::WebString(decimal);
  init_data->thousand = blink::WebString(thousand);
  init_data->negative_sign = blink::WebString(negative_sign);
  init_data->negnumber = negnumber;
  return init_data;
}

std::vector<blink::WebString> WebSandboxSupportWin::LocaleStrings(
    LCID lcid,
    bool force_defaults,
    mojom::SandboxSupport::LcTypeStrings collection) {
  std::vector<std::u16string> results;
  CHECK(sandbox_support_->LocaleStrings(lcid, force_defaults, collection,
                                        &results));
  std::vector<blink::WebString> ret;
  ret.reserve(results.size());
  for (const auto& str : results) {
    ret.push_back(blink::WebString(str));
  }
  return ret;
}

std::vector<blink::WebString> WebSandboxSupportWin::MonthLabels(
    LCID lcid,
    bool force_defaults) {
  return LocaleStrings(lcid, force_defaults,
                       mojom::SandboxSupport::LcTypeStrings::kMonths);
}

std::vector<blink::WebString> WebSandboxSupportWin::WeekDayShortLabels(
    LCID lcid,
    bool force_defaults) {
  return LocaleStrings(lcid, force_defaults,
                       mojom::SandboxSupport::LcTypeStrings::kShortWeekDays);
}

std::vector<blink::WebString> WebSandboxSupportWin::ShortMonthLabels(
    LCID lcid,
    bool force_defaults) {
  return LocaleStrings(lcid, force_defaults,
                       mojom::SandboxSupport::LcTypeStrings::kShortMonths);
}

std::vector<blink::WebString> WebSandboxSupportWin::AmPmLabels(
    LCID lcid,
    bool force_defaults) {
  return LocaleStrings(lcid, force_defaults,
                       mojom::SandboxSupport::LcTypeStrings::kAmPm);
}

blink::WebString WebSandboxSupportWin::LocaleString(LCID lcid,
                                                    LCTYPE type,
                                                    bool force_defaults) {
  mojom::SandboxSupport::LcTypeString wanted;
  switch (type) {
    case LOCALE_SSHORTDATE:
      wanted = mojom::SandboxSupport::LcTypeString::kShortDate;
      break;
    case LOCALE_SYEARMONTH:
      wanted = mojom::SandboxSupport::LcTypeString::kYearMonth;
      break;
    case LOCALE_STIMEFORMAT:
      wanted = mojom::SandboxSupport::LcTypeString::kTimeFormat;
      break;
    case LOCALE_SSHORTTIME:
      wanted = mojom::SandboxSupport::LcTypeString::kShortTime;
      break;
    default:
      NOTREACHED();
  }
  std::u16string str;
  CHECK(sandbox_support_->LocaleString(lcid, force_defaults, wanted, &str));
  return blink::WebString(std::move(str));
}

}  // namespace content
