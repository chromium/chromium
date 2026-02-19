// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_util.h"

#include "base/auto_reset.h"
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
  base::ObserverList<ContextInvalidationListener> invalidation_listeners_;
};

constexpr char ContextInvalidationData::kPerContextDataKey[];

ContextInvalidationData::ContextInvalidationData() = default;
ContextInvalidationData::~ContextInvalidationData() {
  if (is_context_valid_)
    Invalidate();
}

void ContextInvalidationData::AddListener(
    ContextInvalidationListener* listener) {
  CHECK(is_context_valid_);
  invalidation_listeners_.AddObserver(listener);
}

void ContextInvalidationData::RemoveListener(
    ContextInvalidationListener* listener) {
  CHECK(invalidation_listeners_.HasObserver(listener));
  invalidation_listeners_.RemoveObserver(listener);
}

void ContextInvalidationData::Invalidate() {
  CHECK(is_context_valid_);
  is_context_valid_ = false;

  for (ContextInvalidationListener& listener : invalidation_listeners_)
    listener.OnInvalidated();
}

bool IsContextValid(v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return false;

  auto* invalidation_data = GetPerContextData<ContextInvalidationData>(
      context, CreatePerContextData::kDontCreateIfMissing);

  // The context is valid as long as the invalidation data is present and not
  // marked invalid.
  bool is_context_valid =
      invalidation_data && invalidation_data->is_context_valid();

  if (is_context_valid) {
    // As long as the context is valid, there should be an associated
    // JSRunner.
    // TODO(devlin): (Likely) Remove this once https://crbug.com/41375376, since
    // this shouldn't necessarily be a hard dependency. At least downgrade it
    // to a DCHECK.
    CHECK(JSRunner::Get(context));
  }
  return is_context_valid;
}

bool IsContextValidOrThrowError(v8::Local<v8::Context> context) {
  if (IsContextValid(context))
    return true;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  isolate->ThrowException(v8::Exception::Error(
      gin::StringToV8(isolate, "Extension context invalidated.")));
  return false;
}

void InitializeContext(v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  CHECK(per_context_data);

  // It would be nice to CHECK() that the invalidation data does *not* yet exist
  // (since this means we're calling InitializeContext() twice), but a number of
  // tests do this as part of their setup. It's also fairly harmless.
  if (per_context_data->GetUserData(
          ContextInvalidationData::kPerContextDataKey)) {
    return;
  }

  per_context_data->SetUserData(ContextInvalidationData::kPerContextDataKey,
                                std::make_unique<ContextInvalidationData>());
}

void InvalidateContext(v8::Local<v8::Context> context) {
  ContextInvalidationData* data = GetPerContextData<ContextInvalidationData>(
      context, CreatePerContextData::kDontCreateIfMissing);
  CHECK(data);

  data->Invalidate();
}

std::string_view GetPlatformString() {
#if BUILDFLAG(IS_CHROMEOS)
  return "chromeos";
#elif BUILDFLAG(IS_LINUX)
  return "linux";
#elif BUILDFLAG(IS_MAC)
  return "mac";
#elif BUILDFLAG(IS_WIN)
  return "win";
#elif BUILDFLAG(IS_DESKTOP_ANDROID)
  return "desktop_android";
#else
  NOTREACHED();
#endif
}

ContextInvalidationListener::ContextInvalidationListener(
    v8::Local<v8::Context> context,
    base::OnceClosure on_invalidated)
    : on_invalidated_(std::move(on_invalidated)),
      context_invalidation_data_(GetPerContextData<ContextInvalidationData>(
          context,
          CreatePerContextData::kCreateIfMissing)) {
  // We should never add an invalidation observer to an invalid context.
  DCHECK(context_invalidation_data_);
  DCHECK(context_invalidation_data_->is_context_valid());
  context_invalidation_data_->AddListener(this);
}

ContextInvalidationListener::~ContextInvalidationListener() {
  Dispose();
}

void ContextInvalidationListener::Dispose() {
  // We may have already removed ourselves as a listener (in OnInvalidated())
  // if the context was invalidated previously. Check the context first.
  if (context_invalidation_data_) {
    context_invalidation_data_->RemoveListener(this);
    context_invalidation_data_ = nullptr;
  }
}

void ContextInvalidationListener::OnInvalidated() {
  DCHECK(on_invalidated_) << "OnInvalidated() called twice!";
  DCHECK(context_invalidation_data_);

  // The ContextInvalidationData will be cleaned up soon, so we can't store a
  // reference to it. We also remove ourselves as an observer proactively to
  // avoid leaving a dangling pointer in ContextInvalidationData.
  context_invalidation_data_->RemoveListener(this);
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
