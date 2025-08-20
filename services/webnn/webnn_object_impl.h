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

template <typename MojoInterface, typename WebNNTokenType>
  requires internal::IsSupportedTokenType<WebNNTokenType>
class WebNNObjectImpl : public MojoInterface,
                        public base::RefCountedThreadSafe<
                            WebNNObjectImpl<MojoInterface, WebNNTokenType>> {
 public:
  using WebNNObjectType = WebNNObjectImpl<MojoInterface, WebNNTokenType>;

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

 protected:
  // WebNNReceiverBinding manages the lifetime and disconnect handling of a
  // mojo::AssociatedReceiver bound to a WebNNObjectImpl implementation.
  // It is reference-counted and deleted on the sequence used for message
  // dispatch.
  //
  // Lifecycle contract:
  // - Owned via scoped_refptr by WebNNObjectImpl.
  //
  // This design guarantees:
  // - The mojo::AssociatedReceiver is both created and destroyed on the correct
  // sequence.
  // - Disconnect handling is safely posted back to sequence owning
  // WebNNObjectImpl.
  class WebNNReceiverBinding final
      : public base::RefCountedDeleteOnSequence<WebNNReceiverBinding> {
   public:
    WebNNReceiverBinding(
        base::WeakPtr<WebNNObjectType> impl,
        mojo::PendingAssociatedReceiver<MojoInterface> pending_receiver,
        scoped_refptr<base::SequencedTaskRunner> mojo_task_runner,
        scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
        : base::RefCountedDeleteOnSequence<WebNNReceiverBinding>(
              mojo_task_runner),
          receiver_(impl.get(),
                    std::move(pending_receiver),
                    std::move(mojo_task_runner)) {
      CHECK(owning_task_runner);
      receiver_.set_disconnect_handler(base::BindPostTask(
          std::move(owning_task_runner),
          base::BindOnce(&WebNNObjectType::OnDisconnect, impl)));
    }

    mojo::AssociatedReceiver<MojoInterface>& GetMojoReceiver() {
      return receiver_;
    }

   private:
    friend class base::RefCountedDeleteOnSequence<WebNNReceiverBinding>;

    mojo::AssociatedReceiver<MojoInterface> receiver_;
  };

  // Constructs the receiver and binds it to the Mojo pipe.
  // The owning_task_runner is where the disconnect is posted.
  WebNNObjectImpl(
      mojo::PendingAssociatedReceiver<MojoInterface> pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : owning_task_runner_(std::move(owning_task_runner)) {
    mojo_receiver_binding_ = base::MakeRefCounted<WebNNReceiverBinding>(
        weak_factory_.GetWeakPtr(), std::move(pending_receiver),
        base::SequencedTaskRunner::GetCurrentDefault(), owning_task_runner_);
  }

  ~WebNNObjectImpl() override = default;

  // Returns the AssociatedReceiver bound to this implementation.
  // Only legal to call from within the stack frame of a message dispatch.
  mojo::AssociatedReceiver<MojoInterface>& GetMojoReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mojo_sequence_checker_);
    return mojo_receiver_binding_->GetMojoReceiver();
  }

  // Posts a task to the owning sequence.
  // Only legal to call from within the stack frame of a message dispatch.
  void PostTaskToOwningTaskRunner(base::OnceClosure task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mojo_sequence_checker_);
    owning_task_runner_->PostTask(FROM_HERE, std::move(task));
  }

 private:
  // This SequenceChecker is bound to the sequence where WebNNObjectImpl is
  // constructed. All Mojo message dispatches and access to
  // WebNNReceiverBinding must occur on this sequence.
  SEQUENCE_CHECKER(mojo_sequence_checker_);

  friend class base::RefCountedThreadSafe<WebNNObjectImpl>;

  const WebNNTokenType handle_;

  const scoped_refptr<base::SequencedTaskRunner> owning_task_runner_;

  // WebNNReceiverBinding is exclusively owned and only referenced here.
  // Must be destructed on the mojo task runner via
  // RefCountedDeleteOnSequence.
  scoped_refptr<WebNNReceiverBinding> mojo_receiver_binding_
      GUARDED_BY_CONTEXT(mojo_sequence_checker_);

  base::WeakPtrFactory<WebNNObjectType> weak_factory_
      GUARDED_BY_CONTEXT(mojo_sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
