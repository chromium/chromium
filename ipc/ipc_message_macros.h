// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defining IPC Messages
//
// Your IPC messages will be defined by macros inside of an XXX_messages.h
// header file.  Most of the time, the system can automatically generate all
// of messaging mechanism from these definitions, but sometimes some manual
// coding is required.  In these cases, you will also have an XXX_messages.cc
// implementation file as well.
//
// The senders of your messages will include your XXX_messages.h file to
// get the full set of definitions they need to send your messages.
//
// Each XXX_messages.h file must be registered with the IPC system.  This
// requires adding two things:
//   - An XXXMsgStart value to the IPCMessageStart enum in ipc_message_start.h
//   - An inclusion of XXX_messages.h file in a message generator .h file
//
// The XXXMsgStart value is an enumeration that ensures uniqueness for
// each different message file.  Later, you will use this inside your
// XXX_messages.h file before invoking message declaration macros:
//     #define IPC_MESSAGE_START XXXMsgStart
//       ( ... your macro invocations go here ... )
//
// Message Generator Files
//
// A message generator .h header file pulls in all other message-declaring
// headers for a given component.  It is included by a message generator
// .cc file, which is where all the generated code will wind up.  Typically,
// you will use an existing generator (e.g. common_message_generator.cc
// in /chrome/common), but there are circumstances where you may add a
// new one.
//
// In the rare circumstances where you can't re-use an existing file,
// your YYY_message_generator.cc file for a component YYY would contain
// the following code:
//     // Get basic type definitions.
//     #define IPC_MESSAGE_IMPL
//     #include "path/to/YYY_message_generator.h"
//     // Generate constructors.
//     #include "ipc/struct_constructor_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate param traits write methods.
//     #include "ipc/param_traits_write_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//     // Generate param traits read methods.
//     #include "ipc/param_traits_read_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//     // Generate param traits log methods.
//     #include "ipc/param_traits_log_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//
// In cases where manual generation is required, in your XXX_messages.cc
// file, put the following after all the includes for param types:
//     #define IPC_MESSAGE_IMPL
//     #include "XXX_messages.h"
//        (... implementation of traits not auto-generated ...)
//
// Multiple Inclusion
//
// The XXX_messages.h file will be multiply-included by the
// YYY_message_generator.cc file, so your XXX_messages file can't be
// guarded in the usual manner.  Ideally, there will be no need for any
// inclusion guard, since the XXX_messages.h file should consist solely
// of inclusions of other headers (which are self-guarding) and IPC
// macros (which are multiply evaluating).
//
// Note that #pragma once cannot be used here; doing so would mark the whole
// file as being singly-included.  Since your XXX_messages.h file is only
// partially-guarded, care must be taken to ensure that it is only included
// by other .cc files (and the YYY_message_generator.h file).  Including an
// XXX_messages.h file in some other .h file may result in duplicate
// declarations and a compilation failure.
//
// Type Declarations
//
// It is generally a bad idea to have type definitions in a XXX_messages.h
// file; most likely the typedef will then be used in the message, as opposed
// to the struct itself.  Later, an IPC message dispatcher will need to call
// a function taking that type, and that function is declared in some other
// header.  Thus, in order to get the type definition, the other header
// would have to include the XXX_messages.h file, violating the rule above
// about not including XXX_messages.h file in other .h files.
//
// One approach here is to move these type definitions to another (guarded)
// .h file and include this second .h in your XXX_messages.h file.  This
// is still less than ideal, because the dispatched function would have to
// redeclare the typedef or include this second header.  This may be
// reasonable in a few cases.
//
// Failing all of the above, then you will want to bracket the smallest
// possible section of your XXX_messages.h file containing these types
// with an include guard macro.  Be aware that providing an incomplete
// class type declaration to avoid pulling in a long chain of headers is
// acceptable when your XXX_messages.h header is being included by the
// message sending caller's code, but not when the YYY_message_generator.c
// is building the messages. In addition, due to the multiple inclusion
// restriction, these type ought to be guarded.  Follow a convention like:
//      #ifndef SOME_GUARD_MACRO
//      #define SOME_GUARD_MACRO
//      class some_class;        // One incomplete class declaration
//      class_some_other_class;  // Another incomplete class declaration
//      #endif  // SOME_GUARD_MACRO
//      #ifdef IPC_MESSAGE_IMPL
//      #include "path/to/some_class.h"        // Full class declaration
//      #include "path/to/some_other_class.h"  // Full class declaration
//      #endif  // IPC_MESSAGE_IMPL
//        (.. IPC macros using some_class and some_other_class ...)
//
// Macro Invocations
//
// You will use IPC message macro invocations for three things:
//   - New struct definitions for IPC
//   - Registering existing struct and enum definitions with IPC
//   - Defining the messages themselves
//
// New structs are defined with IPC_STRUCT_BEGIN(), IPC_STRUCT_MEMBER(),
// IPC_STRUCT_END() family of macros.  These cause the XXX_messages.h
// to proclaim equivalent struct declarations for use by callers, as well
// as later registering the type with the message generation.  Note that
// IPC_STRUCT_MEMBER() is only permitted inside matching calls to
// IPC_STRUCT_BEGIN() / IPC_STRUCT_END(). There is also an
// IPC_STRUCT_BEGIN_WITH_PARENT(), which behaves like IPC_STRUCT_BEGIN(),
// but also accommodates structs that inherit from other structs.
//
// Externally-defined structs are registered with IPC_STRUCT_TRAITS_BEGIN(),
// IPC_STRUCT_TRAITS_MEMBER(), and IPC_STRUCT_TRAITS_END() macros. These
// cause registration of the types with message generation only.
// There's also IPC_STRUCT_TRAITS_PARENT, which is used to register a parent
// class (whose own traits are already defined). Note that
// IPC_STRUCT_TRAITS_MEMBER() and IPC_STRUCT_TRAITS_PARENT are only permitted
// inside matching calls to IPC_STRUCT_TRAITS_BEGIN() /
// IPC_STRUCT_TRAITS_END().
//
// Enum types are registered with a single IPC_ENUM_TRAITS_VALIDATE() macro.
// There is no need to enumerate each value to the IPC mechanism. Instead,
// pass an expression in terms of the parameter |value| to provide
// range-checking.  For convenience, the IPC_ENUM_TRAITS() is provided which
// performs no checking, passing everything including out-of-range values.
// Its use is discouraged. The IPC_ENUM_TRAITS_MAX_VALUE() macro can be used
// for the typical case where the enum must be in the range 0..maxvalue
// inclusive. The IPC_ENUM_TRAITS_MIN_MAX_VALUE() macro can be used for the
// less typical case where the enum must be in the range minvalue..maxvalue
// inclusive.
//
// Do not place semicolons following these IPC_ macro invocations.  There
// is no reason to expect that their expansion corresponds one-to-one with
// C++ statements.
//
// Once the types have been declared / registered, message definitions follow.
// "Sync" messages are just synchronous calls, the Send() call doesn't return
// until a reply comes back.  To declare a sync message, use the IPC_SYNC_
// macros.  The numbers at the end show how many input/output parameters there
// are (i.e. 1_2 is 1 in, 2 out).  Input parameters are first, followed by
// output parameters.  The caller uses Send([route id, ], in1, &out1, &out2).
// The receiver's handler function will be
//     void OnSyncMessageName(const type1& in1, type2* out1, type3* out2)
//
// A caller can also send a synchronous message, while the receiver can respond
// at a later time.  This is transparent from the sender's side.  The receiver
// needs to use a different handler that takes in a IPC::Message* as the output
// type, stash the message, and when it has the data it can Send the message.
//
// Use the IPC_MESSAGE_HANDLER_DELAY_REPLY macro instead of IPC_MESSAGE_HANDLER
//     IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncMessageName,
//                                     OnSyncMessageName)
// Unlike IPC_MESSAGE_HANDLER which works with IPC_BEGIN_MESSAGE_MAP as well as
// IPC_BEGIN_MESSAGE_MAP_WITH_PARAM, one needs to use
// IPC_MESSAGE_HANDLER_WITH_PARAM_DELAY_REPLY to properly handle the param.
//
// The handler function will look like:
//     void OnSyncMessageName(const type1& in1, IPC::Message* reply_msg);
//
// Receiver stashes the IPC::Message* pointer, and when it's ready, it does:
//     ViewHostMsg_SyncMessageName::WriteReplyParams(reply_msg, out1, out2);
//     Send(reply_msg);

