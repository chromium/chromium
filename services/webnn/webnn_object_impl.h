// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

namespace internal {
// Supported WebNN token types. The list can be expanded as needed.
// Adding a new type must be explicitly instantiated in the cpp.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTokenType = IsAnyOf<T,
                                       blink::WebNNPendingConstantToken,
                                       blink::WebNNContextToken,
                                       blink::WebNNTensorToken,
                                       blink::WebNNGraphToken>;
}  // namespace internal

template <typename MojoInterface,
          typename WebNNTokenType,
          typename MojoReceiverType>
  requires internal::IsSupportedTokenType<WebNNTokenType>
class WebNNObjectBase : public MojoInterface {
 public:
  using WebNNObjectType =
      WebNNObjectBase<MojoInterface, WebNNTokenType, MojoReceiverType>;

  WebNNObjectBase(const WebNNObjectBase&) = delete;
  WebNNObjectBase& operator=(const WebNNObjectBase&) = delete;

  // Called when the Mojo connection is lost.
  // Subclasses must implement this to trigger appropriate cleanup.
  virtual void OnDisconnect() = 0;

  const WebNNTokenType& handle() const { return handle_; }

  // Closes the pipe to the renderer process and cancels pending callbacks
  // responses.
  void ResetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    mojo_receiver_.reset();
  }

 protected:
  // Constructs the receiver and binds it to the Mojo pipe.
  template <typename MojoPendingReceiverType>
  WebNNObjectBase(
      MojoPendingReceiverType pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner)
      : mojo_receiver_(this,
                       std::move(pending_receiver),
                       std::move(scheduler_task_runner)) {
    mojo_receiver_.set_disconnect_handler(base::BindOnce(
        &WebNNObjectType::OnDisconnect, weak_factory_.GetWeakPtr()));
  }

  ~WebNNObjectBase() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  }

  // Returns the AssociatedReceiver bound to this implementation.
  // Only legal to call from within the stack frame of a message dispatch.
  MojoReceiverType& GetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    return mojo_receiver_;
  }

 protected:
  // This SequenceChecker is bound to the sequence where WebNNObjectBase is
  // constructed. All messages dispatches and access to the GPU scheduler must
  // occur on this sequence.
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  const WebNNTokenType handle_;

  MojoReceiverType mojo_receiver_ GUARDED_BY_CONTEXT(gpu_sequence_checker_);

  base::WeakPtrFactory<WebNNObjectType> weak_factory_
      GUARDED_BY_CONTEXT(gpu_sequence_checker_){this};
};

template <typename MojoInterface,
          typename WebNNTokenType,
          typename MojoReceiverType>
  requires internal::IsSupportedTokenType<WebNNTokenType>
class WebNNObjectImpl
    : public WebNNObjectBase<MojoInterface, WebNNTokenType, MojoReceiverType>,
      public base::RefCountedDeleteOnSequence<
          WebNNObjectImpl<MojoInterface, WebNNTokenType, MojoReceiverType>> {
 public:
  using WebNNObjectType =
      WebNNObjectImpl<MojoInterface, WebNNTokenType, MojoReceiverType>;

  // Defines a "transparent" comparator so that scoped_refptr keys to
  // WebNNObjectImpl instances can be compared against tokens for lookup in
  // associative containers like base::flat_set.
  struct Comparator {
    using is_transparent = WebNNTokenType;

    bool operator()(const scoped_refptr<WebNNObjectImpl>& lhs,
                    const scoped_refptr<WebNNObjectImpl>& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    bool operator()(const WebNNTokenType& lhs,
                    const scoped_refptr<WebNNObjectImpl>& rhs) const {
      return lhs < rhs->handle();
    }

    bool operator()(const scoped_refptr<WebNNObjectImpl>& lhs,
                    const WebNNTokenType& rhs) const {
      return lhs->handle() < rhs;
    }
  };

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        WebNNObjectType>::owning_task_runner();
  }

 protected:
  // The scheduler_task_runner posts scheduled work (including disconnects) to
  // the GPU sequence. The owning_task_runner is the underlying single-thread
  // runner for the GPU sequence, used for object deletions.
  template <typename MojoPendingReceiverType>
  WebNNObjectImpl(
      MojoPendingReceiverType pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : WebNNObjectBase<MojoInterface, WebNNTokenType, MojoReceiverType>(
            std::move(pending_receiver),
            std::move(scheduler_task_runner)),
        base::RefCountedDeleteOnSequence<WebNNObjectType>(
            std::move(owning_task_runner)) {}

 protected:
  friend class base::RefCountedDeleteOnSequence<WebNNObjectImpl>;
  friend class base::DeleteHelper<WebNNObjectImpl>;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
