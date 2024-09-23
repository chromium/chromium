// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_IOS_APPEX_CHILD_PROCESS_BRIDGE_H_
#define CONTENT_APP_IOS_APPEX_CHILD_PROCESS_BRIDGE_H_

#import <Foundation/Foundation.h>
#import <xpc/xpc.h>

#ifdef __cplusplus
extern "C" {
#endif

void ChildProcessInit();
void ChildProcessHandleNewConnection(xpc_connection_t);

#ifdef __cplusplus
}
#endif

#endif  // CONTENT_APP_IOS_APPEX_CHILD_PROCESS_BRIDGE_H_