// Files that want to export their ipc messages should do
//   #undef IPC_MESSAGE_EXPORT
//   #define IPC_MESSAGE_EXPORT VISIBILITY_MACRO
// after including this header, but before using any of the macros below.
// (This needs to be before the include guard.)
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#ifndef IPC_IPC_MESSAGE_MACROS_H_
#define IPC_IPC_MESSAGE_MACROS_H_

#include <stdint.h>

#include <tuple>

#include "base/export_template.h"
#include "base/hash/md5_constexpr.h"
#include "base/location.h"
#include "base/task/common/task_annotator.h"
#include "ipc/ipc_message_templates.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

// Convenience macro for defining structs without inheritance. Should not need
// to be subsequently redefined.
#define IPC_STRUCT_BEGIN(struct_name) \
  IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, IPC::NoParams)

// Macros for defining structs. Will be subsequently redefined.
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  struct struct_name; \
  IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  IPC_STRUCT_TRAITS_END() \
  struct IPC_MESSAGE_EXPORT struct_name : parent { \
    struct_name();
// Optional variadic parameters specify the default value for this struct
// member. They are passed through to the constructor for |type|.
#define IPC_STRUCT_MEMBER(type, name, ...) type name;
#define IPC_STRUCT_END() };

// Message macros collect arguments and funnel them into the common message
// generation macro.  These should never be redefined.

// Asynchronous messages have only in parameters and are declared like:
//     IPC_MESSAGE_CONTROL(FooMsg, int, float)
#define IPC_MESSAGE_CONTROL(msg_class, ...) \
  IPC_MESSAGE_DECL(msg_class, CONTROL, IPC_TUPLE(__VA_ARGS__), void)
#define IPC_MESSAGE_ROUTED(msg_class, ...) \
  IPC_MESSAGE_DECL(msg_class, ROUTED, IPC_TUPLE(__VA_ARGS__), void)

// Synchronous messages have both in and out parameters, so the lists need to
// be parenthesized to disambiguate:
//      IPC_SYNC_MESSAGE_CONTROL(BarMsg, (int, int), (bool))
//
// Implementation detail: The parentheses supplied by the caller for
// disambiguation are also used to trigger the IPC_TUPLE invocations below,
// so "IPC_TUPLE in" and "IPC_TUPLE out" are intentional.
#define IPC_SYNC_MESSAGE_CONTROL(msg_class, in, out) \
  IPC_MESSAGE_DECL(msg_class, CONTROL, IPC_TUPLE in, IPC_TUPLE out)
#define IPC_SYNC_MESSAGE_ROUTED(msg_class, in, out) \
  IPC_MESSAGE_DECL(msg_class, ROUTED, IPC_TUPLE in, IPC_TUPLE out)

#define IPC_TUPLE(...) IPC::CheckedTuple<__VA_ARGS__>::Tuple

#define IPC_MESSAGE_DECL(msg_name, kind, in_tuple, out_tuple)       \
  struct IPC_MESSAGE_EXPORT msg_name##_Meta {                       \
    using InTuple = in_tuple;                                       \
    using OutTuple = out_tuple;                                     \
    enum { ID = IPC_MESSAGE_ID() };                                 \
    static const IPC::MessageKind kKind = IPC::MessageKind::kind;   \
    static const char kName[];                                      \
  };                                                                \
  extern template class EXPORT_TEMPLATE_DECLARE(IPC_MESSAGE_EXPORT) \
      IPC::MessageT<msg_name##_Meta>;                               \
  using msg_name = IPC::MessageT<msg_name##_Meta>;                  \
  IPC_MESSAGE_EXTRA(msg_name)

