// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_interaction_provider.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/content_features.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

namespace {
struct ExtensionInteractionData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] = "extension_interaction";
  int interaction_count = 0;
  int token_interaction_count = 0;
};
constexpr char ExtensionInteractionData::kPerContextDataKey[];

}  // namespace

// ExtensionInteractionProvider::Token -----------------------------------------
ExtensionInteractionProvider::Token::Token(bool for_worker)
    : is_for_service_worker_(for_worker) {}
ExtensionInteractionProvider::Token::~Token() = default;

// ExtensionInteractionProvider::Scope -----------------------------------------

ExtensionInteractionProvider::Scope::Scope() = default;
ExtensionInteractionProvider::Scope::~Scope() = default;

// static.
std::unique_ptr<ExtensionInteractionProvider::Scope>
ExtensionInteractionProvider::Scope::ForWorker(
    v8::Local<v8::Context> v8_context) {
  DCHECK(worker_thread_util::IsWorkerThread());
  auto scope = base::WrapUnique(new Scope());
  scope->worker_thread_interaction_ =
      std::make_unique<ScopedWorkerInteraction>(v8_context, false);
  return scope;
}

// static.
std::unique_ptr<ExtensionInteractionProvider::Scope>
ExtensionInteractionProvider::Scope::ForToken(
    v8::Local<v8::Context> v8_context,
    std::unique_ptr<InteractionProvider::Token> token) {
  Token* token_impl = static_cast<Token*>(token.get());
  if (!token_impl->is_for_service_worker()) {
    // UserActivationV2 replaces the concept of (scoped) tokens with a
    // frame-wide state, hence skips token forwarding.
    return nullptr;
  }

  auto scope = base::WrapUnique(new Scope());
  scope->worker_thread_interaction_ =
      std::make_unique<ScopedWorkerInteraction>(v8_context, true);
  return scope;
}

ExtensionInteractionProvider::Scope::ScopedWorkerInteraction::
    ScopedWorkerInteraction(v8::Local<v8::Context> v8_context,
                            bool created_from_token)
    : v8_context_(v8_context), created_from_token_(created_from_token) {
  ExtensionInteractionData* per_context_data =
      GetPerContextData<ExtensionInteractionData>(v8_context, kCreateIfMissing);
  DCHECK(per_context_data);
  if (created_from_token_)
    per_context_data->token_interaction_count++;
  else
    per_context_data->interaction_count++;
}
ExtensionInteractionProvider::Scope::ScopedWorkerInteraction::
    ~ScopedWorkerInteraction() {
  ExtensionInteractionData* per_context_data =
      GetPerContextData<ExtensionInteractionData>(v8_context_,
                                                  kDontCreateIfMissing);
  // If |v8_context_| was invalidated (e.g. because of JS running), bail out.
  if (!per_context_data)
    return;

  if (created_from_token_) {
    DCHECK_GT(per_context_data->token_interaction_count, 0);
    per_context_data->token_interaction_count--;
  } else {
    DCHECK_GT(per_context_data->interaction_count, 0);
    per_context_data->interaction_count--;
  }
}

// ExtensionInteractionProvider ------------------------------------------------

ExtensionInteractionProvider::ExtensionInteractionProvider() = default;

ExtensionInteractionProvider::~ExtensionInteractionProvider() = default;

// static
bool ExtensionInteractionProvider::HasActiveExtensionInteraction(
    v8::Local<v8::Context> v8_context) {
  // Service Worker based context:
  if (worker_thread_util::IsWorkerThread()) {
    ExtensionInteractionData* per_context_data =
        GetPerContextData<ExtensionInteractionData>(v8_context,
                                                    kDontCreateIfMissing);
    if (per_context_data && (per_context_data->interaction_count > 0 ||
                             per_context_data->token_interaction_count > 0)) {
      return true;
    }
    return worker_thread_util::HasWorkerContextProxyInteraction();
  }

  // RenderFrame based context:
  ScriptContext* script_context =
      GetScriptContextFromV8ContextChecked(v8_context);
  if (!script_context->web_frame())
    return false;
  return script_context->web_frame()->HasTransientUserActivation();
}

std::unique_ptr<InteractionProvider::Token>
ExtensionInteractionProvider::GetCurrentToken(
    v8::Local<v8::Context> v8_context) const {
  if (worker_thread_util::IsWorkerThread()) {
    ExtensionInteractionData* per_context_data =
        GetPerContextData<ExtensionInteractionData>(v8_context,
                                                    kDontCreateIfMissing);
    const bool has_extension_api_interaction =
        per_context_data && per_context_data->interaction_count > 0;
    // Only create token for Service Workers when we have an interaction taking
    // place that wasn't created through another token (i.e. do not look at
    // worker_data->token_interaction_count).
    if (!has_extension_api_interaction &&
        !worker_thread_util::HasWorkerContextProxyInteraction()) {
      return nullptr;
    }
    return base::WrapUnique(new Token(true));
  }

  // Render frame based token.
  return base::WrapUnique(new Token(false));
}

std::unique_ptr<InteractionProvider::Scope>
ExtensionInteractionProvider::CreateScopedInteraction(
    v8::Local<v8::Context> v8_context,
    std::unique_ptr<InteractionProvider::Token> token) const {
  return Scope::ForToken(v8_context, std::move(token));
}

bool ExtensionInteractionProvider::HasActiveInteraction(
    v8::Local<v8::Context> v8_context) const {
  return ExtensionInteractionProvider::HasActiveExtensionInteraction(
      v8_context);
}

}  // namespace extensions
