/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_PPAPI_SIMPLE_PS_H_
#define LIBRARIES_PPAPI_SIMPLE_PS_H_

#include "ppapi/c/pp_instance.h"
#include "sdk_util/macros.h"

EXTERN_C_BEGIN

/**
 * The ppapi_simple library simplifies the use of the Pepper interfaces by
 * providing a more traditional 'C' or 'C++' style framework.  The library
 * creates an PSInstance derived object based on the ppapi_cpp library and
 * initializes the nacl_io library to provide a POSIX friendly I/O environment.
 *
 * In order to provide a standard blocking environment, the library will hide
 * the actual "Pepper Thread" which is the thread that standard events
 * such as window resize, mouse keyboard, or other inputs arrive.  To prevent
 * blocking, instead we enqueue these events onto a thread safe linked list
 * and expect them to be processed on a new thread.  In addition, the library
 * will automatically start a new thread on which can be used effectively
 * as a "main" entry point.
 *
 * For C style development, ppapi_simple allows applications to be written using
 * a traditaional "main".  All events are pushed onto an event queue which can
 * then be pulled from this new thread.
 * NOTE: The link will still need libstdc++ and libppapi_cpp since the library
 * is still creating a C++ object which does the initialization work and
 * forwards the events.
 *
 * For C++ style development, use the ppapi_simple_instance.h,
 * ppapi_simple_instance_2d.h, and ppapi_simple_instance_3d.h headers as
 * a base class, and overload the appropriate virtual functions such as
 * Main, ChangeContext, or Render.
 */

/**
 * PSGetInstanceId
 *
 * Return the PP_Instance id of this instance of the module.  This is required
 * by most of the Pepper resource creation routines.
 */
PP_Instance PSGetInstanceId(void);

/**
 * PSGetInterface
 *
 * Return the Pepper instance referred to by 'name'.  Will return a pointer
 * to the interface, or NULL if not found or not available.
 */
const void* PSGetInterface(const char *name);

EXTERN_C_END

#endif  // LIBRARIES_PPAPI_SIMPLE_PS_H_
