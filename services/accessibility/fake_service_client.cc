// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/fake_service_client.h"

#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ax {
FakeServiceClient::FakeServiceClient(mojom::AccessibilityService* service)
    : service_(service) {
  desktop_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
}

FakeServiceClient::~FakeServiceClient() = default;

void FakeServiceClient::BindAutomation(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) {
  automation_remotes_.Add(std::move(automation));
  if (automation_bound_closure_) {
    std::move(automation_bound_closure_).Run();
  }
}

void FakeServiceClient::BindAutomationClient(
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_receivers_.Add(this, std::move(automation_client));
}

void FakeServiceClient::Enable(EnableCallback callback) {
  std::move(callback).Run(desktop_tree_id_);
}

void FakeServiceClient::Disable() {
  num_disable_called_++;
}

void FakeServiceClient::EnableChildTree(const ui::AXTreeID& tree_id) {}

void FakeServiceClient::PerformAction(const ui::AXActionData& data) {
  if (perform_action_called_callback_) {
    std::move(perform_action_called_callback_).Run(data);
  }
}

#if BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)
void FakeServiceClient::BindAutoclickClient(
    mojo::PendingReceiver<ax::mojom::AutoclickClient>
        autoclick_client_reciever) {
  autoclick_client_recievers_.Add(this, std::move(autoclick_client_reciever));
}
void FakeServiceClient::BindSpeechRecognition(
    mojo::PendingReceiver<ax::mojom::SpeechRecognition> sr_receiver) {
  sr_receivers_.Add(this, std::move(sr_receiver));
}

void FakeServiceClient::BindTts(
    mojo::PendingReceiver<ax::mojom::Tts> tts_receiver) {
  tts_receivers_.Add(this, std::move(tts_receiver));
}

void FakeServiceClient::BindUserInput(
    mojo::PendingReceiver<mojom::UserInput> ui_receiver) {
  ui_receivers_.Add(this, std::move(ui_receiver));
}

void FakeServiceClient::BindUserInterface(
    mojo::PendingReceiver<mojom::UserInterface> ux_receiver) {
  ux_receivers_.Add(this, std::move(ux_receiver));
}

void FakeServiceClient::HandleScrollableBoundsForPointFound(
    const gfx::Rect& bounds) {
  if (scrollable_bounds_for_point_callback_) {
    scrollable_bounds_for_point_callback_.Run(bounds);
  }
}

void FakeServiceClient::BindAutoclick(BindAutoclickCallback callback) {
  std::move(callback).Run(autoclick_remote_.BindNewPipeAndPassReceiver());
}

void FakeServiceClient::Start(ax::mojom::StartOptionsPtr options,
                              StartCallback callback) {
  auto info = mojom::SpeechRecognitionStartInfo::New();
  info->type = mojom::SpeechRecognitionType::kNetwork;
  if (speech_recognition_start_error_.has_value()) {
    info->observer_or_error = mojom::ObserverOrError::NewError(
        speech_recognition_start_error_.value());
  } else {
    info->observer_or_error = mojom::ObserverOrError::NewObserver(
        sr_event_observer_.BindNewPipeAndPassReceiver());
  }

  std::move(callback).Run(std::move(info));
  if (speech_recognition_start_callback_) {
    speech_recognition_start_callback_.Run();
  }
}

void FakeServiceClient::Stop(ax::mojom::StopOptionsPtr options,
                             StopCallback callback) {
  std::move(callback).Run(speech_recognition_stop_error_);
}

void FakeServiceClient::BindAccessibilityFileLoader(
    mojo::PendingReceiver<ax::mojom::AccessibilityFileLoader>
        file_loader_receiver) {
  DCHECK(!file_loader_.is_bound());
  file_loader_.Bind(std::move(file_loader_receiver));
}

void FakeServiceClient::Load(const base::FilePath& path,
                             LoadCallback callback) {
  // TODO(crbug.com/40936729): Implement file loading for
  // FakeAccessibilityServiceClient.
}

