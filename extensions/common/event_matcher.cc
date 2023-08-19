// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_matcher.h"

#include <utility>

#include "base/functional/callback.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

namespace {
const char kUrlFiltersKey[] = "url";
const char kWindowTypesKey[] = "windowTypes";
}

namespace extensions {

const char kEventFilterServiceTypeKey[] = "serviceType";

EventMatcher::EventMatcher(std::unique_ptr<base::Value::Dict> filter,
                           int routing_id)
    : filter_(std::move(filter)), routing_id_(routing_id) {}

EventMatcher::~EventMatcher() {
}

bool EventMatcher::MatchNonURLCriteria(
    const mojom::EventFilteringInfo& event_info) const {
  if (event_info.has_instance_id) {
    return event_info.instance_id == GetInstanceID();
  }

  if (event_info.window_type) {
    int window_type_count = GetWindowTypeCount();
    for (int i = 0; i < window_type_count; i++) {
      std::string window_type;
      if (GetWindowType(i, &window_type) &&
          window_type == *event_info.window_type) {
        return true;
      }
    }
    return false;
  }

  if (event_info.has_window_exposed_by_default) {
    // An event with a |window_exposed_by_default| set is only
    // relevant to the listener if no window type filter is set.
    if (GetWindowTypeCount() > 0) {
      return false;
    }
    return event_info.window_exposed_by_default;
  }

  const std::string& service_type_filter = GetServiceTypeFilter();
  return service_type_filter.empty() ||
         (event_info.service_type &&
          service_type_filter == *event_info.service_type);
}

int EventMatcher::GetURLFilterCount() const {
  base::Value::List* url_filters = filter_->FindList(kUrlFiltersKey);
  if (url_filters)
    return url_filters->size();
  return 0;
}

const base::Value::Dict* EventMatcher::GetURLFilter(int i) {
  base::Value::List* url_filters = filter_->FindList(kUrlFiltersKey);
  if (url_filters)
    return (*url_filters)[i].GetIfDict();
  return nullptr;
}

bool EventMatcher::HasURLFilters() const {
  return GetURLFilterCount() != 0;
}

std::string EventMatcher::GetServiceTypeFilter() const {
  std::string service_type_filter;
  if (const std::string* ptr =
          filter_->FindString(kEventFilterServiceTypeKey)) {
    if (base::IsStringASCII(*ptr))
      service_type_filter = *ptr;
  }
  return service_type_filter;
}

int EventMatcher::GetInstanceID() const {
  return filter_->FindInt("instanceId").value_or(0);
}

int EventMatcher::GetWindowTypeCount() const {
  base::Value::List* window_type_filters = filter_->FindList(kWindowTypesKey);
  if (window_type_filters)
    return window_type_filters->size();
  return 0;
}

bool EventMatcher::GetWindowType(int i, std::string* window_type_out) const {
  base::Value::List* window_types = filter_->FindList(kWindowTypesKey);
  if (window_types) {
    if (i >= 0 && static_cast<size_t>(i) < window_types->size() &&
        (*window_types)[i].is_string()) {
      *window_type_out = (*window_types)[i].GetString();
      return true;
    }
  }
  return false;
}

}  // namespace extensions
