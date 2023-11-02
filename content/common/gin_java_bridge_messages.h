// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GIN_JAVA_BRIDGE_MESSAGES_H_
#define CONTENT_COMMON_GIN_JAVA_BRIDGE_MESSAGES_H_

// IPC messages for injected Java objects (Gin-based implementation).

#include <stdint.h>

#include "content/common/android/gin_java_bridge_errors.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START GinJavaBridgeMsgStart

// Messages for handling Java objects injected into JavaScript -----------------

IPC_ENUM_TRAITS_MAX_VALUE(content::GinJavaBridgeError,
                          content::kGinJavaBridgeErrorLast)

// Sent from browser to renderer to add a Java object with the given name.
// Object IDs are generated on the browser side.
IPC_MESSAGE_ROUTED2(GinJavaBridgeMsg_AddNamedObject,
                    std::string /* name */,
                    int32_t /* object_id */)

// Sent from browser to renderer to remove a Java object with the given name.
IPC_MESSAGE_ROUTED1(GinJavaBridgeMsg_RemoveNamedObject,
                    std::string /* name */)

// Sent from renderer to browser to get information about methods of
// the given object. The query will only succeed if inspection of injected
// objects is enabled on the browser side.
IPC_SYNC_MESSAGE_ROUTED1_1(GinJavaBridgeHostMsg_GetMethods,
                           int32_t /* object_id */,
                           std::set<std::string> /* returned_method_names */)

// Sent from renderer to browser to find out, if an object has a method with
// the given name.
IPC_SYNC_MESSAGE_ROUTED2_1(GinJavaBridgeHostMsg_HasMethod,
                           int32_t /* object_id */,
                           std::string /* method_name */,
                           bool /* result */)

// Sent from renderer to browser to invoke a method. Method arguments
// are chained into |arguments| list. base::Value::List is used for |result| as
// a container to work around immutability of base::Value.
// Empty result list indicates that an error has happened on the Java side
// (either bridge-induced error or an unhandled Java exception) and an exception
// must be thrown into JavaScript. |error_code| indicates the cause of
// the error.
// Some special value types that are not supported by base::Value are encoded
// as BinaryValues via GinJavaBridgeValue.
IPC_SYNC_MESSAGE_ROUTED3_2(GinJavaBridgeHostMsg_InvokeMethod,
                           int32_t /* object_id */,
                           std::string /* method_name */,
                           base::Value::List /* arguments */,
                           base::Value::List /* result */,
                           content::GinJavaBridgeError /* error_code */)

// Sent from renderer to browser in two cases:
//
//  1. (Main usage) To inform that the JS wrapper of the object has
//     been completely dereferenced and garbage-collected.
//
//  2. To notify the browser that wrapper creation has failed.  The browser side
//     assumes optimistically that every time an object is returned from a
//     method, the corresponding wrapper object will be successfully created on
//     the renderer side. Sending of this message informs the browser whether
//     this expectation has failed.
IPC_MESSAGE_ROUTED1(GinJavaBridgeHostMsg_ObjectWrapperDeleted,
                    int32_t /* object_id */)

#endif  // CONTENT_COMMON_GIN_JAVA_BRIDGE_MESSAGES_H_
