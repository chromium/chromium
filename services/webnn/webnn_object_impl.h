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

  // Sequence task runner used for Mojo dispatch (scheduler runner when
  // present). Note: treat this as a posting/binding target; under
  // gpu::Scheduler, runner identity checks may not match execution context
  // exactly. Sequence correctness is enforced via `sequence_checker_`.
  const scoped_refptr<base::SequencedTaskRunner>& mojo_task_runner() const {
    return task_runner_;
  }

  // Closes the pipe to the renderer process and cancels pending callbacks
  // responses.
  void ResetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    GetMojoReceiver().reset();
  }

  // Similar to the method above, but also specifies a disconnect reason.
  void ResetMojoReceiver(std::string_view description) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    GetMojoReceiver().ResetWithReason(/*custom_reason_code=*/0, description);
  }

 protected:
  // Constructs the receiver and binds it to the Mojo pipe.
  template <typename MojoPendingReceiverType>
  WebNNObjectBase(MojoPendingReceiverType pending_receiver,
                  scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)),
        mojo_receiver_(this, std::move(pending_receiver), task_runner_) {
    mojo_receiver_.set_disconnect_handler(base::BindOnce(
        &WebNNObjectType::OnDisconnect, weak_factory_.GetWeakPtr()));
  }

  ~WebNNObjectBase() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // The receiver must be explicitly reset via ResetMojoReceiver() on
    // `task_runner_` before destruction. Implicitly destroying a bound
    // receiver may DCHECK in Mojo if destruction occurs on a different runner.
    DCHECK(!mojo_receiver_.is_bound())
        << "Receiver must be reset before destruction.";
  }

  // Returns the AssociatedReceiver bound to this implementation.
  MojoReceiverType& GetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mojo_receiver_;
  }

 protected:
  // This SequenceChecker is bound to the sequence where WebNNObjectBase is
  // constructed. All message dispatches must occur on this sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  const WebNNTokenType handle_;

  // The task runner on which the Mojo receiver is bound and must be used to
  // reset.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  MojoReceiverType mojo_receiver_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<WebNNObjectType> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
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
  // `task_runner` is the sequence where Mojo messages (including disconnects)
  // are dispatched for this object. When a GPU sequence exists, this is the
  // scheduler task runner; otherwise, it is the `owning_task_runner`.
  // `owning_task_runner` is the single-thread runner used for the object's
  // construction and for RefCountedDeleteOnSequence destruction.
  template <typename MojoPendingReceiverType>
  WebNNObjectImpl(MojoPendingReceiverType pending_receiver,
                  scoped_refptr<base::SequencedTaskRunner> task_runner,
                  scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : WebNNObjectBase<MojoInterface, WebNNTokenType, MojoReceiverType>(
            std::move(pending_receiver),
            std::move(task_runner)),
        base::RefCountedDeleteOnSequence<WebNNObjectType>(
            std::move(owning_task_runner)) {}

 protected:
  friend class base::RefCountedDeleteOnSequence<WebNNObjectImpl>;
  friend class base::DeleteHelper<WebNNObjectImpl>;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
