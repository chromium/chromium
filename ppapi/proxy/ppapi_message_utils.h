// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_MESSAGE_UTILS_H_
#define PPAPI_PROXY_PPAPI_MESSAGE_UTILS_H_

#include "base/pickle.h"
#include "base/tuple.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

namespace ppapi {

namespace internal {

// TupleTypeMatch* check whether a tuple type contains elements of the specified
// types. They are used to make sure the output parameters of UnpackMessage()
// match the corresponding message type.
template <class TupleType, class A>
struct TupleTypeMatch1 {
  static const bool kValue = false;
};
template <class A>
struct TupleTypeMatch1<std::tuple<A>, A> {
  static const bool kValue = true;
};

template <class TupleType, class A, class B>
struct TupleTypeMatch2 {
  static const bool kValue = false;
};
template <class A, class B>
struct TupleTypeMatch2<std::tuple<A, B>, A, B> {
  static const bool kValue = true;
};

template <class TupleType, class A, class B, class C>
struct TupleTypeMatch3 {
  static const bool kValue = false;
};
template <class A, class B, class C>
struct TupleTypeMatch3<std::tuple<A, B, C>, A, B, C> {
  static const bool kValue = true;
};

template <class TupleType, class A, class B, class C, class D>
struct TupleTypeMatch4 {
  static const bool kValue = false;
};
template <class A, class B, class C, class D>
struct TupleTypeMatch4<std::tuple<A, B, C, D>, A, B, C, D> {
  static const bool kValue = true;
};

template <class TupleType, class A, class B, class C, class D, class E>
struct TupleTypeMatch5 {
  static const bool kValue = false;
};
template <class A, class B, class C, class D, class E>
struct TupleTypeMatch5<std::tuple<A, B, C, D, E>, A, B, C, D, E> {
  static const bool kValue = true;
};

}  // namespace internal

template <class MsgClass, class A>
bool UnpackMessage(const IPC::Message& msg, A* a) {
  static_assert(
      (internal::TupleTypeMatch1<typename MsgClass::Param, A>::kValue),
      "tuple types should match");

  base::PickleIterator iter(msg);
  return IPC::ReadParam(&msg, &iter, a);
}

template <class MsgClass, class A, class B>
bool UnpackMessage(const IPC::Message& msg, A* a, B* b) {
  static_assert(
      (internal::TupleTypeMatch2<typename MsgClass::Param, A, B>::kValue),
      "tuple types should match");

  base::PickleIterator iter(msg);
  return IPC::ReadParam(&msg, &iter, a) && IPC::ReadParam(&msg, &iter, b);
}

template <class MsgClass, class A, class B, class C>
bool UnpackMessage(const IPC::Message& msg, A* a, B* b, C* c) {
  static_assert(
      (internal::TupleTypeMatch3<typename MsgClass::Param, A, B, C>::kValue),
      "tuple types should match");

  base::PickleIterator iter(msg);
  return IPC::ReadParam(&msg, &iter, a) &&
         IPC::ReadParam(&msg, &iter, b) &&
         IPC::ReadParam(&msg, &iter, c);
}

template <class MsgClass, class A, class B, class C, class D>
bool UnpackMessage(const IPC::Message& msg, A* a, B* b, C* c, D* d) {
  static_assert(
      (internal::TupleTypeMatch4<typename MsgClass::Param, A, B, C, D>::kValue),
      "tuple types should match");

  base::PickleIterator iter(msg);
  return IPC::ReadParam(&msg, &iter, a) &&
         IPC::ReadParam(&msg, &iter, b) &&
         IPC::ReadParam(&msg, &iter, c) &&
         IPC::ReadParam(&msg, &iter, d);
}

template <class MsgClass, class A, class B, class C, class D, class E>
bool UnpackMessage(const IPC::Message& msg, A* a, B* b, C* c, D* d, E* e) {
  static_assert(
      (internal::TupleTypeMatch5<
           typename MsgClass::Param, A, B, C, D, E>::kValue),
      "tuple types should match");

  base::PickleIterator iter(msg);
  return IPC::ReadParam(&msg, &iter, a) &&
         IPC::ReadParam(&msg, &iter, b) &&
         IPC::ReadParam(&msg, &iter, c) &&
         IPC::ReadParam(&msg, &iter, d) &&
         IPC::ReadParam(&msg, &iter, e);
}

}  // namespace ppapi

#endif  // PPAPI_PROXY_PPAPI_MESSAGE_UTILS_H_
