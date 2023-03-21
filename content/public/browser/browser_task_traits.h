// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// Semantic annotations which tell the scheduler what type of task it's dealing
// with. This will be used by the scheduler for dynamic prioritization and for
// attribution in traces, etc... In general, BrowserTaskType::kDefault is what
// you want (it's implicit if you don't specify this trait). Only explicitly
// specify this trait if you carefully isolated a set of tasks that have no
// ordering requirements with anything else (in doubt, consult with
// scheduler-dev@chromium.org).
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
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

  // Used to validate values in Java
  kBrowserTaskType_Last
};

// TaskTraits for running tasks on the browser threads.
//
// To post a task to the UI thread (analogous for IO thread):
//     GetUIThreadTaskRunner({})->PostTask(FROM_HERE, task);
//
// To obtain a TaskRunner for the UI thread (analogous for the IO thread):
//     GetUIThreadTaskRunner({});
//
// Tasks posted to the same BrowserThread with the same traits will be executed
// in the order they were posted, regardless of the TaskRunners they were
// posted via.
//
// Posting to a BrowserThread must only be done after it was initialized (ref.
// BrowserMainLoop::CreateThreads() phase).
class CONTENT_EXPORT BrowserTaskTraitsExtension {
 public:
  static constexpr uint8_t kExtensionId =
      base::TaskTraitsExtensionStorage::kFirstEmbedderExtensionId;

  struct ValidTrait : public base::TaskTraits::ValidTrait {
    using base::TaskTraits::ValidTrait::ValidTrait;

    ValidTrait(BrowserThread::ID);
    ValidTrait(BrowserTaskType);
  };

  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr BrowserTaskTraitsExtension(ArgTypes... args)
      : browser_thread_(
            base::trait_helpers::GetEnum<BrowserThread::ID,
                                         BrowserThread::ID_COUNT>(args...)),
        task_type_(
            base::trait_helpers::GetEnum<BrowserTaskType,
                                         BrowserTaskType::kDefault>(args...)) {}

  constexpr base::TaskTraitsExtensionStorage Serialize() const {
    static_assert(8 == sizeof(BrowserTaskTraitsExtension),
                  "Update Serialize() and Parse() when changing "
                  "BrowserTaskTraitsExtension");
    return {kExtensionId,
            {static_cast<uint8_t>(browser_thread_),
             static_cast<uint8_t>(task_type_)}};
  }

  static const BrowserTaskTraitsExtension Parse(
      const base::TaskTraitsExtensionStorage& extension) {
    return BrowserTaskTraitsExtension(
        static_cast<BrowserThread::ID>(extension.data[0]),
        static_cast<BrowserTaskType>(extension.data[1]));
  }

  constexpr BrowserThread::ID browser_thread() const {
    // TODO(1026641): Migrate to BrowserTaskTraits under which BrowserThread is
    // not a trait. Until then, only code that knows traits have explicitly set
    // the BrowserThread trait should check this field.
    DCHECK_NE(browser_thread_, BrowserThread::ID_COUNT);
    return browser_thread_;
  }

  constexpr BrowserTaskType task_type() const { return task_type_; }

 private:
  BrowserTaskTraitsExtension(BrowserThread::ID browser_thread,
                             BrowserTaskType task_type)
      : browser_thread_(browser_thread), task_type_(task_type) {}

  BrowserThread::ID browser_thread_;
  BrowserTaskType task_type_;
};

template <class... ArgTypes,
          class = std::enable_if_t<base::trait_helpers::AreValidTraits<
              BrowserTaskTraitsExtension::ValidTrait,
              ArgTypes...>::value>>
constexpr base::TaskTraitsExtensionStorage MakeTaskTraitsExtension(
    ArgTypes&&... args) {
  return BrowserTaskTraitsExtension(std::forward<ArgTypes>(args)...)
      .Serialize();
}

class CONTENT_EXPORT BrowserTaskTraits : public base::TaskTraits {
 public:
  struct ValidTrait : public base::TaskTraits::ValidTrait {
    ValidTrait(BrowserTaskType);

    // TODO(1026641): Reconsider whether BrowserTaskTraits should really be
    // supporting base::TaskPriority.
    ValidTrait(base::TaskPriority);

    // TODO(1026641): These traits are meaningless on BrowserThreads but some
    // callers of post_task.h had been using them in conjunction with
    // BrowserThread::ID traits. Remove such usage post-migration.
    ValidTrait(base::MayBlock);
    ValidTrait(base::TaskShutdownBehavior);
  };

  // TODO(1026641): Get rid of BrowserTaskTraitsExtension and store its member
  // |task_type_| directly in BrowserTaskTraits.
  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr BrowserTaskTraits(ArgTypes... args) : base::TaskTraits(args...) {}

  BrowserTaskType task_type() {
    return GetExtension<BrowserTaskTraitsExtension>().task_type();
  }
};

static_assert(sizeof(BrowserTaskTraits) == sizeof(base::TaskTraits),
              "During the migration away from BrowserTasktraitsExtension, "
              "BrowserTaskTraits must only use base::TaskTraits for storage "
              "to prevent slicing.");

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
