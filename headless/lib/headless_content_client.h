// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/synchronization/lock.h"
#include "content/public/common/content_client.h"

namespace embedder_support {
class OriginTrialPolicyImpl;
}

namespace headless {

class HeadlessContentClient : public content::ContentClient {
 public:
  HeadlessContentClient();

  HeadlessContentClient(const HeadlessContentClient&) = delete;
  HeadlessContentClient& operator=(const HeadlessContentClient&) = delete;

  ~HeadlessContentClient() override;

  // content::ContentClient implementation:
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;

 private:
  // Used to lock when |origin_trial_policy_| is initialized.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<embedder_support::OriginTrialPolicyImpl> origin_trial_policy_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_
