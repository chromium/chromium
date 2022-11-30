// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_LOG_MACROS_H_
#define IPC_PARAM_TRAITS_LOG_MACROS_H_

#include <string>

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

// Set up so next include will generate log methods.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name)                                \
  void ParamTraits<struct_name>::Log(const param_type& p, std::string* l) { \
    bool needs_comma = false;                                               \
    (void)needs_comma;                                                      \
    l->append("(");
#define IPC_STRUCT_TRAITS_MEMBER(name) \
    if (needs_comma) \
      l->append(", "); \
    LogParam(p.name, l); \
    needs_comma = true;
#define IPC_STRUCT_TRAITS_PARENT(type) \
    if (needs_comma) \
      l->append(", "); \
      ParamTraits<type>::Log(p, l); \
      needs_comma = true;
#define IPC_STRUCT_TRAITS_END() \
  l->append(")");               \
  }

#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, validation_expression) \
  void ParamTraits<enum_name>::Log(const param_type& p, std::string* l) { \
    LogParam(static_cast<int>(p), l); \
  }

#endif  // IPC_PARAM_TRAITS_LOG_MACROS_H_

