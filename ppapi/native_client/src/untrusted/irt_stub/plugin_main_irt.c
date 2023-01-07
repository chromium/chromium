/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/shared/ppapi_proxy/ppruntime.h"

/*
 * An application that doesn't define its own main but links in -lppapi
 * gets this one.  A plugin may instead have its own main that calls
 * PpapiPluginMain (or PpapiPluginStart) after doing some other setup.
 */
int main(void) {
  return PpapiPluginMain();
}
