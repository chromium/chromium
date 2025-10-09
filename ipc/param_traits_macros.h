// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_MACROS_H_
#define IPC_PARAM_TRAITS_MACROS_H_

#include <string>
#include <type_traits>

// Files that want to export their traits should do
//   #undef IPC_MESSAGE_EXPORT
//   #define IPC_MESSAGE_EXPORT VISIBILITY_MACRO
// immediately before using any of the macros below. Like the macros
// themselves, this needs to happen outside of the include guard.

// Traits generation for structs.
#define IPC_STRUCT_TRAITS_BEGIN(struct_name)                 \
  namespace IPC {                                            \
  template <>                                                \
  struct IPC_MESSAGE_EXPORT ParamTraits<struct_name> {       \
    typedef struct_name param_type;                          \
    static void Write(base::Pickle* m, const param_type& p); \
    static bool Read(const base::Pickle* m,                  \
                     base::PickleIterator* iter,             \
                     param_type* p);                         \
  };                                                         \
  }

#define IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_TRAITS_PARENT(type)
#define IPC_STRUCT_TRAITS_END()

// Convenience macro for defining enumerated type traits for types which are
// not range-checked by the IPC system. The author of the message handlers
// is responsible for all validation. This macro should not need to be
// subsequently redefined.
#define IPC_ENUM_TRAITS(type) \
  IPC_ENUM_TRAITS_VALIDATE(type, true)

// Convenience macro for defining enumerated type traits for types which are
// range-checked by the IPC system to be in the range of 0..maxvalue inclusive.
// This macro should not need to be subsequently redefined.
#define IPC_ENUM_TRAITS_MAX_VALUE(type, maxvalue) \
  IPC_ENUM_TRAITS_MIN_MAX_VALUE(type, 0, maxvalue)

// Convenience macro for defining enumerated type traits for types which are
// range-checked by the IPC system to be in the range of minvalue..maxvalue
// inclusive. This macro should not need to be subsequently redefined.
#define IPC_ENUM_TRAITS_MIN_MAX_VALUE(typ, minvalue, maxvalue)            \
  IPC_ENUM_TRAITS_VALIDATE(                                               \
      typ, (static_cast<std::underlying_type<typ>::type>(value) >=        \
                static_cast<std::underlying_type<typ>::type>(minvalue) && \
            static_cast<std::underlying_type<typ>::type>(value) <=        \
                static_cast<std::underlying_type<typ>::type>(maxvalue)))

// Traits generation for enums. This macro may be redefined later.
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, validation_expression) \
  namespace IPC {                                                  \
  template <>                                                      \
  struct IPC_MESSAGE_EXPORT ParamTraits<enum_name> {               \
    typedef enum_name param_type;                                  \
    static void Write(base::Pickle* m, const param_type& p);       \
    static bool Read(const base::Pickle* m,                        \
                     base::PickleIterator* iter,                   \
                     param_type* p);                               \
  };                                                               \
  }

#endif  // IPC_PARAM_TRAITS_MACROS_H_

