// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_CONTENT_CLIENT_H_
#define IOS_WEB_CONTENT_INIT_IOS_CONTENT_CLIENT_H_

#include <string_view>

#import "build/blink_buildflags.h"
#import "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#import "content/public/common/content_client.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class IOSContentClient : public content::ContentClient {
 public:
  IOSContentClient();
  IOSContentClient(const IOSContentClient&) = delete;
  IOSContentClient& operator=(const IOSContentClient&) = delete;
  ~IOSContentClient() override;

  // ContentClient implementation:
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
  std::u16string GetLocalizedString(int message_id) override;
  std::u16string GetLocalizedString(int message_id,
                                    const std::u16string& replacement) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;

 private:
  embedder_support::OriginTrialPolicyImpl origin_trial_policy_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_CONTENT_CLIENT_H_