#if defined(IPC_MESSAGE_IMPL)

// "Implementation" inclusion provides the explicit template definition
// for msg_name.
#define IPC_MESSAGE_EXTRA(msg_name)                         \
  const char msg_name##_Meta::kName[] = #msg_name;          \
  IPC_MESSAGE_DEFINE_KIND(msg_name)                         \
  template class EXPORT_TEMPLATE_DEFINE(IPC_MESSAGE_EXPORT) \
      IPC::MessageT<msg_name##_Meta>;

// MSVC has an intentionally non-compliant "feature" that results in LNK2005
// ("symbol already defined") errors if we provide an out-of-line definition
// for kKind.  Microsoft's official response is to test for _MSC_EXTENSIONS:
// https://connect.microsoft.com/VisualStudio/feedback/details/786583/
#if defined(_MSC_EXTENSIONS)
#define IPC_MESSAGE_DEFINE_KIND(msg_name)
#else
#define IPC_MESSAGE_DEFINE_KIND(msg_name) \
  const IPC::MessageKind msg_name##_Meta::kKind;
#endif

#elif defined(IPC_MESSAGE_MACROS_LOG_ENABLED)

#ifndef IPC_LOG_TABLE_ADD_ENTRY
#error You need to define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger)
#endif

// "Log table" inclusion produces extra logging registration code.
#define IPC_MESSAGE_EXTRA(msg_name)                                \
  class LoggerRegisterHelper##msg_name {                           \
   public:                                                         \
    LoggerRegisterHelper##msg_name() {                             \
      const uint32_t msg_id = static_cast<uint32_t>(msg_name::ID); \
      IPC_LOG_TABLE_ADD_ENTRY(msg_id, msg_name::Log);              \
    }                                                              \
  };                                                               \
  LoggerRegisterHelper##msg_name g_LoggerRegisterHelper##msg_name;

