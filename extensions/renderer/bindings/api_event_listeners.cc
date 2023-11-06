// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_event_listeners.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/event_matcher.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/listener_tracker.h"
#include "gin/converter.h"

namespace extensions {

namespace {

// TODO(devlin): The EventFilter supports adding EventMatchers associated with
// an id. For now, we ignore it and add/return everything associated with this
// constant. We should rethink that.
const int kIgnoreRoutingId = 0;

const char kErrorTooManyListeners[] = "Too many listeners.";

// Pseudo-validates the given |filter| and converts it into a
// base::Value::Dict. Returns true on success.
// TODO(devlin): This "validation" is pretty terrible. It matches the JS
// equivalent, but it's lousy and makes it easy for users to get it wrong.
// We should generate an argument spec for it and match it exactly.
bool ValidateFilter(v8::Local<v8::Context> context,
                    v8::Local<v8::Object> filter,
                    std::unique_ptr<base::Value::Dict>* filter_dict,
                    std::string* error) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  if (filter.IsEmpty()) {
    *filter_dict = std::make_unique<base::Value::Dict>();
    return true;
  }

  v8::Local<v8::Value> url_filter;
  if (!filter->Get(context, gin::StringToSymbol(isolate, "url"))
           .ToLocal(&url_filter)) {
    return false;
  }

  if (!url_filter->IsUndefined() && !url_filter->IsArray()) {
    *error = "filters.url should be an array.";
    return false;
  }

  v8::Local<v8::Value> service_type;
  if (!filter->Get(context, gin::StringToSymbol(isolate, "serviceType"))
           .ToLocal(&service_type)) {
    return false;
  }

  if (!service_type->IsUndefined() && !service_type->IsString()) {
    *error = "filters.serviceType should be a string.";
    return false;
  }

  std::unique_ptr<base::Value> value =
      content::V8ValueConverter::Create()->FromV8Value(filter, context);
  if (!value || !value->is_dict()) {
    *error = "could not convert filter.";
    return false;
  }

  *filter_dict = std::make_unique<base::Value::Dict>(value->GetDict().Clone());
  return true;
}

}  // namespace

UnfilteredEventListeners::UnfilteredEventListeners(
    ListenersUpdated listeners_updated,
    const std::string& event_name,
    ContextOwnerIdGetter context_owner_id_getter,
    int max_listeners,
    bool supports_lazy_listeners,
    ListenerTracker* listener_tracker)
    : listeners_updated_(std::move(listeners_updated)),
      event_name_(event_name),
      context_owner_id_getter_(std::move(context_owner_id_getter)),
      max_listeners_(max_listeners),
      supports_lazy_listeners_(supports_lazy_listeners),
      listener_tracker_(listener_tracker) {
  DCHECK(max_listeners_ == binding::kNoListenerMax || max_listeners_ > 0);
  DCHECK_EQ(listener_tracker_ == nullptr, context_owner_id_getter_.is_null())
      << "Managed events must have both a listener tracker and context owner; "
      << "unmanaged must have neither.";
}
UnfilteredEventListeners::~UnfilteredEventListeners() = default;

bool UnfilteredEventListeners::AddListener(v8::Local<v8::Function> listener,
                                           v8::Local<v8::Object> filter,
                                           v8::Local<v8::Context> context,
                                           std::string* error) {
  // |filter| should be checked before getting here.
  DCHECK(filter.IsEmpty())
      << "Filtered events should use FilteredEventListeners";

  if (HasListener(listener))
    return false;

  if (max_listeners_ != binding::kNoListenerMax &&
      listeners_.size() >= static_cast<size_t>(max_listeners_)) {
    *error = kErrorTooManyListeners;
    return false;
  }

  listeners_.push_back(
      v8::Global<v8::Function>(context->GetIsolate(), listener));
  if (listeners_.size() == 1) {
    // NOTE: |listener_tracker_| is null for unmanaged events, in which case we
    // send no notifications.
    if (listener_tracker_) {
      LazilySetContextOwner(context);
      bool was_first_listener_for_context_owner =
          listener_tracker_->AddUnfilteredListener(context_owner_id_,
                                                   event_name_);
      binding::EventListenersChanged changed =
          was_first_listener_for_context_owner
              ? binding::EventListenersChanged::
                    kFirstUnfilteredListenerForContextOwnerAdded
              : binding::EventListenersChanged::
                    kFirstUnfilteredListenerForContextAdded;
      listeners_updated_.Run(event_name_, changed, nullptr,
                             supports_lazy_listeners_, context);
    }
  }

  return true;
}

