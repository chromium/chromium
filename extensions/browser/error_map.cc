// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/error_map.h"

#include <utility>

#include "base/lazy_instance.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

// The maximum number of errors to be stored per extension.
const size_t kMaxErrorsPerExtension = 100;

base::LazyInstance<ErrorList>::DestructorAtExit g_empty_error_list =
    LAZY_INSTANCE_INITIALIZER;

// An incrementing counter for the next error id. Overflowing this is very
// unlikely, since the number of errors per extension is capped at 100.
int g_next_error_id = 1;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ErrorMap::Filter
ErrorMap::Filter::Filter(const ExtensionId& restrict_to_extension_id,
                         int restrict_to_type,
                         const std::set<int>& restrict_to_ids,
                         bool restrict_to_incognito)
    : restrict_to_extension_id(restrict_to_extension_id),
      restrict_to_type(restrict_to_type),
      restrict_to_ids(restrict_to_ids),
      restrict_to_incognito(restrict_to_incognito) {}

ErrorMap::Filter::Filter(const Filter& other) = default;

ErrorMap::Filter::~Filter() {
}

ErrorMap::Filter ErrorMap::Filter::ErrorsForExtension(
    const ExtensionId& extension_id) {
  return Filter(extension_id, -1, std::set<int>(), false);
}

ErrorMap::Filter ErrorMap::Filter::ErrorsForExtensionWithType(
    const ExtensionId& extension_id,
    ExtensionError::Type type) {
  return Filter(extension_id, static_cast<int>(type), std::set<int>(), false);
}

ErrorMap::Filter ErrorMap::Filter::ErrorsForExtensionWithIds(
    const ExtensionId& extension_id,
    const std::set<int>& ids) {
  return Filter(extension_id, -1, ids, false);
}

ErrorMap::Filter ErrorMap::Filter::ErrorsForExtensionWithTypeAndIds(
    const ExtensionId& extension_id,
    ExtensionError::Type type,
    const std::set<int>& ids) {
  return Filter(extension_id, static_cast<int>(type), ids, false);
}

ErrorMap::Filter ErrorMap::Filter::IncognitoErrors() {
  return Filter(std::string(), -1, std::set<int>(), true);
}

bool ErrorMap::Filter::Matches(const ExtensionError* error) const {
  if (restrict_to_type != -1 &&
      restrict_to_type != static_cast<int>(error->type())) {
    return false;
  }
  if (restrict_to_incognito && !error->from_incognito()) {
    return false;
  }
  if (!restrict_to_extension_id.empty() &&
      error->extension_id() != restrict_to_extension_id) {
    return false;
  }
  if (!restrict_to_ids.empty() && restrict_to_ids.count(error->id()) == 0) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ErrorMap::ExtensionEntry
class ErrorMap::ExtensionEntry {
 public:
  ExtensionEntry();

  ExtensionEntry(const ExtensionEntry&) = delete;
  ExtensionEntry& operator=(const ExtensionEntry&) = delete;

  ~ExtensionEntry();

  // Delete any errors in the entry that match the given ids and type, if
  // provided.
  // Returns true if any errors were deleted.
  bool DeleteErrors(const ErrorMap::Filter& filter);
  // Delete all errors in the entry.
  void DeleteAllErrors();

  // Add the error to the list, and return a weak reference.
  const ExtensionError* AddError(std::unique_ptr<ExtensionError> error);

  const ErrorList* list() const { return &list_; }

 private:
  // The list of all errors associated with the extension. The errors are
  // owned by the Entry (in turn owned by the ErrorMap) and are deleted upon
  // destruction.
  ErrorList list_;
};

ErrorMap::ExtensionEntry::ExtensionEntry() {
}

ErrorMap::ExtensionEntry::~ExtensionEntry() {
  DeleteAllErrors();
}

bool ErrorMap::ExtensionEntry::DeleteErrors(const Filter& filter) {
  bool deleted = false;
  for (auto iter = list_.begin(); iter != list_.end();) {
    if (filter.Matches(iter->get())) {
      iter = list_.erase(iter);
      deleted = true;
    } else {
      ++iter;
    }
  }
  return deleted;
}

void ErrorMap::ExtensionEntry::DeleteAllErrors() {
  list_.clear();
}

const ExtensionError* ErrorMap::ExtensionEntry::AddError(
    std::unique_ptr<ExtensionError> error) {
  for (auto iter = list_.begin(); iter != list_.end(); ++iter) {
    // If we find a duplicate error, remove the old error and add the new one,
    // incrementing the occurrence count of the error. We use the new error
    // for runtime errors, so we can link to the latest context, inspectable
    // view, etc.
    if (error->IsEqual(iter->get())) {
      error->set_occurrences((*iter)->occurrences() + 1);
      error->set_id((*iter)->id());
      list_.erase(iter);
      break;
    }
  }

  // If there are too many errors for an extension already, limit ourselves to
  // the most recent ones.
  if (list_.size() >= kMaxErrorsPerExtension) {
    list_.pop_front();
  }

  if (error->id() == 0) {
    error->set_id(g_next_error_id++);
  }

  list_.push_back(std::move(error));
  return list_.back().get();
}

////////////////////////////////////////////////////////////////////////////////
// ErrorMap
ErrorMap::ErrorMap() = default;

ErrorMap::~ErrorMap() {
  RemoveAllErrors();
}

const ErrorList& ErrorMap::GetErrorsForExtension(
    const ExtensionId& extension_id) const {
  auto iter = map_.find(extension_id);
  return iter != map_.end() ? *iter->second->list() : g_empty_error_list.Get();
}

const ExtensionError* ErrorMap::AddError(
    std::unique_ptr<ExtensionError> error) {
  std::unique_ptr<ExtensionEntry>& entry = map_[error->extension_id()];
  if (!entry) {
    entry = std::make_unique<ExtensionEntry>();
  }

  return entry->AddError(std::move(error));
}

void ErrorMap::RemoveErrors(const Filter& filter,
                            std::set<ExtensionId>* affected_ids) {
  if (!filter.restrict_to_extension_id.empty()) {
    auto iter = map_.find(filter.restrict_to_extension_id);
    if (iter != map_.end()) {
      if (iter->second->DeleteErrors(filter) && affected_ids) {
        affected_ids->insert(filter.restrict_to_extension_id);
      }
    }
  } else {
    for (auto& key_val : map_) {
      if (key_val.second->DeleteErrors(filter) && affected_ids) {
        affected_ids->insert(key_val.first);
      }
    }
  }
}

void ErrorMap::RemoveAllErrors() {
  map_.clear();
}

}  // namespace extensions
