// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_SUPPORT_EXPORT_H_
#define IPC_IPC_MESSAGE_SUPPORT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(IPC_MESSAGE_SUPPORT_IMPL)
#define IPC_MESSAGE_SUPPORT_EXPORT __declspec(dllexport)
#else
#define IPC_MESSAGE_SUPPORT_EXPORT __declspec(dllimport)
#endif  // defined(IPC_MESSAGE_SUPPORT_IMPL)

#else  // defined(WIN32)

#if defined(IPC_MESSAGE_SUPPORT_IMPL)
#define IPC_MESSAGE_SUPPORT_EXPORT __attribute__((visibility("default")))
#else
#define IPC_MESSAGE_SUPPORT_EXPORT
#endif

#endif

#else  // defined(COMPONENT_BUILD)
#define IPC_MESSAGE_SUPPORT_EXPORT
#endif

#endif  // IPC_IPC_MESSAGE_SUPPORT_EXPORT_H_
