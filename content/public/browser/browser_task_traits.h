// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// Tasks with this trait will not be executed inside a nested RunLoop.
//
// Note: This should rarely be required. Drivers of nested loops should instead
// make sure to be reentrant when allowing nested application tasks (also rare).
//
// TODO(https://crbug.com/876272): Investigate removing this trait -- and any
// logic for deferred tasks in MessageLoop.
struct NonNestable {};

// TaskTraits for running tasks on the browser threads.
//
// These traits enable the use of the //base/task/post_task.h APIs to post tasks
// to a BrowserThread.
//
// To post a task to the UI thread (analogous for IO thread):
//     base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, task);
//
// To obtain a TaskRunner for the UI thread (analogous for the IO thread):
//     base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
//
// Tasks posted to the same BrowserThread with the same traits will be executed
// in the order they were posted, regardless of the TaskRunners they were
// posted via.
//
// See //base/task/post_task.h for more detailed documentation.
//
// Posting to a BrowserThread must only be done after it was initialized (ref.
// BrowserMainLoop::CreateThreads() phase).
class CONTENT_EXPORT BrowserTaskTraitsExtension {
  using BrowserThreadIDFilter =
      base::trait_helpers::RequiredEnumTraitFilter<BrowserThread::ID>;
  using NonNestableFilter =
      base::trait_helpers::BooleanTraitFilter<NonNestable>;

 public:
  static constexpr uint8_t kExtensionId =
      base::TaskTraitsExtensionStorage::kFirstEmbedderExtensionId;

  struct ValidTrait : public base::TaskTraits::ValidTrait {
    using base::TaskTraits::ValidTrait::ValidTrait;

    ValidTrait(BrowserThread::ID);
    ValidTrait(NonNestable);
  };

  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr BrowserTaskTraitsExtension(ArgTypes... args)
      : browser_thread_(
            base::trait_helpers::GetTraitFromArgList<BrowserThreadIDFilter>(
                args...)),
        nestable_(!base::trait_helpers::GetTraitFromArgList<NonNestableFilter>(
            args...)) {}

  // Keep in sync with UiThreadTaskTraits.java
  constexpr base::TaskTraitsExtensionStorage Serialize() const {
    static_assert(8 == sizeof(BrowserTaskTraitsExtension),
                  "Update Serialize() and Parse() when changing "
                  "BrowserTaskTraitsExtension");
    return {kExtensionId,
            {static_cast<uint8_t>(browser_thread_),
             static_cast<uint8_t>(nestable_)}};
  }

  static const BrowserTaskTraitsExtension Parse(
      const base::TaskTraitsExtensionStorage& extension) {
    return BrowserTaskTraitsExtension(
        static_cast<BrowserThread::ID>(extension.data[0]),
        static_cast<bool>(extension.data[1]));
  }

  constexpr BrowserThread::ID browser_thread() const { return browser_thread_; }

  // Returns true if tasks with these traits may run in a nested RunLoop.
  constexpr bool nestable() const { return nestable_; }

 private:
  BrowserTaskTraitsExtension(BrowserThread::ID browser_thread, bool nestable)
      : browser_thread_(browser_thread), nestable_(nestable) {}

  BrowserThread::ID browser_thread_;
  bool nestable_;
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

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_TASK_TRAITS_H_