void FakeServiceClient::Speak(const std::string& utterance,
                              ax::mojom::TtsOptionsPtr options,
                              SpeakCallback callback) {
  auto result = mojom::TtsSpeakResult::New();
  result->error = mojom::TtsError::kNoError;
  result->utterance_client = tts_utterance_client_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(result));
  if (tts_speak_callback_) {
    tts_speak_callback_.Run(utterance, std::move(options));
  }
}

void FakeServiceClient::Stop() {
  if (!tts_utterance_client_.is_bound()) {
    return;
  }
  auto event = mojom::TtsEvent::New();
  event->type = mojom::TtsEventType::kInterrupted;
  tts_utterance_client_->OnEvent(std::move(event));
  tts_utterance_client_.reset();
}

void FakeServiceClient::Pause() {
  if (!tts_utterance_client_.is_bound()) {
    return;
  }
  auto event = mojom::TtsEvent::New();
  event->type = mojom::TtsEventType::kPause;
  tts_utterance_client_->OnEvent(std::move(event));
}

void FakeServiceClient::Resume() {
  if (!tts_utterance_client_.is_bound()) {
    return;
  }
  auto event = mojom::TtsEvent::New();
  event->type = mojom::TtsEventType::kResume;
  tts_utterance_client_->OnEvent(std::move(event));
}

