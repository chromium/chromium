// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_util.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/per_context_data.h"

namespace extensions {
namespace binding {

namespace {

bool g_response_validation_enabled =
#if DCHECK_IS_ON()
    true;
#else
    false;
#endif

}  // namespace

class ContextInvalidationData : public base::SupportsUserData::Data {
 public:
  ContextInvalidationData();

  ContextInvalidationData(const ContextInvalidationData&) = delete;
  ContextInvalidationData& operator=(const ContextInvalidationData&) = delete;

  ~ContextInvalidationData() override;

  static constexpr char kPerContextDataKey[] = "extension_context_invalidation";

  void Invalidate();

  void AddListener(ContextInvalidationListener* listener);
  void RemoveListener(ContextInvalidationListener* listener);

  bool is_context_valid() const { return is_context_valid_; }

 private:
  bool is_context_valid_ = true;
  base::ObserverList<ContextInvalidationListener>::Unchecked
      invalidation_listeners_;
};

constexpr char ContextInvalidationData::kPerContextDataKey[];

ContextInvalidationData::ContextInvalidationData() = default;
ContextInvalidationData::~ContextInvalidationData() {
  if (is_context_valid_)
    Invalidate();
}

void ContextInvalidationData::AddListener(
    ContextInvalidationListener* listener) {
  DCHECK(is_context_valid_);
  invalidation_listeners_.AddObserver(listener);
}

void ContextInvalidationData::RemoveListener(
    ContextInvalidationListener* listener) {
  DCHECK(is_context_valid_);
  DCHECK(invalidation_listeners_.HasObserver(listener));
  invalidation_listeners_.RemoveObserver(listener);
}

void ContextInvalidationData::Invalidate() {
  DCHECK(is_context_valid_);
  is_context_valid_ = false;

  for (ContextInvalidationListener& listener : invalidation_listeners_)
    listener.OnInvalidated();
}

bool IsContextValid(v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return false;

  auto* invalidation_data =
      static_cast<ContextInvalidationData*>(per_context_data->GetUserData(
          ContextInvalidationData::kPerContextDataKey));
  // The context is valid if we've never created invalidation data for it, or if
  // we have and it hasn't been marked as invalid.
  bool is_context_valid =
      !invalidation_data || invalidation_data->is_context_valid();

  if (is_context_valid) {
    // As long as the context is valid, there should be an associated
    // JSRunner.
    // TODO(devlin): (Likely) Remove this once https://crbug.com/819968, since
    // this shouldn't necessarily be a hard dependency. At least downgrade it
    // to a DCHECK.
    CHECK(JSRunner::Get(context));
  }
  return is_context_valid;
}

bool IsContextValidOrThrowError(v8::Local<v8::Context> context) {
  if (IsContextValid(context))
    return true;
  v8::Isolate* isolate = context->GetIsolate();
  isolate->ThrowException(v8::Exception::Error(
      gin::StringToV8(isolate, "Extension context invalidated.")));
  return false;
}

void InvalidateContext(v8::Local<v8::Context> context) {
  ContextInvalidationData* data =
      GetPerContextData<ContextInvalidationData>(context, kCreateIfMissing);
  if (!data)
    return;

  data->Invalidate();
}

std::string GetPlatformString() {
// TODO(crbug.com/40118868): For readability, this should become
// BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_LACROS). The second
// conditional should be BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_ASH).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return "lacros";
#elif BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  return "chromeos";
#elif BUILDFLAG(IS_LINUX)
  return "linux";
#elif BUILDFLAG(IS_MAC)
  return "mac";
#elif BUILDFLAG(IS_WIN)
  return "win";
#elif BUILDFLAG(IS_FUCHSIA)
  return "fuchsia";
#elif BUILDFLAG(IS_DESKTOP_ANDROID)
  return "desktop_android";
#else
  NOTREACHED_IN_MIGRATION();
  return std::string();
#endif
}

ContextInvalidationListener::ContextInvalidationListener(
    v8::Local<v8::Context> context,
    base::OnceClosure on_invalidated)
    : on_invalidated_(std::move(on_invalidated)),
      context_invalidation_data_(
          GetPerContextData<ContextInvalidationData>(context,
                                                     kCreateIfMissing)) {
  // We should never add an invalidation observer to an invalid context.
  DCHECK(context_invalidation_data_);
  DCHECK(context_invalidation_data_->is_context_valid());
  context_invalidation_data_->AddListener(this);
}

ContextInvalidationListener::~ContextInvalidationListener() {
  if (!on_invalidated_)
    return;  // Context was invalidated.

  DCHECK(context_invalidation_data_);
  context_invalidation_data_->RemoveListener(this);
}

void ContextInvalidationListener::OnInvalidated() {
  DCHECK(on_invalidated_);
  context_invalidation_data_ = nullptr;
  std::move(on_invalidated_).Run();
}

bool IsResponseValidationEnabled() {
  return g_response_validation_enabled;
}

std::unique_ptr<base::AutoReset<bool>> SetResponseValidationEnabledForTesting(
    bool is_enabled) {
  return std::make_unique<base::AutoReset<bool>>(&g_response_validation_enabled,
                                                 is_enabled);
}

}  // namespace binding
}  // namespace extensions
