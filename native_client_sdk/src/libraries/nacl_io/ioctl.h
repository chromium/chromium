/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_IOCTL_H_
#define LIBRARIES_NACL_IO_IOCTL_H_

#include <sys/types.h>

/*
 * ioctl to register an output handler with the tty node.  Will fail with
 * EALREADY if a handler is already registered.  Expects an argument of type
 * tioc_nacl_output.  The handler will be called during calls to write() on the
 * thread that calls write(), or, for echoed input during the
 * NACL_IOC_HANDLEMESSAGE ioctl() on the thread calling ioctl(). The handler
 * should return the number of bytes written/handled, or -errno if an error
 * occurred.
 */
#define TIOCNACLOUTPUT 0xadcd03

/*
 * ioctl used to set a name for a JavaScript pipe.  The name
 * is a string that is used to uniquely identify messages posted to and from
 * JavaScript which signifies that the message is destined for a
 * particular pipe device.  For this reason each device must have a
 * unique prefix.  Until a prefix is set on a given pipe any I/O operations
 * will return EIO.
 */
#define NACL_IOC_PIPE_SETNAME 0xadcd04

/*
 * Find out how much space is available in a nacl_io pipe.
 * Argument type is "int*" which will be set to the amount of space in the
 * pipe in bytes.
 */
#define NACL_IOC_PIPE_GETOSPACE 0xadcd06
#define NACL_IOC_PIPE_GETISPACE 0xadcd07

/*
 * ioctl used to pass messages from JavaScript to a node.
 * Argument type is "struct PP_Var*".
 */
#define NACL_IOC_HANDLEMESSAGE 0xadcd05

typedef char* naclioc_jspipe_name;

typedef ssize_t (*tioc_nacl_output_handler_t)(const char* buf,
                                              size_t count,
                                              void* user_data);

struct tioc_nacl_output {
  tioc_nacl_output_handler_t handler;
  void* user_data;
};


#endif  // LIBRARIES_NACL_IO_IOCTL_H_
