// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_WIN_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_WIN_H_

#include "content/common/sandbox_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/win/web_sandbox_support.h"

namespace content {

// Implementation of the interface used by Blink to upcall to the privileged
// process (browser) for handling requests for data that are not allowed within
// the sandbox.
class WebSandboxSupportWin : public blink::WebSandboxSupport {
 public:
  WebSandboxSupportWin();

  WebSandboxSupportWin(const WebSandboxSupportWin&) = delete;
  WebSandboxSupportWin& operator=(const WebSandboxSupportWin&) = delete;

  ~WebSandboxSupportWin() override;

  bool IsLocaleProxyEnabled() override;
  std::pair<LCID, unsigned> LcidAndFirstDayOfWeek(
      blink::WebString locale,
      blink::WebString default_language,
      bool force_defaults) override;

  std::unique_ptr<blink::WebSandboxSupport::LocaleInitData> DigitsAndSigns(
      LCID lcid,
      bool force_defaults) override;

  std::vector<blink::WebString> MonthLabels(LCID lcid,
                                            bool force_defaults) override;
  std::vector<blink::WebString> WeekDayShortLabels(
      LCID lcid,
      bool force_defaults) override;
  std::vector<blink::WebString> ShortMonthLabels(LCID lcid,
                                                 bool force_defaults) override;
  std::vector<blink::WebString> AmPmLabels(LCID lcid,
                                           bool force_defaults) override;
  blink::WebString LocaleString(LCID lcid,
                                LCTYPE type,
                                bool force_defaults) override;

 private:
  std::vector<blink::WebString> LocaleStrings(
      LCID lcid,
      bool force_defaults,
      mojom::SandboxSupport::LcTypeStrings collection);
  mojo::Remote<mojom::SandboxSupport> sandbox_support_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_WIN_H_
