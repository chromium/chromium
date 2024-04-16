// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/common/web_engine_content_client.h"

#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "fuchsia_web/common/fuchsia_dir_scheme.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

WebEngineContentClient::WebEngineContentClient() = default;

WebEngineContentClient::~WebEngineContentClient() = default;

std::u16string WebEngineContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::string_view WebEngineContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebEngineContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string WebEngineContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& WebEngineContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

blink::OriginTrialPolicy* WebEngineContentClient::GetOriginTrialPolicy() {
  if (!base::FeatureList::IsEnabled(features::kOriginTrials))
    return nullptr;

  // Prevent initialization race (see crbug.com/721144). There may be a
  // race when the policy is needed for worker startup (which happens on a
  // separate worker thread).
  base::AutoLock auto_lock(origin_trial_policy_lock_);
  if (!origin_trial_policy_) {
    origin_trial_policy_ =
        std::make_unique<embedder_support::OriginTrialPolicyImpl>();
  }
  return origin_trial_policy_.get();
}

void WebEngineContentClient::AddAdditionalSchemes(Schemes* schemes) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableContentDirectories)) {
    schemes->standard_schemes.push_back(kFuchsiaDirScheme);
  }
}
