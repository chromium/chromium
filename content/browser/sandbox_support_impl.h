// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/sandbox_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Performs privileged operations on behalf of sandboxed child processes.
// This is used to implement the blink::WebSandboxSupport interface in the
// renderer. However all child process types have access to this interface.
// This class lives on the IO thread and is owned by the Mojo interface
// registry.
class CONTENT_EXPORT SandboxSupportImpl : public mojom::SandboxSupport {
 public:
  SandboxSupportImpl();

  SandboxSupportImpl(const SandboxSupportImpl&) = delete;
  SandboxSupportImpl& operator=(const SandboxSupportImpl&) = delete;

  ~SandboxSupportImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SandboxSupport> receiver);

  // content::mojom::SandboxSupport:
#if BUILDFLAG(IS_MAC)
  void GetSystemColors(GetSystemColorsCallback callback) override;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  void LcidAndFirstDayOfWeek(const std::u16string& locale,
                             const std::u16string& default_language,
                             bool force_defaults,
                             LcidAndFirstDayOfWeekCallback callback) override;
  void DigitsAndSigns(uint32_t lcid,
                      bool force_defaults,
                      DigitsAndSignsCallback callback) override;
  void LocaleStrings(uint32_t lcid,
                     bool force_defaults,
                     LcTypeStrings collection,
                     LocaleStringsCallback callback) override;
  void LocaleString(uint32_t lcid,
                    bool force_defaults,
                    LcTypeString type,
                    LocaleStringCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)

 private:
  mojo::ReceiverSet<mojom::SandboxSupport> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_