#else

// Normal inclusion produces nothing extra.
#define IPC_MESSAGE_EXTRA(msg_name)

#endif  // defined(IPC_MESSAGE_IMPL)

// Message IDs
// Note: we currently use __LINE__ to give unique IDs to messages within
// a file.  They're globally unique since each file defines its own
// IPC_MESSAGE_START.
#define IPC_MESSAGE_ID() ((IPC_MESSAGE_START << 16) + __LINE__)
#define IPC_MESSAGE_ID_CLASS(id) ((id) >> 16)
#define IPC_MESSAGE_ID_LINE(id) ((id) & 0xffff)

// Message crackers and handlers. Usage:
//
//   bool MyClass::OnMessageReceived(const IPC::Message& msg) {
//     bool handled = true;
//     IPC_BEGIN_MESSAGE_MAP(MyClass, msg)
//       IPC_MESSAGE_HANDLER(MsgClassOne, OnMsgClassOne)
//       ...more handlers here ...
//       IPC_MESSAGE_HANDLER(MsgClassTen, OnMsgClassTen)
//       IPC_MESSAGE_UNHANDLED(handled = false)
//     IPC_END_MESSAGE_MAP()
//     return handled;
//   }

#define IPC_TASK_ANNOTATOR_STRINGIFY(s) #s

// A macro to be used from within the IPC_MESSAGE_FORWARD macros, for providing
// the IPC message context to the TaskAnnotator. This allows posted tasks to be
// associated with the incoming IPC message that caused them to be posted.
#define IPC_TASK_ANNOTATOR_CONTEXT(msg_class)                            \
  static constexpr uint32_t kMessageHash =                               \
      base::MD5Hash32Constexpr(IPC_TASK_ANNOTATOR_STRINGIFY(msg_class)); \
  base::TaskAnnotator::ScopedSetIpcHash scoped_ipc_hash(kMessageHash);

