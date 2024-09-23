// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "content/common/content_export.h"

namespace content {

// Semantic annotations which tell the scheduler what type of task it's dealing
// with. This will be used by the scheduler for dynamic prioritization and for
// attribution in traces, etc... In general, BrowserTaskType::kDefault is what
// you want (it's implicit if you don't specify this trait). Only explicitly
// specify this trait if you carefully isolated a set of tasks that have no
// ordering requirements with anything else (in doubt, consult with
// scheduler-dev@chromium.org).
enum class BrowserTaskType {
  // A catch all for tasks that don't fit the types below.
  kDefault,

  // A subset of tasks related to user input.
  kUserInput,

  // Tasks processing navigation network request's response from the network
  // service.
  // NOTE: This task type should not be used for other navigation-related tasks
  // as they should be ordered w.r.t. IPC channel and the UI thread's default
  // task runner. Reach out to navigation-dev@ before adding new usages.
  // TODO(altimin): Make this content-internal.
  kNavigationNetworkResponse,

  // Tasks processing ServiceWorker's storage control's response.
  // TODO(chikamune): Make this content-internal.
  kServiceWorkerStorageControlResponse,

  // Task continuing navigation asynchronously after determining that no before
  // unload handlers are registered in the unloading render.
  // NOTE: This task type should not be used for other navigation-related tasks
  // as they should be ordered w.r.t. IPC channel and the UI thread's default
  // task runner. Reach out to navigation-dev@ before adding new usages.
  kBeforeUnloadBrowserResponse,

};

class CONTENT_EXPORT BrowserTaskTraits {
 public:
  struct ValidTrait {
    ValidTrait(BrowserTaskType);

    // TODO(crbug.com/40108370): Reconsider whether BrowserTaskTraits should
    // really be supporting base::TaskPriority.
    ValidTrait(base::TaskPriority);
  };

  template <class... ArgTypes>
    requires base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  // TaskTraits are intended to be implicitly-constructable (eg {}).
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BrowserTaskTraits(ArgTypes... args)
      : task_type_(
            base::trait_helpers::GetEnum<BrowserTaskType,
                                         BrowserTaskType::kDefault>(args...)),
        priority_(
            base::trait_helpers::GetEnum<base::TaskPriority,
                                         base::TaskPriority::USER_BLOCKING>(
                args...)) {}

  BrowserTaskType task_type() const { return task_type_; }

  base::TaskPriority priority() const { return priority_; }

 private:
  BrowserTaskType task_type_;
  base::TaskPriority priority_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
