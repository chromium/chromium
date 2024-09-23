// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_
#define FUCHSIA_WEB_WEBENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/synchronization/lock.h"
#include "content/public/common/content_client.h"

namespace embedder_support {
class OriginTrialPolicyImpl;
}

class WebEngineContentClient : public content::ContentClient {
 public:
  WebEngineContentClient();
  ~WebEngineContentClient() override;

  WebEngineContentClient(const WebEngineContentClient&) = delete;
  WebEngineContentClient& operator=(const WebEngineContentClient&) = delete;

  // content::ContentClient implementation.
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
  void AddAdditionalSchemes(Schemes* schemes) override;

 private:
  // Used for lazy initialization of |origin_trial_policy_|.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<embedder_support::OriginTrialPolicyImpl> origin_trial_policy_
      GUARDED_BY(origin_trial_policy_lock_);
};

#endif  // FUCHSIA_WEB_WEBENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_
