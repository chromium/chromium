// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_
#define IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "base/task/task_traits_extension.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {

// Tasks with this trait will not be executed inside a nested RunLoop.
//
// Note: This should rarely be required. Drivers of nested loops should instead
// make sure to be reentrant when allowing nested application tasks (also rare).
//
// TODO(crbug.com/876272): Investigate removing this trait -- and any logic for
// deferred tasks in MessageLoop.
struct NonNestable {};

// TaskTraits for running tasks on a WebThread.
//
// To post a task to the UI thread (analogous for IO thread):
//     web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, task);
//
// To obtain a TaskRunner for the UI thread (analogous for the IO thread):
//     web::GetUIThreadTaskRunner({});
//
// Tasks posted to the same WebThread with the same traits will be executed
// in the order they were posted, regardless of the TaskRunners they were
// posted via.
//
// Posting to a WebThread must only be done after it was initialized (ref.
// WebMainLoop::CreateThreads() phase).
class WebTaskTraitsExtension {
 public:
  static constexpr uint8_t kExtensionId =
      base::TaskTraitsExtensionStorage::kFirstEmbedderExtensionId;

  struct ValidTrait : public base::TaskTraits::ValidTrait {
    using base::TaskTraits::ValidTrait::ValidTrait;

    ValidTrait(WebThread::ID);
    ValidTrait(NonNestable);
  };

  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr WebTaskTraitsExtension(ArgTypes... args)
      : web_thread_(
            base::trait_helpers::GetEnum<WebThread::ID, WebThread::ID_COUNT>(
                args...)),
        nestable_(!base::trait_helpers::HasTrait<NonNestable, ArgTypes...>()) {}

  constexpr base::TaskTraitsExtensionStorage Serialize() const {
    static_assert(8 == sizeof(WebTaskTraitsExtension),
                  "Update Serialize() and Parse() when changing "
                  "WebTaskTraitsExtension");
    return {
        kExtensionId,
        {static_cast<uint8_t>(web_thread_), static_cast<uint8_t>(nestable_)}};
  }

  static const WebTaskTraitsExtension Parse(
      const base::TaskTraitsExtensionStorage& extension) {
    return WebTaskTraitsExtension(static_cast<WebThread::ID>(extension.data[0]),
                                  static_cast<bool>(extension.data[1]));
  }

  constexpr WebThread::ID web_thread() const {
    // TODO(crbug.com/1026641): Migrate to WebTaskTraits under which WebThread
    // is not a trait. Until then, only code that knows traits have explicitly
    // set the WebThread trait should check this field.
    DCHECK_NE(web_thread_, WebThread::ID_COUNT);
    return web_thread_;
  }

  // Returns true if tasks with these traits may run in a nested RunLoop.
  constexpr bool nestable() const { return nestable_; }

 private:
  WebTaskTraitsExtension(WebThread::ID web_thread, bool nestable)
      : web_thread_(web_thread), nestable_(nestable) {}

  WebThread::ID web_thread_;
  bool nestable_;
};

template <class... ArgTypes,
          class = std::enable_if_t<base::trait_helpers::AreValidTraits<
              WebTaskTraitsExtension::ValidTrait,
              ArgTypes...>::value>>
constexpr base::TaskTraitsExtensionStorage MakeTaskTraitsExtension(
    ArgTypes&&... args) {
  return WebTaskTraitsExtension(std::forward<ArgTypes>(args)...).Serialize();
}

class WebTaskTraits : public base::TaskTraits {
 public:
  struct ValidTrait : public base::TaskTraits::ValidTrait {
    ValidTrait(NonNestable);

    // TODO(crbug.com/1304248): iOS never supported TaskPriority, but some
    // callers are providing it... Add support?
    ValidTrait(base::TaskPriority);

    // TODO(crbug.com/1026641): These traits are meaningless on WebThreads but
    // some callers of post_task.h had been using them in conjunction with
    // WebThread::ID traits. Remove such usage post-migration.
    ValidTrait(base::MayBlock);
    ValidTrait(base::TaskShutdownBehavior);
  };

  // TODO(crbug.com/1026641): Get rid of WebTaskTraitsExtension and store its
  // members (`nestable_`) directly in WebTaskTraits.
  template <
      class... ArgTypes,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr WebTaskTraits(ArgTypes... args) : base::TaskTraits(args...) {}

  // Returns true if tasks with these traits may run in a nested RunLoop.
  bool nestable() const {
    return GetExtension<WebTaskTraitsExtension>().nestable();
  }
};

static_assert(sizeof(WebTaskTraits) == sizeof(base::TaskTraits),
              "During the migration away from WebTasktraitsExtension, "
              "WebTaskTraits must only use base::TaskTraits for storage "
              "to prevent slicing.");

}  // namespace web

#endif  // IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_