#define IPC_BEGIN_MESSAGE_MAP(class_name, msg) \
  { \
    typedef class_name _IpcMessageHandlerClass ALLOW_UNUSED_TYPE; \
    void* param__ = NULL; \
    (void)param__; \
    const IPC::Message& ipc_message__ = msg; \
    switch (ipc_message__.type()) {

#define IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(class_name, msg, param)  \
  {                                                               \
    typedef class_name _IpcMessageHandlerClass ALLOW_UNUSED_TYPE; \
    decltype(param) param__ = param;                              \
    const IPC::Message& ipc_message__ = msg;                      \
    switch (ipc_message__.type()) {
#define IPC_MESSAGE_FORWARD(msg_class, obj, member_func)         \
  case msg_class::ID: {                                          \
    IPC_TASK_ANNOTATOR_CONTEXT(msg_class)                        \
    if (!msg_class::Dispatch(&ipc_message__, obj, this, param__, \
                             &member_func))                      \
      ipc_message__.set_dispatch_error();                        \
  } break;

#define IPC_MESSAGE_HANDLER(msg_class, member_func) \
  IPC_MESSAGE_FORWARD(msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, obj, member_func) \
  case msg_class::ID: {                                              \
    IPC_TASK_ANNOTATOR_CONTEXT(msg_class)                            \
    if (!msg_class::DispatchDelayReply(&ipc_message__, obj, param__, \
                                       &member_func))                \
      ipc_message__.set_dispatch_error();                            \
  } break;

#define IPC_MESSAGE_HANDLER_DELAY_REPLY(msg_class, member_func)                \
    IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, this,                           \
                                    _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_FORWARD_WITH_PARAM_DELAY_REPLY(msg_class, obj,         \
                                                   member_func)            \
  case msg_class::ID: {                                                    \
    IPC_TASK_ANNOTATOR_CONTEXT(msg_class)                                  \
    if (!msg_class::DispatchWithParamDelayReply(&ipc_message__, obj,       \
                                                param__, \ & member_func)) \
      ipc_message__.set_dispatch_error();                                  \
  } break;

#define IPC_MESSAGE_HANDLER_WITH_PARAM_DELAY_REPLY(msg_class, member_func)     \
    IPC_MESSAGE_FORWARD_WITH_PARAM_DELAY_REPLY(                                \
        msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_HANDLER_GENERIC(msg_class, code) \
  case msg_class::ID: {                              \
    IPC_TASK_ANNOTATOR_CONTEXT(msg_class) { code; }  \
  } break;

#define IPC_REPLY_HANDLER(func)                                                \
    case IPC_REPLY_ID: {                                                       \
        func(ipc_message__);                                                   \
      }                                                                        \
      break;


#define IPC_MESSAGE_UNHANDLED(code)                                            \
    default: {                                                                 \
        code;                                                                  \
      }                                                                        \
      break;

#define IPC_MESSAGE_UNHANDLED_ERROR() \
  IPC_MESSAGE_UNHANDLED(NOTREACHED() << \
                              "Invalid message with type = " << \
                              ipc_message__.type())

#define IPC_END_MESSAGE_MAP() \
  } \
}

// This corresponds to an enum value from IPCMessageStart.
#define IPC_MESSAGE_CLASS(message) IPC_MESSAGE_ID_CLASS((message).type())

// Deprecated legacy macro names.
// TODO(mdempsky): Replace uses with generic names.

#define IPC_MESSAGE_CONTROL0(msg) IPC_MESSAGE_CONTROL(msg)
#define IPC_MESSAGE_CONTROL1(msg, a) IPC_MESSAGE_CONTROL(msg, a)
#define IPC_MESSAGE_CONTROL2(msg, a, b) IPC_MESSAGE_CONTROL(msg, a, b)
#define IPC_MESSAGE_CONTROL3(msg, a, b, c) IPC_MESSAGE_CONTROL(msg, a, b, c)
#define IPC_MESSAGE_CONTROL4(msg, a, b, c, d) \
  IPC_MESSAGE_CONTROL(msg, a, b, c, d)
#define IPC_MESSAGE_CONTROL5(msg, a, b, c, d, e) \
  IPC_MESSAGE_CONTROL(msg, a, b, c, d, e)

#define IPC_MESSAGE_ROUTED0(msg) IPC_MESSAGE_ROUTED(msg)
#define IPC_MESSAGE_ROUTED1(msg, a) IPC_MESSAGE_ROUTED(msg, a)
#define IPC_MESSAGE_ROUTED2(msg, a, b) IPC_MESSAGE_ROUTED(msg, a, b)
#define IPC_MESSAGE_ROUTED3(msg, a, b, c) IPC_MESSAGE_ROUTED(msg, a, b, c)
#define IPC_MESSAGE_ROUTED4(msg, a, b, c, d) IPC_MESSAGE_ROUTED(msg, a, b, c, d)
#define IPC_MESSAGE_ROUTED5(msg, a, b, c, d, e) \
  IPC_MESSAGE_ROUTED(msg, a, b, c, d, e)

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg) IPC_SYNC_MESSAGE_CONTROL(msg, (), ())
#define IPC_SYNC_MESSAGE_CONTROL0_1(msg, a) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (), (a))
#define IPC_SYNC_MESSAGE_CONTROL0_2(msg, a, b) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (), (a, b))
#define IPC_SYNC_MESSAGE_CONTROL0_3(msg, a, b, c) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (), (a, b, c))
#define IPC_SYNC_MESSAGE_CONTROL0_4(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (), (a, b, c, d))
#define IPC_SYNC_MESSAGE_CONTROL1_0(msg, a) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a), ())
#define IPC_SYNC_MESSAGE_CONTROL1_1(msg, a, b) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a), (b))
#define IPC_SYNC_MESSAGE_CONTROL1_2(msg, a, b, c) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a), (b, c))
#define IPC_SYNC_MESSAGE_CONTROL1_3(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a), (b, c, d))
#define IPC_SYNC_MESSAGE_CONTROL1_4(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a), (b, c, d, e))
#define IPC_SYNC_MESSAGE_CONTROL2_0(msg, a, b) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b), ())
#define IPC_SYNC_MESSAGE_CONTROL2_1(msg, a, b, c) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b), (c))
#define IPC_SYNC_MESSAGE_CONTROL2_2(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b), (c, d))
#define IPC_SYNC_MESSAGE_CONTROL2_3(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b), (c, d, e))
#define IPC_SYNC_MESSAGE_CONTROL2_4(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b), (c, d, e, f))
#define IPC_SYNC_MESSAGE_CONTROL3_0(msg, a, b, c) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c), ())
#define IPC_SYNC_MESSAGE_CONTROL3_1(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c), (d))
#define IPC_SYNC_MESSAGE_CONTROL3_2(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c), (d, e))
#define IPC_SYNC_MESSAGE_CONTROL3_3(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c), (d, e, f))
#define IPC_SYNC_MESSAGE_CONTROL3_4(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c), (d, e, f, g))
#define IPC_SYNC_MESSAGE_CONTROL4_0(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d), ())
#define IPC_SYNC_MESSAGE_CONTROL4_1(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d), (e))
#define IPC_SYNC_MESSAGE_CONTROL4_2(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d), (e, f))
#define IPC_SYNC_MESSAGE_CONTROL4_3(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d), (e, f, g))
#define IPC_SYNC_MESSAGE_CONTROL4_4(msg, a, b, c, d, e, f, g, h) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d), (e, f, g, h))
#define IPC_SYNC_MESSAGE_CONTROL5_0(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d, e), ())
#define IPC_SYNC_MESSAGE_CONTROL5_1(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d, e), (f))
#define IPC_SYNC_MESSAGE_CONTROL5_2(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d, e), (f, g))
#define IPC_SYNC_MESSAGE_CONTROL5_3(msg, a, b, c, d, e, f, g, h) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d, e), (f, g, h))
#define IPC_SYNC_MESSAGE_CONTROL5_4(msg, a, b, c, d, e, f, g, h, i) \
  IPC_SYNC_MESSAGE_CONTROL(msg, (a, b, c, d, e), (f, g, h, i))

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg) IPC_SYNC_MESSAGE_ROUTED(msg, (), ())
#define IPC_SYNC_MESSAGE_ROUTED0_1(msg, a) IPC_SYNC_MESSAGE_ROUTED(msg, (), (a))
#define IPC_SYNC_MESSAGE_ROUTED0_2(msg, a, b) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (), (a, b))
#define IPC_SYNC_MESSAGE_ROUTED0_3(msg, a, b, c) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (), (a, b, c))
#define IPC_SYNC_MESSAGE_ROUTED0_4(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (), (a, b, c, d))
#define IPC_SYNC_MESSAGE_ROUTED1_0(msg, a) IPC_SYNC_MESSAGE_ROUTED(msg, (a), ())
#define IPC_SYNC_MESSAGE_ROUTED1_1(msg, a, b) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a), (b))
#define IPC_SYNC_MESSAGE_ROUTED1_2(msg, a, b, c) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a), (b, c))
#define IPC_SYNC_MESSAGE_ROUTED1_3(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a), (b, c, d))
#define IPC_SYNC_MESSAGE_ROUTED1_4(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a), (b, c, d, e))
#define IPC_SYNC_MESSAGE_ROUTED2_0(msg, a, b) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b), ())
#define IPC_SYNC_MESSAGE_ROUTED2_1(msg, a, b, c) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b), (c))
#define IPC_SYNC_MESSAGE_ROUTED2_2(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b), (c, d))
#define IPC_SYNC_MESSAGE_ROUTED2_3(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b), (c, d, e))
#define IPC_SYNC_MESSAGE_ROUTED2_4(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b), (c, d, e, f))
#define IPC_SYNC_MESSAGE_ROUTED3_0(msg, a, b, c) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c), ())
#define IPC_SYNC_MESSAGE_ROUTED3_1(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c), (d))
#define IPC_SYNC_MESSAGE_ROUTED3_2(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c), (d, e))
#define IPC_SYNC_MESSAGE_ROUTED3_3(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c), (d, e, f))
#define IPC_SYNC_MESSAGE_ROUTED3_4(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c), (d, e, f, g))
#define IPC_SYNC_MESSAGE_ROUTED4_0(msg, a, b, c, d) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d), ())
#define IPC_SYNC_MESSAGE_ROUTED4_1(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d), (e))
#define IPC_SYNC_MESSAGE_ROUTED4_2(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d), (e, f))
#define IPC_SYNC_MESSAGE_ROUTED4_3(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d), (e, f, g))
#define IPC_SYNC_MESSAGE_ROUTED4_4(msg, a, b, c, d, e, f, g, h) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d), (e, f, g, h))
#define IPC_SYNC_MESSAGE_ROUTED5_0(msg, a, b, c, d, e) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d, e), ())
#define IPC_SYNC_MESSAGE_ROUTED5_1(msg, a, b, c, d, e, f) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d, e), (f))
#define IPC_SYNC_MESSAGE_ROUTED5_2(msg, a, b, c, d, e, f, g) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d, e), (f, g))
#define IPC_SYNC_MESSAGE_ROUTED5_3(msg, a, b, c, d, e, f, g, h) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d, e), (f, g, h))
#define IPC_SYNC_MESSAGE_ROUTED5_4(msg, a, b, c, d, e, f, g, h, i) \
  IPC_SYNC_MESSAGE_ROUTED(msg, (a, b, c, d, e), (f, g, h, i))

#endif  // IPC_IPC_MESSAGE_MACROS_H_

// Clean up IPC_MESSAGE_START in this unguarded section so that the
// XXX_messages.h files need not do so themselves.  This makes the
// XXX_messages.h files easier to write.
#undef IPC_MESSAGE_START
