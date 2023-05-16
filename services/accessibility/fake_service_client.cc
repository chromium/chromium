// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/fake_service_client.h"

namespace ax {
FakeServiceClient::FakeServiceClient(mojom::AccessibilityService* service)
    : service_(service) {}

FakeServiceClient::~FakeServiceClient() = default;

void FakeServiceClient::BindAutomation(
    mojo::PendingRemote<ax::mojom::Automation> automation,
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_receivers_.Add(this, std::move(automation_client));
  automation_remotes_.Add(std::move(automation));
  if (automation_bound_closure_) {
    std::move(automation_bound_closure_).Run();
  }
}

#if BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)
void FakeServiceClient::BindTts(
    mojo::PendingReceiver<ax::mojom::Tts> tts_receiver) {
  tts_receivers_.Add(this, std::move(tts_receiver));
  if (tts_bound_closure_) {
    std::move(tts_bound_closure_).Run();
  }
}

void FakeServiceClient::GetVoices(GetVoicesCallback callback) {
  std::vector<ax::mojom::TtsVoicePtr> voices;

  // Create a voice with all event types.
  auto first_voice = ax::mojom::TtsVoice::New();
  first_voice->voice_name = "Lyra";
  first_voice->lang = "en-US", first_voice->remote = false;
  first_voice->engine_id = "us_toddler";
  first_voice->event_types = std::vector<mojom::TtsEventType>();
  for (int i = static_cast<int>(mojom::TtsEventType::kMinValue);
       i <= static_cast<int>(mojom::TtsEventType::kMaxValue); i++) {
    first_voice->event_types->emplace_back(static_cast<mojom::TtsEventType>(i));
  }

  // Create a voice with just two event types/
  auto second_voice = ax::mojom::TtsVoice::New();
  second_voice->voice_name = "Juno";
  second_voice->lang = "en-GB", second_voice->remote = true;
  second_voice->engine_id = "us_baby";
  second_voice->event_types = std::vector<mojom::TtsEventType>();
  second_voice->event_types->emplace_back(mojom::TtsEventType::kStart);
  second_voice->event_types->emplace_back(mojom::TtsEventType::kEnd);

  voices.emplace_back(std::move(first_voice));
  voices.emplace_back(std::move(second_voice));
  std::move(callback).Run(std::move(voices));
}
#endif  // BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)

void FakeServiceClient::BindAccessibilityServiceClientForTest() {
  if (service_) {
    service_->BindAccessibilityServiceClient(
        a11y_client_receiver_.BindNewPipeAndPassRemote());
  }
}

void FakeServiceClient::SetAutomationBoundClosure(base::OnceClosure closure) {
  automation_bound_closure_ = std::move(closure);
}

bool FakeServiceClient::AutomationIsBound() const {
  return automation_client_receivers_.size() && automation_remotes_.size();
}

#if BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)
void FakeServiceClient::SetTtsBoundClosure(base::OnceClosure closure) {
  tts_bound_closure_ = std::move(closure);
}

bool FakeServiceClient::TtsIsBound() const {
  return tts_receivers_.size();
}
#endif  // BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)

bool FakeServiceClient::AccessibilityServiceClientIsBound() const {
  return a11y_client_receiver_.is_bound();
}

}  // namespace ax
