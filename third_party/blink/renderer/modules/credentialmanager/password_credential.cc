// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kPasswordCredentialType[] = "password";
}

// https://w3c.github.io/webappsec-credential-management/#construct-passwordcredential-data
PasswordCredential* PasswordCredential::Create(
    const PasswordCredentialData* data,
    ExceptionState& exception_state) {
  if (data->id().IsEmpty())
    exception_state.ThrowTypeError("'id' must not be empty.");
  if (data->password().IsEmpty())
    exception_state.ThrowTypeError("'password' must not be empty.");

  KURL icon_url = ParseStringAsURLOrThrow(data->iconURL(), exception_state);

  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<PasswordCredential>(data->id(), data->password(),
                                                  data->name(), icon_url);
}

// https://w3c.github.io/webappsec-credential-management/#construct-passwordcredential-form
PasswordCredential* PasswordCredential::Create(
    HTMLFormElement* form,
    ExceptionState& exception_state) {
  // Extract data from the form, then use the extracted |form_data| object's
  // value to populate |data|.
  FormData* form_data = FormData::Create(form, exception_state);
  if (exception_state.HadException())
    return nullptr;
  PasswordCredentialData* data = PasswordCredentialData::Create();
  for (ListedElement* submittable_element : form->ListedElements()) {
    // The "form data set" contains an entry for a |submittable_element| only if
    // it has a non-empty `name` attribute.
    // https://html.spec.whatwg.org/C/#constructing-the-form-data-set
    DCHECK(!submittable_element->GetName().IsEmpty());

    FileOrUSVString value;
    form_data->get(submittable_element->GetName(), value);
    if (!value.IsUSVString())
      continue;

    Vector<String> autofill_tokens;
    submittable_element->ToHTMLElement()
        .FastGetAttribute(html_names::kAutocompleteAttr)
        .GetString()
        .LowerASCII()
        .Split(' ', autofill_tokens);
    for (const auto& token : autofill_tokens) {
      if (token == "current-password" || token == "new-password") {
        data->setPassword(value.GetAsUSVString());
      } else if (token == "photo") {
        data->setIconURL(value.GetAsUSVString());
      } else if (token == "name" || token == "nickname") {
        data->setName(value.GetAsUSVString());
      } else if (token == "username") {
        data->setId(value.GetAsUSVString());
      }
    }
  }

  // Create a PasswordCredential using the data gathered above.
  return PasswordCredential::Create(data, exception_state);
}

PasswordCredential* PasswordCredential::Create(const String& id,
                                               const String& password,
                                               const String& name,
                                               const KURL& icon_url) {
  return MakeGarbageCollected<PasswordCredential>(
      id, password, name, icon_url.IsEmpty() ? blink::KURL() : icon_url);
}

PasswordCredential::PasswordCredential(const String& id,
                                       const String& password,
                                       const String& name,
                                       const KURL& icon_url)
    : Credential(id, kPasswordCredentialType),
      password_(password),
      name_(name),
      icon_url_(icon_url) {
  DCHECK(!password.IsEmpty());
}

bool PasswordCredential::IsPasswordCredential() const {
  return true;
}

}  // namespace blink
