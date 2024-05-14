// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_response_validator.h"

#include <ostream>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"

namespace extensions {

namespace {

APIResponseValidator::TestHandler* g_handler_for_testing = nullptr;

}  // namespace

APIResponseValidator::TestHandler::TestHandler(HandlerMethod method)
    : method_(method) {
  DCHECK(!g_handler_for_testing)
      << "Only one TestHandler is allowed at a time.";
  g_handler_for_testing = this;
}

APIResponseValidator::TestHandler::~TestHandler() {
  DCHECK_EQ(this, g_handler_for_testing);
  g_handler_for_testing = nullptr;
}

void APIResponseValidator::TestHandler::IgnoreSignature(std::string signature) {
  signatures_to_ignore_.insert(std::move(signature));
}

void APIResponseValidator::TestHandler::HandleFailure(
    const std::string& signature_name,
    const std::string& error) {
  method_.Run(signature_name, error);
}

bool APIResponseValidator::TestHandler::ShouldIgnoreSignature(
    const std::string& signature_name) const {
  return base::Contains(signatures_to_ignore_, signature_name);
}

APIResponseValidator::APIResponseValidator(const APITypeReferenceMap* type_refs)
    : type_refs_(type_refs) {}

APIResponseValidator::~APIResponseValidator() = default;

void APIResponseValidator::ValidateResponse(
    v8::Local<v8::Context> context,
    const std::string& method_name,
    const v8::LocalVector<v8::Value>& response_arguments,
    const std::string& api_error,
    CallbackType callback_type) {
  DCHECK(binding::IsResponseValidationEnabled());

  // If the callback is API-provided, the response can't be validated against
  // the expected schema because the callback may modify the arguments.
  if (callback_type == CallbackType::kAPIProvided)
    return;

  // If the call failed, there are no expected arguments.
  if (!api_error.empty()) {
    // TODO(devlin): It would be really nice to validate that
    // |response_arguments| is empty here, but some functions both set an error
    // and supply arguments.
    return;
  }

  const APISignature* signature =
      type_refs_->GetAsyncResponseSignature(method_name);
  // If there's no corresponding signature, don't validate. This can
  // legitimately happen with APIs that create custom requests.
  if (!signature || !signature->has_async_return_signature())
    return;

  std::string error;
  if (signature->ValidateResponse(context, response_arguments, *type_refs_,
                                  &error)) {
    // Response was valid.
    return;
  }

  // The response did not match the expected schema.
  if (g_handler_for_testing) {
    g_handler_for_testing->HandleFailure(method_name, error);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Error validating response to `" << method_name << "`: " << error;
  }
}

void APIResponseValidator::ValidateEvent(
    v8::Local<v8::Context> context,
    const std::string& event_name,
    const v8::LocalVector<v8::Value>& event_args) {
  DCHECK(binding::IsResponseValidationEnabled());

  const APISignature* signature = type_refs_->GetEventSignature(event_name);
  // If there's no corresponding signature, don't validate. This can
  // legitimately happen with APIs that create custom requests.
  if (!signature)
    return;

  if (g_handler_for_testing &&
      g_handler_for_testing->ShouldIgnoreSignature(event_name))
    return;

  // The following signatures are incorrect (the parameters dispatched to the
  // event don't match the schema's event definition). These should be fixed
  // and then validated.
  // TODO(crbug.com/40226845): Eliminate this list.
  static constexpr char const* kBrokenSignaturesToIgnore[] = {
      "automationInternal.onAccessibilityEvent",
      "chromeWebViewInternal.onClicked",
      "input.ime.onFocus",
      "inputMethodPrivate.onFocus",
      "test.onMessage",

      // https://crbug.com/1343611.
      "runtime.onMessage",
      "runtime.onConnect",
      "contextMenus.onClicked",

      // https://crbug.com/1375903.
      "downloads.onCreated",
  };

  if (base::ranges::find(kBrokenSignaturesToIgnore, event_name) !=
      std::end(kBrokenSignaturesToIgnore))
    return;

  std::string error;
  if (signature->ValidateCall(context, event_args, *type_refs_, &error)) {
    // Response was valid.
    return;
  }

  // The response did not match the expected schema.
  // Pull to helper method.
  if (g_handler_for_testing) {
    g_handler_for_testing->HandleFailure(event_name, error);
  } else {
    NOTREACHED_IN_MIGRATION() << "Error validating event arguments to `"
                              << event_name << "`: " << error;
  }
}

}  // namespace extensions
