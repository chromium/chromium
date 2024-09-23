// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "extensions/common/api/storage.h"

namespace extensions {

enum class StorageAreaNamespace;

using SettingsChangedCallback =
    base::RepeatingCallback<void(const std::string&,
                                 StorageAreaNamespace,
                                 std::optional<api::storage::AccessLevel>,
                                 base::Value)>;

using SequenceBoundSettingsChangedCallback =
    base::StrongAlias<class SequenceBoundSettingsChangedCallbackTag,
                      SettingsChangedCallback>;

// Returns a callback that is guaranteed to run on |task_runner|. This should be
// used when the callback is invoked from other sequences.
inline SequenceBoundSettingsChangedCallback
GetSequenceBoundSettingsChangedCallback(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    SettingsChangedCallback callback,
    const base::Location& location = FROM_HERE) {
  return SequenceBoundSettingsChangedCallback(base::BindPostTask(
      std::move(task_runner), std::move(callback), location));
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_
