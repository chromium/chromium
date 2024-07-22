// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contacts_manager.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_contact_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_contact_property.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/contacts_picker/contact_address.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace mojo {

template <>
struct TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr> {
  static blink::ContactInfo* Convert(
      const blink::mojom::blink::ContactInfoPtr& contact);
};

blink::ContactInfo*
TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr>::
    Convert(const blink::mojom::blink::ContactInfoPtr& contact) {
  blink::ContactInfo* contact_info = blink::ContactInfo::Create();

  if (contact->name) {
    Vector<String> names;
    names.ReserveInitialCapacity(contact->name->size());

    for (const String& name : *contact->name)
      names.push_back(name);

    contact_info->setName(names);
  }

  if (contact->email) {
    Vector<String> emails;
    emails.ReserveInitialCapacity(contact->email->size());

    for (const String& email : *contact->email)
      emails.push_back(email);

    contact_info->setEmail(emails);
  }

  if (contact->tel) {
    Vector<String> numbers;
    numbers.ReserveInitialCapacity(contact->tel->size());

    for (const String& number : *contact->tel)
      numbers.push_back(number);

    contact_info->setTel(numbers);
  }

  if (contact->address) {
    blink::HeapVector<blink::Member<blink::ContactAddress>> addresses;
    for (auto& address : *contact->address) {
      auto* blink_address = blink::MakeGarbageCollected<blink::ContactAddress>(
          std::move(address));
      addresses.push_back(blink_address);
    }

    contact_info->setAddress(addresses);
  }

  if (contact->icon) {
    blink::HeapVector<blink::Member<blink::Blob>> icons;
    for (blink::mojom::blink::ContactIconBlobPtr& icon : *contact->icon) {
      icons.push_back(blink::Blob::Create(icon->data, icon->mime_type));
    }

    contact_info->setIcon(icons);
  }

  return contact_info;
}

}  // namespace mojo

namespace blink {

// static
const char ContactsManager::kSupplementName[] = "ContactsManager";

// static
ContactsManager* ContactsManager::contacts(Navigator& navigator) {
  auto* supplement = Supplement<Navigator>::From<ContactsManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<ContactsManager>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

ContactsManager::ContactsManager(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      contacts_manager_(navigator.DomWindow()) {}

ContactsManager::~ContactsManager() = default;

mojom::blink::ContactsManager* ContactsManager::GetContactsManager(
    ScriptState* script_state) {
  if (!contacts_manager_.is_bound()) {
    ExecutionContext::From(script_state)
        ->GetBrowserInterfaceBroker()
        .GetInterface(contacts_manager_.BindNewPipeAndPassReceiver(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return contacts_manager_.get();
}

const Vector<V8ContactProperty>& ContactsManager::GetProperties(
    ScriptState* script_state) {
  if (properties_.empty()) {
    properties_ = {V8ContactProperty(V8ContactProperty::Enum::kEmail),
                   V8ContactProperty(V8ContactProperty::Enum::kName),
                   V8ContactProperty(V8ContactProperty::Enum::kTel)};

    if (RuntimeEnabledFeatures::ContactsManagerExtraPropertiesEnabled(
            ExecutionContext::From(script_state))) {
      properties_.push_back(
          V8ContactProperty(V8ContactProperty::Enum::kAddress));
      properties_.push_back(V8ContactProperty(V8ContactProperty::Enum::kIcon));
    }
  }
  return properties_;
}

ScriptPromise<IDLSequence<ContactInfo>> ContactsManager::select(
    ScriptState* script_state,
    const Vector<V8ContactProperty>& properties,
    ContactsSelectOptions* options,
    ExceptionState& exception_state) {
  LocalFrame* frame = script_state->ContextIsValid()
                          ? LocalDOMWindow::From(script_state)->GetFrame()
                          : nullptr;

  if (!frame || !frame->IsOutermostMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The contacts API can only be used in the top frame");
    return ScriptPromise<IDLSequence<ContactInfo>>();
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(
        "A user gesture is required to call this method");
    return ScriptPromise<IDLSequence<ContactInfo>>();
  }

  if (properties.empty()) {
    exception_state.ThrowTypeError("At least one property must be provided");
    return ScriptPromise<IDLSequence<ContactInfo>>();
  }

  if (contact_picker_in_use_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Contacts Picker is already in use.");
    return ScriptPromise<IDLSequence<ContactInfo>>();
  }

  bool include_names = false;
  bool include_emails = false;
  bool include_tel = false;
  bool include_addresses = false;
  bool include_icons = false;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  for (const auto& property : properties) {
    if (!RuntimeEnabledFeatures::ContactsManagerExtraPropertiesEnabled(
            execution_context) &&
        (property == V8ContactProperty::Enum::kAddress ||
         property == V8ContactProperty::Enum::kIcon)) {
      exception_state.ThrowTypeError(
          "The provided value '" + property.AsString() +
          "' is not a valid enum value of type ContactProperty");
      return ScriptPromise<IDLSequence<ContactInfo>>();
    }

    switch (property.AsEnum()) {
      case V8ContactProperty::Enum::kName:
        include_names = true;
        break;
      case V8ContactProperty::Enum::kEmail:
        include_emails = true;
        break;
      case V8ContactProperty::Enum::kTel:
        include_tel = true;
        break;
      case V8ContactProperty::Enum::kAddress:
        include_addresses = true;
        break;
      case V8ContactProperty::Enum::kIcon:
        include_icons = true;
        break;
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<ContactInfo>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  contact_picker_in_use_ = true;
  GetContactsManager(script_state)
      ->Select(options->multiple(), include_names, include_emails, include_tel,
               include_addresses, include_icons,
               WTF::BindOnce(&ContactsManager::OnContactsSelected,
                             WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void ContactsManager::OnContactsSelected(
    ScriptPromiseResolver<IDLSequence<ContactInfo>>* resolver,
    std::optional<Vector<mojom::blink::ContactInfoPtr>> contacts) {
  ScriptState* script_state = resolver->GetScriptState();

  if (!script_state->ContextIsValid()) {
    // This can happen if the page is programmatically redirected while
    // contacts are still being chosen.
    return;
  }

  ScriptState::Scope scope(script_state);

  contact_picker_in_use_ = false;

  if (!contacts.has_value()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Unable to open a contact selector"));
    return;
  }

  HeapVector<Member<ContactInfo>> contacts_list;
  for (const auto& contact : *contacts)
    contacts_list.push_back(contact.To<blink::ContactInfo*>());

  resolver->Resolve(contacts_list);
}

ScriptPromise<IDLSequence<V8ContactProperty>> ContactsManager::getProperties(
    ScriptState* script_state) {
  return ToResolvedPromise<IDLSequence<V8ContactProperty>>(
      script_state, GetProperties(script_state));
}

void ContactsManager::Trace(Visitor* visitor) const {
  visitor->Trace(contacts_manager_);
  Supplement<Navigator>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
