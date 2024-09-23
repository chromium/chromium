// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_
#define IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_

#include "base/task/task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {

class WebTaskTraits : public base::TaskTraits {
 public:
  struct ValidTrait : public base::TaskTraits::ValidTrait {
    // TODO(crbug.com/40217644): iOS never supported TaskPriority, but some
    // callers are providing it... Add support?
    ValidTrait(base::TaskPriority);

    // TODO(crbug.com/40108370): These traits are meaningless on WebThreads but
    // some callers of post_task.h had been using them in conjunction with
    // WebThread::ID traits. Remove such usage post-migration.
    ValidTrait(base::MayBlock);
    ValidTrait(base::TaskShutdownBehavior);
  };

  template <class... ArgTypes>
    requires base::trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr WebTaskTraits(ArgTypes... args) : base::TaskTraits(args...) {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_THREAD_WEB_TASK_TRAITS_H_
