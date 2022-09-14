// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/nacl_io.h"

#include <stdlib.h>
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

int nacl_io_init() {
  return ki_init(NULL);
}

int nacl_io_uninit() {
  return ki_uninit();
}

int nacl_io_init_ppapi(PP_Instance instance, PPB_GetInterface get_interface) {
  return ki_init_ppapi(NULL, instance, get_interface);
}

int nacl_io_register_fs_type(const char* fs_type, fuse_operations* fuse_ops) {
  return ki_get_proxy()->RegisterFsType(fs_type, fuse_ops);
}

int nacl_io_unregister_fs_type(const char* fs_type) {
  return ki_get_proxy()->UnregisterFsType(fs_type);
}

void nacl_io_set_exit_callback(nacl_io_exit_callback_t exit_callback,
                               void* user_data) {
  ki_get_proxy()->SetExitCallback(exit_callback, user_data);
}

void nacl_io_set_mount_callback(nacl_io_mount_callback_t callback,
                                void* user_data) {
  ki_get_proxy()->SetMountCallback(callback, user_data);
}
