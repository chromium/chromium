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

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/base/ime/mojom/ime_types.mojom-blink.h"

namespace blink {

static ui::mojom::TextInputType ToTextInputType(const AtomicString& source) {
  if (source == input_type_names::kDate)
    return ui::mojom::TextInputType::DATE;
  if (source == input_type_names::kDatetime)
    return ui::mojom::TextInputType::TIME;
  if (source == input_type_names::kDatetimeLocal)
    return ui::mojom::TextInputType::DATE_TIME_LOCAL;
  if (source == input_type_names::kMonth)
    return ui::mojom::TextInputType::MONTH;
  if (source == input_type_names::kTime)
    return ui::mojom::TextInputType::TIME;
  if (source == input_type_names::kWeek)
    return ui::mojom::TextInputType::WEEK;
  return ui::mojom::TextInputType::NONE;
}

ExternalDateTimeChooser::~ExternalDateTimeChooser() = default;

void ExternalDateTimeChooser::Trace(Visitor* visitor) {
  visitor->Trace(client_);
  DateTimeChooser::Trace(visitor);
}

ExternalDateTimeChooser::ExternalDateTimeChooser(DateTimeChooserClient* client)
    : client_(client) {
  DCHECK(!RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  DCHECK(client);
}

ExternalDateTimeChooser* ExternalDateTimeChooser::Create(
    DateTimeChooserClient* client) {
  ExternalDateTimeChooser* chooser =
      MakeGarbageCollected<ExternalDateTimeChooser>(client);
  return chooser;
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

  auto response_callback = WTF::Bind(&ExternalDateTimeChooser::ResponseHandler,
                                     WrapPersistent(this));
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
  return client_;
}

mojom::blink::DateTimeChooser& ExternalDateTimeChooser::GetDateTimeChooser(
    LocalFrame* frame) {
  if (!date_time_chooser_) {
    frame->GetBrowserInterfaceBroker().GetInterface(
        date_time_chooser_.BindNewPipeAndPassReceiver());
  }

  DCHECK(date_time_chooser_);
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
  DateTimeChooserClient* client = client_;
  client_ = nullptr;
  client->DidEndChooser();
}

AXObject* ExternalDateTimeChooser::RootAXObject() {
  return nullptr;
}

}  // namespace blink
