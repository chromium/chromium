// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_READ_MACROS_H_
#define IPC_PARAM_TRAITS_READ_MACROS_H_

// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate read methods.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name)                              \
  bool ParamTraits<struct_name>::Read(                                    \
      const base::Pickle* m, base::PickleIterator* iter, param_type* p) { \
  return
#define IPC_STRUCT_TRAITS_MEMBER(name) ReadParam(m, iter, &p->name) &&
#define IPC_STRUCT_TRAITS_PARENT(type) ParamTraits<type>::Read(m, iter, p) &&
#define IPC_STRUCT_TRAITS_END() 1; }

#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, validation_expression)        \
  bool ParamTraits<enum_name>::Read(                                      \
      const base::Pickle* m, base::PickleIterator* iter, param_type* p) { \
    int value;                                                            \
    if (!iter->ReadInt(&value))                                           \
      return false;                                                       \
    if (!(validation_expression))                                         \
      return false;                                                       \
    *p = static_cast<param_type>(value);                                  \
    return true;                                                          \
  }

#endif  // IPC_PARAM_TRAITS_READ_MACROS_H_