void FakeServiceClient::IsSpeaking(IsSpeakingCallback callback) {
  std::move(callback).Run(tts_utterance_client_.is_bound());
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

void FakeServiceClient::SendSyntheticKeyEventForShortcutOrNavigation(
    mojom::SyntheticKeyEventPtr key_event) {
  key_events_.emplace_back(std::move(key_event));
  if (synthetic_key_event_callback_) {
    synthetic_key_event_callback_.Run();
  }
}

void FakeServiceClient::SendSyntheticMouseEvent(
    mojom::SyntheticMouseEventPtr mouse_event) {
  mouse_events_.emplace_back(mouse_event.Clone());
  if (synthetic_mouse_event_callback_) {
    synthetic_mouse_event_callback_.Run();
  }
}

void FakeServiceClient::DarkenScreen(bool darken) {
  if (darken_screen_callback_) {
    darken_screen_callback_.Run(darken);
  }
}

void FakeServiceClient::OpenSettingsSubpage(const std::string& subpage) {
  if (open_settings_subpage_callback_) {
    open_settings_subpage_callback_.Run(subpage);
  }
}

void FakeServiceClient::ShowConfirmationDialog(
    const std::string& title,
    const std::string& description,
    const std::optional<std::string>& cancel_name,
    ShowConfirmationDialogCallback callback) {
  std::move(callback).Run(true);
}

void FakeServiceClient::SetFocusRings(
    std::vector<mojom::FocusRingInfoPtr> focus_rings,
    mojom::AssistiveTechnologyType at_type) {
  focus_rings_for_type_[at_type] = std::move(focus_rings);
  if (focus_rings_callback_) {
    focus_rings_callback_.Run();
  }
}

void FakeServiceClient::SetHighlights(const std::vector<gfx::Rect>& rects,
                                      SkColor color) {
  if (highlights_callback_) {
    highlights_callback_.Run(rects, color);
  }
}

void FakeServiceClient::SetVirtualKeyboardVisible(bool is_visible) {
  if (virtual_keyboard_visible_callback_) {
    virtual_keyboard_visible_callback_.Run(is_visible);
  }
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

void FakeServiceClient::SetPerformActionCalledCallback(
    base::OnceCallback<void(const ui::AXActionData&)> callback) {
  perform_action_called_callback_ = std::move(callback);
}

#if BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)
void FakeServiceClient::RequestScrollableBoundsForPoint(
    const gfx::Point& point) {
  autoclick_remote_->RequestScrollableBoundsForPoint(point);
}

void FakeServiceClient::SetScrollableBoundsForPointFoundCallback(
    base::RepeatingCallback<void(const gfx::Rect&)> callback) {
  scrollable_bounds_for_point_callback_ = std::move(callback);
}

void FakeServiceClient::SetSpeechRecognitionStartCallback(
    base::RepeatingCallback<void()> callback) {
  speech_recognition_start_callback_ = std::move(callback);
}

void FakeServiceClient::SendSpeechRecognitionStopEvent() {
  sr_event_observer_->OnStop();
}

void FakeServiceClient::SendSpeechRecognitionResultEvent() {
  auto result = ax::mojom::SpeechRecognitionResultEvent::New();
  result->transcript = "Hello world";
  result->is_final = true;
  sr_event_observer_->OnResult(std::move(result));
}

void FakeServiceClient::SendSpeechRecognitionErrorEvent() {
  auto event = ax::mojom::SpeechRecognitionErrorEvent::New();
  event->message = "Goodnight world";
  sr_event_observer_->OnError(std::move(event));
}

void FakeServiceClient::SetSpeechRecognitionStartError(
    const std::string& error) {
  speech_recognition_start_error_ = error;
}

void FakeServiceClient::SetSpeechRecognitionStopError(
    const std::string& error) {
  speech_recognition_stop_error_ = error;
}

void FakeServiceClient::SetTtsSpeakCallback(
    base::RepeatingCallback<void(const std::string&, mojom::TtsOptionsPtr)>
        callback) {
  tts_speak_callback_ = std::move(callback);
}

void FakeServiceClient::SendTtsUtteranceEvent(mojom::TtsEventPtr tts_event) {
  CHECK(tts_utterance_client_.is_bound());
  tts_utterance_client_->OnEvent(std::move(tts_event));
}

void FakeServiceClient::SetSyntheticKeyEventCallback(
    base::RepeatingCallback<void()> callback) {
  synthetic_key_event_callback_ = std::move(callback);
}

void FakeServiceClient::SetSyntheticMouseEventCallback(
    base::RepeatingCallback<void()> callback) {
  synthetic_mouse_event_callback_ = std::move(callback);
}

const std::vector<mojom::SyntheticKeyEventPtr>&
FakeServiceClient::GetKeyEvents() const {
  return key_events_;
}

const std::vector<mojom::SyntheticMouseEventPtr>&
FakeServiceClient::GetMouseEvents() const {
  return mouse_events_;
}

void FakeServiceClient::SetDarkenScreenCallback(
    base::RepeatingCallback<void(bool darken)> callback) {
  darken_screen_callback_ = std::move(callback);
}

void FakeServiceClient::SetOpenSettingsSubpageCallback(
    base::RepeatingCallback<void(const std::string& subpage)> callback) {
  open_settings_subpage_callback_ = std::move(callback);
}

void FakeServiceClient::SetFocusRingsCallback(
    base::RepeatingCallback<void()> callback) {
  focus_rings_callback_ = std::move(callback);
}

void FakeServiceClient::SetHighlightsCallback(
    base::RepeatingCallback<void(const std::vector<gfx::Rect>& rects,
                                 SkColor color)> callback) {
  highlights_callback_ = callback;
}

void FakeServiceClient::SetVirtualKeyboardVisibleCallback(
    base::RepeatingCallback<void(bool is_visible)> callback) {
  virtual_keyboard_visible_callback_ = std::move(callback);
}

bool FakeServiceClient::UserInterfaceIsBound() const {
  return ux_receivers_.size();
}

const std::vector<mojom::FocusRingInfoPtr>&
FakeServiceClient::GetFocusRingsForType(
    mojom::AssistiveTechnologyType type) const {
  return focus_rings_for_type_.at(type);
}

void FakeServiceClient::SendAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  for (auto& remote : automation_remotes_) {
    remote->DispatchAccessibilityEvents(tree_id, updates, mouse_location,
                                        events);
  }
}

void FakeServiceClient::SendTreeDestroyedEvent(const ui::AXTreeID& tree_id) {
  for (auto& remote : automation_remotes_) {
    remote->DispatchTreeDestroyedEvent(tree_id);
  }
}

void FakeServiceClient::SendActionResult(const ui::AXActionData& data,
                                         bool result) {
  for (auto& remote : automation_remotes_) {
    remote->DispatchActionResult(data, result);
  }
}

#endif  // BUILDFLAG(SUPPORTS_OS_ACCESSIBILITY_SERVICE)

bool FakeServiceClient::AccessibilityServiceClientIsBound() const {
  return a11y_client_receiver_.is_bound();
}

}  // namespace ax
