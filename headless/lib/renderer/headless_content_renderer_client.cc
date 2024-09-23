// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include <memory>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "content/public/common/web_identity.h"
#include "content/public/renderer/render_thread.h"
#include "headless/public/switches.h"
#include "media/base/video_codecs.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/renderer/print_render_frame_helper.h"
#include "headless/lib/renderer/headless_print_render_frame_helper_delegate.h"
#endif

namespace {

class HeadlessContentRendererUrlLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  HeadlessContentRendererUrlLoaderThrottleProvider()
      : main_thread_task_runner_(
            content::RenderThread::IsMainThread()
                ? base::SequencedTaskRunner::GetCurrentDefault()
                : nullptr) {}

  // This constructor works in conjunction with Clone().
  HeadlessContentRendererUrlLoaderThrottleProvider(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      base::PassKey<HeadlessContentRendererUrlLoaderThrottleProvider>)
      : main_thread_task_runner_(std::move(main_thread_task_runner)) {}

  std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
    return std::make_unique<HeadlessContentRendererUrlLoaderThrottleProvider>(
        main_thread_task_runner_,
        base::PassKey<HeadlessContentRendererUrlLoaderThrottleProvider>());
  }

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override {
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    if (local_frame_token.has_value()) {
      auto throttle =
          content::MaybeCreateIdentityUrlLoaderThrottle(base::BindRepeating(
              [](const blink::LocalFrameToken& token,
                 const scoped_refptr<base::SequencedTaskRunner>
                     main_thread_task_runner,
                 const url::Origin& origin,
                 blink::mojom::IdpSigninStatus status) {
                if (content::RenderThread::IsMainThread()) {
                  blink::SetIdpSigninStatus(token, origin, status);
                  return;
                }
                if (main_thread_task_runner) {
                  main_thread_task_runner->PostTask(
                      FROM_HERE, base::BindOnce(&blink::SetIdpSigninStatus,
                                                token, origin, status));
                }
              },
              local_frame_token.value(), main_thread_task_runner_));
      if (throttle) {
        throttles.push_back(std::move(throttle));
      }
    }

    return throttles;
  }

  void SetOnline(bool is_online) override {}

 private:
  // Set only when `this` was created on the main thread, or cloned from a
  // provider which was created on the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
};

}  // namespace

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() {
  const auto& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  if (command_line.HasSwitch(switches::kAllowVideoCodecs)) {
    video_codecs_allowlist_.emplace(
        base::ToLowerASCII(
            command_line.GetSwitchValueASCII(switches::kAllowVideoCodecs)),
        /*default_allow=*/false);
  }
}

HeadlessContentRendererClient::~HeadlessContentRendererClient() = default;

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<HeadlessPrintRenderFrameHelperDelegate>());
#endif
}

bool HeadlessContentRendererClient::IsSupportedVideoType(
    const media::VideoType& type) {
  const bool allowed_by_flags =
      !video_codecs_allowlist_ ||
      video_codecs_allowlist_->IsAllowed(
          base::ToLowerASCII(GetCodecName(type.codec)));
  // Besides being _allowed_, the codec actually has to be _supported_.
  return allowed_by_flags && ContentRendererClient::IsSupportedVideoType(type);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
HeadlessContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<HeadlessContentRendererUrlLoaderThrottleProvider>();
}

}  // namespace headless
