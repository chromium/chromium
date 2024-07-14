/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/base/ime/mojom/ime_types.mojom-blink.h"

namespace blink {

static ui::TextInputType ToTextInputType(InputType::Type source) {
  switch (source) {
    case InputType::Type::kDate:
      return ui::TextInputType::TEXT_INPUT_TYPE_DATE;
    case InputType::Type::kDateTimeLocal:
      return ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
    case InputType::Type::kMonth:
      return ui::TextInputType::TEXT_INPUT_TYPE_MONTH;
    case InputType::Type::kTime:
      return ui::TextInputType::TEXT_INPUT_TYPE_TIME;
    case InputType::Type::kWeek:
      return ui::TextInputType::TEXT_INPUT_TYPE_WEEK;
    default:
      return ui::TextInputType::TEXT_INPUT_TYPE_NONE;
  }
}

ExternalDateTimeChooser::~ExternalDateTimeChooser() = default;

void ExternalDateTimeChooser::Trace(Visitor* visitor) const {
  visitor->Trace(date_time_chooser_);
  visitor->Trace(client_);
  DateTimeChooser::Trace(visitor);
}

ExternalDateTimeChooser::ExternalDateTimeChooser(DateTimeChooserClient* client)
    : date_time_chooser_(client->OwnerElement().GetExecutionContext()),
      client_(client) {
  DCHECK(!RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  DCHECK(client);
}

void ExternalDateTimeChooser::OpenDateTimeChooser(
    LocalFrame* frame,
    const DateTimeChooserParameters& parameters) {
  auto date_time_dialog_value = mojom::blink::DateTimeDialogValue::New();
  date_time_dialog_value->dialog_type = ToTextInputType(parameters.type);
  date_time_dialog_value->dialog_value = parameters.double_value;
  date_time_dialog_value->minimum = parameters.minimum;
  date_time_dialog_value->maximum = parameters.maximum;
  date_time_dialog_value->step = parameters.step;
  for (const auto& suggestion : parameters.suggestions) {
    date_time_dialog_value->suggestions.push_back(suggestion->Clone());
  }

  auto response_callback = WTF::BindOnce(
      &ExternalDateTimeChooser::ResponseHandler, WrapPersistent(this));
  GetDateTimeChooser(frame).OpenDateTimeDialog(
      std::move(date_time_dialog_value), std::move(response_callback));
}

void ExternalDateTimeChooser::ResponseHandler(bool success,
                                              double dialog_value) {
  if (success)
    DidChooseValue(dialog_value);
  else
    DidCancelChooser();
  client_ = nullptr;
}

bool ExternalDateTimeChooser::IsShowingDateTimeChooserUI() const {
  return client_ != nullptr;
}

mojom::blink::DateTimeChooser& ExternalDateTimeChooser::GetDateTimeChooser(
    LocalFrame* frame) {
  if (!date_time_chooser_.is_bound()) {
    frame->GetBrowserInterfaceBroker().GetInterface(
        date_time_chooser_.BindNewPipeAndPassReceiver(
            // Per the spec, this is a user interaction.
            // https://html.spec.whatwg.org/multipage/input.html#common-input-element-events
            frame->GetTaskRunner(TaskType::kUserInteraction)));
  }

  DCHECK(date_time_chooser_.is_bound());
  return *date_time_chooser_.get();
}

void ExternalDateTimeChooser::DidChooseValue(double value) {
  // Cache the owner element first, because DidChooseValue might run
  // JavaScript code and destroy |client|.
  Element* element = client_ ? &client_->OwnerElement() : nullptr;
  if (client_)
    client_->DidChooseValue(value);

  // Post an accessibility event on the owner element to indicate the
  // value changed.
  if (element) {
    if (AXObjectCache* cache = element->GetDocument().ExistingAXObjectCache())
      cache->HandleValueChanged(element);
  }

  // DidChooseValue might run JavaScript code, and endChooser() might be
  // called. However DateTimeChooserCompletionImpl still has one reference to
  // this object.
  if (client_)
    client_->DidEndChooser();
}

void ExternalDateTimeChooser::DidCancelChooser() {
  if (client_)
    client_->DidEndChooser();
}

void ExternalDateTimeChooser::EndChooser() {
  DCHECK(client_);
  if (date_time_chooser_.is_bound()) {
    date_time_chooser_->CloseDateTimeDialog();
    date_time_chooser_.reset();
  }
  DateTimeChooserClient* client = client_;
  client_ = nullptr;
  client->DidEndChooser();
}

AXObject* ExternalDateTimeChooser::RootAXObject(Element* popup_owner) {
  return nullptr;
}

}  // namespace blink
