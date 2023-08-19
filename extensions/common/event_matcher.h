// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_MATCHER_H_
#define EXTENSIONS_COMMON_EVENT_MATCHER_H_

#include <memory>

#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"

namespace extensions {
extern const char kEventFilterServiceTypeKey[];

// Matches EventFilteringInfos against a set of criteria. This is intended to
// be used by EventFilter which performs efficient URL matching across
// potentially many EventMatchers itself. This is why this class only exposes
// MatchNonURLCriteria() - URL matching is handled by EventFilter.
class EventMatcher {
 public:
  EventMatcher(std::unique_ptr<base::Value::Dict> filter, int routing_id);

  EventMatcher(const EventMatcher&) = delete;
  EventMatcher& operator=(const EventMatcher&) = delete;

  ~EventMatcher();

  // Returns true if |event_info| satisfies this matcher's criteria, not taking
  // into consideration any URL criteria.
  bool MatchNonURLCriteria(const mojom::EventFilteringInfo& event_info) const;

  // Retrieves the number of specified URL filters.
  int GetURLFilterCount() const;

  // Retrieves the URL filter at the index `i`.
  const base::Value::Dict* GetURLFilter(int i);

  // Returns true if there are any specified URL filters.
  bool HasURLFilters() const;

  base::Value::Dict* value() const { return filter_.get(); }
  int routing_id() const { return routing_id_; }

 private:
  // Returns the number of specified window types in the filter.
  int GetWindowTypeCount() const;
  // Populates `window_type_out` with the window type at index `i`,
  // return true if successful.
  bool GetWindowType(int i, std::string* window_type_out) const;
  // Returns the filter for service type, if one exists. Otherwise, returns
  // an empty string.
  std::string GetServiceTypeFilter() const;
  // Returns the specified instance ID in the filter, or 0 if none was
  // specified.
  int GetInstanceID() const;

  // Contains a dictionary that corresponds to a single event filter, eg:
  //
  // {url: [{hostSuffix: 'google.com'}]}
  //
  // The valid filter keys are event-specific.
  const std::unique_ptr<base::Value::Dict> filter_;

  const int routing_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_MATCHER_H_
