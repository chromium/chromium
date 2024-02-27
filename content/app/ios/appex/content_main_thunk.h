// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_IOS_APPEX_CONTENT_MAIN_THUNK_H_
#define CONTENT_APP_IOS_APPEX_CONTENT_MAIN_THUNK_H_

#import <Foundation/Foundation.h>
#import <xpc/xpc.h>

#ifdef __cplusplus
extern "C" {
#endif

void ContentProcessInit();
void ContentProcessHandleNewConnection(xpc_connection_t);

#ifdef __cplusplus
}
#endif

#endif  // CONTENT_APP_IOS_APPEX_CONTENT_MAIN_THUNK_H_
