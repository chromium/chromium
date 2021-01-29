// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_REQUEST_CONTEXT_H_
#define MOJO_CORE_REQUEST_CONTEXT_H_

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/core/watch.h"

namespace base {
template <typename T>
class ThreadLocalPointer;
}

namespace mojo {
namespace core {

// A RequestContext is a thread-local object which exists for the duration of
// a single system API call. It is constructed immediately upon EDK entry and
// destructed immediately before returning to the caller, after any internal
// locks have been released.
//
// NOTE: It is legal to construct a RequestContext while another one already
// exists on the current thread, but it is not safe to use the nested context
// for any reason. Therefore it is important to always use
// |RequestContext::current()| rather than referring to any local instance
// directly.
class MOJO_SYSTEM_IMPL_EXPORT RequestContext {
 public:
  // Identifies the source of the current stack frame's RequestContext.
  enum class Source {
    LOCAL_API_CALL,
    SYSTEM,
  };

  // Constructs a RequestContext with a LOCAL_API_CALL Source.
  RequestContext();

  explicit RequestContext(Source source);
  ~RequestContext();

  // Returns the current thread-local RequestContext.
  static RequestContext* current();

  Source source() const { return source_; }

  // Adds a finalizer to this RequestContext corresponding to a watch callback
  // which should be triggered in response to some handle state change. If
  // the WatcherDispatcher hasn't been closed by the time this RequestContext is
  // destroyed, its WatchCallback will be invoked with |result| and |state|
  // arguments.
  void AddWatchNotifyFinalizer(scoped_refptr<Watch> watch,
                               MojoResult result,
                               const HandleSignalsState& state);

  // Adds a finalizer to this RequestContext corresponding to a watch callback
  // which should be triggered to notify of watch cancellation. This appends to
  // a separate finalizer list from AddWatchNotifyFinalizer, as pending
  // cancellations must always preempt other pending notifications.
  void AddWatchCancelFinalizer(scoped_refptr<Watch> watch);

 private:
  // Is this request context the current one?
  bool IsCurrent() const;

  struct WatchNotifyFinalizer {
    WatchNotifyFinalizer(scoped_refptr<Watch> watch,
                         MojoResult result,
                         const HandleSignalsState& state);
    WatchNotifyFinalizer(const WatchNotifyFinalizer& other);
    ~WatchNotifyFinalizer();

    scoped_refptr<Watch> watch;
    MojoResult result;
    HandleSignalsState state;
  };

  // NOTE: This upper bound was chosen somewhat arbitrarily after observing some
  // rare worst-case behavior in Chrome. A vast majority of RequestContexts only
  // ever accumulate 0 or 1 finalizers.
  static const size_t kStaticWatchFinalizersCapacity = 8;

  using WatchNotifyFinalizerList =
      base::StackVector<WatchNotifyFinalizer, kStaticWatchFinalizersCapacity>;
  using WatchCancelFinalizerList =
      base::StackVector<scoped_refptr<Watch>, kStaticWatchFinalizersCapacity>;

  const Source source_;

  WatchNotifyFinalizerList watch_notify_finalizers_;
  WatchCancelFinalizerList watch_cancel_finalizers_;

  // Pointer to the TLS context. Although this can easily be accessed via the
  // global LazyInstance, accessing a LazyInstance has a large cost relative to
  // the rest of this class and its usages.
  base::ThreadLocalPointer<RequestContext>* tls_context_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_REQUEST_CONTEXT_H_
