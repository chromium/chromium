// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_file_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_password_credential_data.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kPasswordCredentialType[] = "password";
}

// https://w3c.github.io/webappsec-credential-management/#construct-passwordcredential-data
PasswordCredential* PasswordCredential::Create(
    const PasswordCredentialData* data,
    ExceptionState& exception_state) {
  if (data->id().empty()) {
    exception_state.ThrowTypeError("'id' must not be empty.");
    return nullptr;
  }
  if (data->password().empty()) {
    exception_state.ThrowTypeError("'password' must not be empty.");
    return nullptr;
  }

  KURL icon_url;
  if (data->hasIconURL())
    icon_url = ParseStringAsURLOrThrow(data->iconURL(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  String name;
  if (data->hasName())
    name = data->name();

  return MakeGarbageCollected<PasswordCredential>(data->id(), data->password(),
                                                  name, icon_url);
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
  bool is_id_set = false;
  bool is_password_set = false;
  for (ListedElement* submittable_element : form->ListedElements()) {
    // The "form data set" contains an entry for a |submittable_element| only if
    // it has a non-empty `name` attribute.
    // https://html.spec.whatwg.org/C/#constructing-the-form-data-set
    if (submittable_element->GetName().empty())
      continue;

    V8FormDataEntryValue* value =
        form_data->get(submittable_element->GetName());
    if (!value || !value->IsUSVString())
      continue;
    const String& usv_string_value = value->GetAsUSVString();

    Vector<String> autofill_tokens;
    submittable_element->ToHTMLElement()
        .FastGetAttribute(html_names::kAutocompleteAttr)
        .GetString()
        .LowerASCII()
        .Split(' ', autofill_tokens);
    for (const auto& token : autofill_tokens) {
      if (token == "current-password" || token == "new-password") {
        data->setPassword(usv_string_value);
        is_password_set = true;
      } else if (token == "photo") {
        data->setIconURL(usv_string_value);
      } else if (token == "name" || token == "nickname") {
        data->setName(usv_string_value);
      } else if (token == "username") {
        data->setId(usv_string_value);
        is_id_set = true;
      }
    }
  }

  // Check required fields of PasswordCredentialData dictionary.
  if (!is_id_set) {
    exception_state.ThrowTypeError(
        "'username' must be specified in the form's autocomplete attribute.");
    return nullptr;
  }
  if (!is_password_set) {
    exception_state.ThrowTypeError(
        "Either 'current-password' or 'new-password' must be specified in the "
        "form's autocomplete attribute.");
    return nullptr;
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
  DCHECK(!password.empty());
}

bool PasswordCredential::IsPasswordCredential() const {
  return true;
}

}  // namespace blink
