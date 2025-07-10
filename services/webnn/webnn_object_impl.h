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

template <typename WebNNTokenType>
  requires internal::IsSupportedTokenType<WebNNTokenType>
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNObjectImpl {
 public:
  WebNNObjectImpl() = default;

  explicit WebNNObjectImpl(WebNNTokenType handle)
      : handle_(std::move(handle)) {}

  virtual ~WebNNObjectImpl() = default;

  WebNNObjectImpl(const WebNNObjectImpl&) = delete;
  WebNNObjectImpl& operator=(const WebNNObjectImpl&) = delete;

  const WebNNTokenType& handle() const { return handle_; }

  // Defines a "transparent" comparator so that unique_ptr keys to
  // WebNNObjectImpl instances can be compared against tokens for lookup in
  // associative containers like base::flat_set.
  template <typename WebNNObjectImplType>
  struct Comparator {
    using is_transparent = WebNNTokenType;

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const WebNNTokenType& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
                    const WebNNTokenType& rhs) const {
      return lhs->handle() < rhs;
    }

    bool operator()(const scoped_refptr<WebNNObjectImplType>& lhs,
                    const scoped_refptr<WebNNObjectImplType>& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    bool operator()(const WebNNTokenType& lhs,
                    const scoped_refptr<WebNNObjectImplType>& rhs) const {
      return lhs < rhs->handle();
    }

    bool operator()(const scoped_refptr<WebNNObjectImplType>& lhs,
                    const WebNNTokenType& rhs) const {
      return lhs->handle() < rhs;
    }
  };

 private:
  const WebNNTokenType handle_;
};

template <typename MojoInterface>
class WebNNReceiverImpl;

// WebNNReceiverBinding manages the lifetime and disconnect handling of a
// mojo::AssociatedReceiver bound to a WebNNReceiverImpl implementation.
// It is reference-counted and deleted on the sequence used for message
// dispatch.
//
// Lifecycle contract:
// - Owned via scoped_refptr by WebNNReceiverImpl.
// - `impl_` is a WeakPtr and is guaranteed to remain valid for the lifetime of
//   WebNNReceiverBinding because the wrapper is destroyed before or with its
//   parent.
//
// This design guarantees:
// - The mojo::AssociatedReceiver is both created and destroyed on the correct
// sequence.
// - Disconnect handling is safely posted back to sequence owning
// WebNNReceiverImpl.
template <typename MojoInterface>
class WebNNReceiverBinding final : public base::RefCountedDeleteOnSequence<
                                       WebNNReceiverBinding<MojoInterface>> {
 public:
  WebNNReceiverBinding(
      base::WeakPtr<WebNNReceiverImpl<MojoInterface>> impl,
      mojo::PendingAssociatedReceiver<MojoInterface> pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> mojo_task_runner,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : base::RefCountedDeleteOnSequence<WebNNReceiverBinding<MojoInterface>>(
            mojo_task_runner),
        impl_(std::move(impl)),
        receiver_(impl_.get(),
                  std::move(pending_receiver),
                  std::move(mojo_task_runner)) {
    CHECK(owning_task_runner);
    // Safe to use base::Unretained because `this` is owned by `impl_`,
    // so it will be destroyed before `impl_` is deleted.
    receiver_.set_disconnect_handler(base::BindPostTask(
        std::move(owning_task_runner),
        base::BindOnce(&WebNNReceiverBinding<MojoInterface>::OnDisconnect,
                       base::Unretained(this))));
  }

  mojo::AssociatedReceiver<MojoInterface>& GetMojoReceiver() {
    return receiver_;
  }

 private:
  friend class base::RefCountedDeleteOnSequence<
      WebNNReceiverBinding<MojoInterface>>;
  friend class base::DeleteHelper<WebNNReceiverBinding<MojoInterface>>;

  // Called when the Mojo pipe is disconnected. Forwards the callback to the
  // implementation so it can handle cleanup or potentially trigger
  // self-deletion.
  //
  // Note: WebNNReceiverBinding does not own the implementation. This separation
  // ensures correct sequence-bound cleanup and avoids use-after-free.
  void OnDisconnect() {
    if (impl_) {
      impl_->OnDisconnect();
    }
  }

  // WeakPtr to the owning implementation. Valid for the entire lifetime of
  // WebNNReceiverBinding. See lifecycle contract above.
  base::WeakPtr<WebNNReceiverImpl<MojoInterface>> impl_;
  mojo::AssociatedReceiver<MojoInterface> receiver_;
};

// TODO(crbug.com/345352987): merge WebNNObjectImpl with WebNNReceiverImpl.
template <typename MojoInterface>
class WebNNReceiverImpl
    : public MojoInterface,
      public base::RefCountedThreadSafe<WebNNReceiverImpl<MojoInterface>> {
 public:
  WebNNReceiverImpl(const WebNNReceiverImpl&) = delete;
  WebNNReceiverImpl& operator=(const WebNNReceiverImpl&) = delete;

  // Called when the Mojo connection is lost.
  // Subclasses must implement this to trigger appropriate cleanup.
  virtual void OnDisconnect() = 0;

 protected:
  // Constructs the receiver and binds it to the Mojo pipe.
  // The owning_task_runner is where the disconnect is posted.
  WebNNReceiverImpl(
      mojo::PendingAssociatedReceiver<MojoInterface> pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : owning_task_runner_(std::move(owning_task_runner)) {
    mojo_receiver_binding_ =
        base::MakeRefCounted<WebNNReceiverBinding<MojoInterface>>(
            weak_factory_.GetWeakPtr(), std::move(pending_receiver),
            base::SequencedTaskRunner::GetCurrentDefault(),
            owning_task_runner_);
  }

  ~WebNNReceiverImpl() override = default;

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
  // This SequenceChecker is bound to the sequence where WebNNReceiverImpl is
  // constructed. All Mojo message dispatches and access to
  // WebNNReceiverBinding must occur on this sequence.
  SEQUENCE_CHECKER(mojo_sequence_checker_);

  friend class base::RefCountedThreadSafe<WebNNReceiverImpl>;

  const scoped_refptr<base::SequencedTaskRunner> owning_task_runner_;

  // WebNNReceiverBinding is exclusively owned and only referenced here.
  // Must be destructed on the mojo task runner via
  // RefCountedDeleteOnSequence.
  scoped_refptr<WebNNReceiverBinding<MojoInterface>> mojo_receiver_binding_
      GUARDED_BY_CONTEXT(mojo_sequence_checker_);

  base::WeakPtrFactory<WebNNReceiverImpl<MojoInterface>> weak_factory_
      GUARDED_BY_CONTEXT(mojo_sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
