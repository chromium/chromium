// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No include guard, may be included multiple times.

// NULL out all the macros that need NULLing, so that multiple includes of
// the XXXX_messages_internal.h files will not generate noise.
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#undef IPC_ENUM_TRAITS_VALIDATE
#undef IPC_MESSAGE_DECL

#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent)
#define IPC_STRUCT_MEMBER(type, name, ...)
#define IPC_STRUCT_END()
#define IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_TRAITS_PARENT(type)
#define IPC_STRUCT_TRAITS_END()
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, validation_expression)
#define IPC_MESSAGE_DECL(...)
