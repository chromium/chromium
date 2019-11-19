// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace mojo {

// BinderMap is a simple helper class which maintains a registry of callbacks
// that can bind receivers for arbitrary Mojo interfaces. By default a BinderMap
// is empty and cannot bind any interfaces.
//
// Call |Add()| to register a new binder for a specific interface.
// Call |Bind()| to attempt to run a registered binder on the generic input
// receiver.
//
// BinderMap instances are safe to move across sequences but must be used from
// only once sequence at a time; they are also copyable.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) BinderMap {
 public:
  BinderMap();
  BinderMap(const BinderMap&);
  BinderMap(BinderMap&&);
  ~BinderMap();

  BinderMap& operator=(const BinderMap&);
  BinderMap& operator=(BinderMap&&);

  template <typename Interface>
  using Binder = base::RepeatingCallback<void(PendingReceiver<Interface>)>;

  // Adds a binder for Interface to this BinderMap. If |Bind()| is ever called
  // with a GenericPendingReceiver which matches Interface, this binder will be
  // invoked asynchronously on |task_runner|.
  //
  // It's an error to call |Add()| multiple times for the same interface.
  template <typename Interface>
  void Add(Binder<Interface> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner) {
    AddGenericBinder(
        Interface::Name_,
        MakeGenericBinder(std::move(binder), std::move(task_runner)));
  }

  // Temporary helper during the transition to new Mojo types.
  template <typename Interface>
  using LegacyBinder =
      base::RepeatingCallback<void(InterfaceRequest<Interface>)>;

  template <typename Interface>
  void Add(LegacyBinder<Interface> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner) {
    AddGenericBinder(
        Interface::Name_,
        MakeGenericBinder(std::move(binder), std::move(task_runner)));
  }

  // Attempts to bind |receiver| using one of the binders registered in this
  // BinderMap. If a matching binder is found, it is scheduled to run
  // asynchronously on its associated SequencedTaskRunner; the value in
  // |*receiver| is consumed immediately and this method returns |true|.
  //
  // If no matching binder is found, |*receiver| is left intact and this method
  // returns |false|.
  bool Bind(GenericPendingReceiver* receiver) WARN_UNUSED_RESULT;

  // Indicates whether or not |Bind()| would succeed if given |receiver|.
  bool CanBind(const GenericPendingReceiver& receiver) const;

 private:
  using GenericBinder = base::RepeatingCallback<void(GenericPendingReceiver)>;

  template <typename Interface>
  GenericBinder MakeGenericBinder(
      Binder<Interface> binder,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return base::BindRepeating(
        [](Binder<Interface> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           GenericPendingReceiver receiver) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(binder, receiver.As<Interface>()));
        },
        std::move(binder), std::move(task_runner));
  }

  template <typename Interface>
  GenericBinder MakeGenericBinder(
      LegacyBinder<Interface> binder,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return base::BindRepeating(
        [](LegacyBinder<Interface> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           GenericPendingReceiver receiver) {
          InterfaceRequest<Interface> request = receiver.As<Interface>();
          task_runner->PostTask(FROM_HERE,
                                base::BindOnce(binder, std::move(request)));
        },
        std::move(binder), std::move(task_runner));
  }

  void AddGenericBinder(base::StringPiece name, GenericBinder binder);

  std::map<std::string, GenericBinder> binders_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_