void UnfilteredEventListeners::RemoveListener(v8::Local<v8::Function> listener,
                                              v8::Local<v8::Context> context) {
  auto iter = base::ranges::find(listeners_, listener);
  if (iter == listeners_.end())
    return;

  listeners_.erase(iter);
  if (listeners_.empty()) {
    bool update_lazy_listeners = supports_lazy_listeners_;
    NotifyListenersEmpty(context, update_lazy_listeners);
  }
}

bool UnfilteredEventListeners::HasListener(v8::Local<v8::Function> listener) {
  return base::Contains(listeners_, listener);
}

size_t UnfilteredEventListeners::GetNumListeners() {
  return listeners_.size();
}

v8::LocalVector<v8::Function> UnfilteredEventListeners::GetListeners(
    mojom::EventFilteringInfoPtr filter,
    v8::Local<v8::Context> context) {
  v8::LocalVector<v8::Function> listeners(context->GetIsolate());
  listeners.reserve(listeners_.size());
  for (const auto& listener : listeners_)
    listeners.push_back(listener.Get(context->GetIsolate()));
  return listeners;
}

void UnfilteredEventListeners::Invalidate(v8::Local<v8::Context> context) {
  if (!listeners_.empty()) {
    listeners_.clear();
    // We don't want to update stored lazy listeners in this case, since the
    // extension didn't explicitly unregister interest in the event.
    constexpr bool update_lazy_listeners = false;
    NotifyListenersEmpty(context, update_lazy_listeners);
  }
}

void UnfilteredEventListeners::LazilySetContextOwner(
    v8::Local<v8::Context> context) {
  if (context_owner_id_.empty()) {
    DCHECK(context_owner_id_getter_);
    context_owner_id_ = context_owner_id_getter_.Run(context);
    DCHECK(!context_owner_id_.empty());
  }
}

void UnfilteredEventListeners::NotifyListenersEmpty(
    v8::Local<v8::Context> context,
    bool update_lazy_listeners) {
  DCHECK(listeners_.empty());
  // NOTE: |listener_tracker_| is null for unmanaged events, in which case we
  // send no notifications.
  if (!listener_tracker_)
    return;

  DCHECK(!context_owner_id_.empty())
      << "The context owner must be instantiated if listeners were removed.";

  bool was_last_listener_for_context_owner =
      listener_tracker_->RemoveUnfilteredListener(context_owner_id_,
                                                  event_name_);
  binding::EventListenersChanged changed =
      was_last_listener_for_context_owner
          ? binding::EventListenersChanged::
                kLastUnfilteredListenerForContextOwnerRemoved
          : binding::EventListenersChanged::
                kLastUnfilteredListenerForContextRemoved;
  listeners_updated_.Run(event_name_, changed, nullptr, update_lazy_listeners,
                         context);
}

struct FilteredEventListeners::ListenerData {
  bool operator==(v8::Local<v8::Function> other_function) const {
    // Note that we only consider the listener function here, and not the
    // filter. This implies that it's invalid to try and add the same
    // function for multiple filters.
    // TODO(devlin): It's always been this way, but should it be?
    return function == other_function;
  }

  v8::Global<v8::Function> function;
  int filter_id;
};

FilteredEventListeners::FilteredEventListeners(
    ListenersUpdated listeners_updated,
    const std::string& event_name,
    ContextOwnerIdGetter context_owner_id_getter,
    int max_listeners,
    bool supports_lazy_listeners,
    ListenerTracker* listener_tracker)
    : listeners_updated_(std::move(listeners_updated)),
      event_name_(event_name),
      context_owner_id_getter_(std::move(context_owner_id_getter)),
      max_listeners_(max_listeners),
      supports_lazy_listeners_(supports_lazy_listeners),
      listener_tracker_(listener_tracker) {
  DCHECK(listener_tracker_);
  DCHECK(context_owner_id_getter_);
}

