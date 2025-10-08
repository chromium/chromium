// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_

#include "base/component_export.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
class WebNNObjectImpl
    : public MojoInterface,
      public base::RefCountedDeleteOnSequence<
          WebNNObjectImpl<MojoInterface, WebNNTokenType, MojoReceiverType>> {
 public:
  using WebNNObjectType =
      WebNNObjectImpl<MojoInterface, WebNNTokenType, MojoReceiverType>;

  WebNNObjectImpl(const WebNNObjectImpl&) = delete;
  WebNNObjectImpl& operator=(const WebNNObjectImpl&) = delete;

  // Called when the Mojo connection is lost.
  // Subclasses must implement this to trigger appropriate cleanup.
  virtual void OnDisconnect() = 0;

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

  const WebNNTokenType& handle() const { return handle_; }

  // Closes the pipe to the renderer process and cancels pending callbacks
  // responses.
  void ResetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    mojo_receiver_.reset();
  }

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        WebNNObjectType>::owning_task_runner();
  }

 protected:
  // Constructs the receiver and binds it to the Mojo pipe.
  // The scheduler_task_runner posts scheduled work (including disconnects) to
  // the GPU sequence. The owning_task_runner is the underlying single-thread
  // runner for the GPU sequence, used for object deletions.
  template <typename MojoPendingReceiverType>
  WebNNObjectImpl(
      MojoPendingReceiverType pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : base::RefCountedDeleteOnSequence<WebNNObjectType>(
            std::move(owning_task_runner)),
        mojo_receiver_(this,
                       std::move(pending_receiver),
                       std::move(scheduler_task_runner)) {
    mojo_receiver_.set_disconnect_handler(base::BindOnce(
        &WebNNObjectType::OnDisconnect, weak_factory_.GetWeakPtr()));
  }

  ~WebNNObjectImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  }

  // Returns the AssociatedReceiver bound to this implementation.
  // Only legal to call from within the stack frame of a message dispatch.
  MojoReceiverType& GetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    return mojo_receiver_;
  }

 protected:
  // This SequenceChecker is bound to the sequence where WebNNObjectImpl is
  // constructed. All messages dispatches and access to
  // the GPU scheduler must occur on this sequence.
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  friend class base::RefCountedDeleteOnSequence<WebNNObjectImpl>;
  friend class base::DeleteHelper<WebNNObjectImpl>;

  const WebNNTokenType handle_;

  MojoReceiverType mojo_receiver_ GUARDED_BY_CONTEXT(gpu_sequence_checker_);

  base::WeakPtrFactory<WebNNObjectType> weak_factory_
      GUARDED_BY_CONTEXT(gpu_sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