FilteredEventListeners::~FilteredEventListeners() = default;

bool FilteredEventListeners::AddListener(v8::Local<v8::Function> listener,
                                         v8::Local<v8::Object> filter,
                                         v8::Local<v8::Context> context,
                                         std::string* error) {
  if (HasListener(listener))
    return false;

  if (max_listeners_ != binding::kNoListenerMax &&
      listeners_.size() >= static_cast<size_t>(max_listeners_)) {
    *error = kErrorTooManyListeners;
    return false;
  }

  std::unique_ptr<base::Value::Dict> filter_dict;
  if (!ValidateFilter(context, filter, &filter_dict, error))
    return false;

  base::Value::Dict* filter_weak = filter_dict.get();
  int filter_id = -1;
  bool was_first_of_kind = false;
  LazilySetContextOwner(context);
  std::tie(was_first_of_kind, filter_id) =
      listener_tracker_->AddFilteredListener(context_owner_id_, event_name_,
                                             std::move(filter_dict),
                                             kIgnoreRoutingId);

  if (filter_id == -1) {
    *error = "Could not add listener";
    return false;
  }

  listeners_.push_back(
      {v8::Global<v8::Function>(context->GetIsolate(), listener), filter_id});
  if (was_first_of_kind) {
    listeners_updated_.Run(event_name_,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           filter_weak, supports_lazy_listeners_, context);
  }

  return true;
}

void FilteredEventListeners::RemoveListener(v8::Local<v8::Function> listener,
                                            v8::Local<v8::Context> context) {
  auto iter = base::ranges::find(listeners_, listener);
  if (iter == listeners_.end())
    return;

  ListenerData data = std::move(*iter);
  listeners_.erase(iter);

  InvalidateListener(data, true, context);
}

bool FilteredEventListeners::HasListener(v8::Local<v8::Function> listener) {
  return base::Contains(listeners_, listener);
}

size_t FilteredEventListeners::GetNumListeners() {
  return listeners_.size();
}

v8::LocalVector<v8::Function> FilteredEventListeners::GetListeners(
    mojom::EventFilteringInfoPtr filter,
    v8::Local<v8::Context> context) {
  std::set<int> ids = listener_tracker_->GetMatchingFilteredListeners(
      event_name_,
      filter ? std::move(filter) : mojom::EventFilteringInfo::New(),
      kIgnoreRoutingId);

  v8::LocalVector<v8::Function> listeners(context->GetIsolate());
  listeners.reserve(ids.size());
  for (const auto& listener : listeners_) {
    if (ids.count(listener.filter_id))
      listeners.push_back(listener.function.Get(context->GetIsolate()));
  }
  return listeners;
}

void FilteredEventListeners::Invalidate(v8::Local<v8::Context> context) {
  for (const auto& listener : listeners_)
    InvalidateListener(listener, false, context);
  listeners_.clear();
}

void FilteredEventListeners::LazilySetContextOwner(
    v8::Local<v8::Context> context) {
  if (context_owner_id_.empty()) {
    DCHECK(context_owner_id_getter_);
    context_owner_id_ = context_owner_id_getter_.Run(context);
    DCHECK(!context_owner_id_.empty());
  }
}

void FilteredEventListeners::InvalidateListener(
    const ListenerData& listener,
    bool was_manual,
    v8::Local<v8::Context> context) {
  DCHECK(!context_owner_id_.empty())
      << "The context owner must be instantiated if listeners were removed.";

  bool was_last_of_kind = false;
  std::unique_ptr<base::Value::Dict> filter;
  std::tie(was_last_of_kind, filter) =
      listener_tracker_->RemoveFilteredListener(context_owner_id_, event_name_,
                                                listener.filter_id);
  if (was_last_of_kind) {
    listeners_updated_.Run(event_name_,
                           binding::EventListenersChanged::
                               kLastListenerWithFilterForContextOwnerRemoved,
                           filter.get(), was_manual && supports_lazy_listeners_,
                           context);
  }
}

}  // namespace extensions
