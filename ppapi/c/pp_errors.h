/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_errors.idl modified Tue Sep 23 15:37:27 2014. */

#ifndef PPAPI_C_PP_ERRORS_H_
#define PPAPI_C_PP_ERRORS_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * This file defines an enumeration of all PPAPI error codes.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains enumerators of all PPAPI error codes.
 *
 * Errors are negative valued. Callers should treat all negative values as a
 * failure, even if it's not in the list, since the possible errors are likely
 * to expand and change over time.
 */
enum {
  /**
   * This value is returned by a function on successful synchronous completion
   * or is passed as a result to a PP_CompletionCallback_Func on successful
   * asynchronous completion.
   */
  PP_OK = 0,
  /**
   * This value is returned by a function that accepts a PP_CompletionCallback
   * and cannot complete synchronously. This code indicates that the given
   * callback will be asynchronously notified of the final result once it is
   * available.
   */
  PP_OK_COMPLETIONPENDING = -1,
  /**This value indicates failure for unspecified reasons. */
  PP_ERROR_FAILED = -2,
  /**
   * This value indicates failure due to an asynchronous operation being
   * interrupted. The most common cause of this error code is destroying a
   * resource that still has a callback pending. All callbacks are guaranteed
   * to execute, so any callbacks pending on a destroyed resource will be
   * issued with PP_ERROR_ABORTED.
   *
   * If you get an aborted notification that you aren't expecting, check to
   * make sure that the resource you're using is still in scope. A common
   * mistake is to create a resource on the stack, which will destroy the
   * resource as soon as the function returns.
   */
  PP_ERROR_ABORTED = -3,
  /** This value indicates failure due to an invalid argument. */
  PP_ERROR_BADARGUMENT = -4,
  /** This value indicates failure due to an invalid PP_Resource. */
  PP_ERROR_BADRESOURCE = -5,
  /** This value indicates failure due to an unavailable PPAPI interface. */
  PP_ERROR_NOINTERFACE = -6,
  /** This value indicates failure due to insufficient privileges. */
  PP_ERROR_NOACCESS = -7,
  /** This value indicates failure due to insufficient memory. */
  PP_ERROR_NOMEMORY = -8,
  /** This value indicates failure due to insufficient storage space. */
  PP_ERROR_NOSPACE = -9,
  /** This value indicates failure due to insufficient storage quota. */
  PP_ERROR_NOQUOTA = -10,
  /**
   * This value indicates failure due to an action already being in
   * progress.
   */
  PP_ERROR_INPROGRESS = -11,
  /**
   * The requested command is not supported by the browser.
   */
  PP_ERROR_NOTSUPPORTED = -12,
  /**
   * Returned if you try to use a null completion callback to "block until
   * complete" on the main thread. Blocking the main thread is not permitted
   * to keep the browser responsive (otherwise, you may not be able to handle
   * input events, and there are reentrancy and deadlock issues).
   */
  PP_ERROR_BLOCKS_MAIN_THREAD = -13,
  /**
   * This value indicates that the plugin sent bad input data to a resource,
   * leaving it in an invalid state. The resource can't be used after returning
   * this error and should be released.
   */
  PP_ERROR_MALFORMED_INPUT = -14,
  /**
   * This value indicates that a resource has failed.  The resource can't be
   * used after returning this error and should be released.
   */
  PP_ERROR_RESOURCE_FAILED = -15,
  /** This value indicates failure due to a file that does not exist. */
  PP_ERROR_FILENOTFOUND = -20,
  /** This value indicates failure due to a file that already exists. */
  PP_ERROR_FILEEXISTS = -21,
  /** This value indicates failure due to a file that is too big. */
  PP_ERROR_FILETOOBIG = -22,
  /**
   * This value indicates failure due to a file having been modified
   * unexpectedly.
   */
  PP_ERROR_FILECHANGED = -23,
  /** This value indicates that the pathname does not reference a file. */
  PP_ERROR_NOTAFILE = -24,
  /** This value indicates failure due to a time limit being exceeded. */
  PP_ERROR_TIMEDOUT = -30,
  /**
   * This value indicates that the user cancelled rather than providing
   * expected input.
   */
  PP_ERROR_USERCANCEL = -40,
  /**
   * This value indicates failure due to lack of a user gesture such as a
   * mouse click or key input event. Examples of actions requiring a user
   * gesture are showing the file chooser dialog and going into fullscreen
   * mode.
   */
  PP_ERROR_NO_USER_GESTURE = -41,
  /**
   * This value indicates that the graphics context was lost due to a
   * power management event.
   */
  PP_ERROR_CONTEXT_LOST = -50,
  /**
   * Indicates an attempt to make a PPAPI call on a thread without previously
   * registering a message loop via PPB_MessageLoop.AttachToCurrentThread.
   * Without this registration step, no PPAPI calls are supported.
   */
  PP_ERROR_NO_MESSAGE_LOOP = -51,
  /**
   * Indicates that the requested operation is not permitted on the current
   * thread.
   */
  PP_ERROR_WRONG_THREAD = -52,
  /**
   * Indicates that a null completion callback was used on a thread handling a
   * blocking message from JavaScript. Null completion callbacks "block until
   * complete", which could cause the main JavaScript thread to be blocked
   * excessively.
   */
  PP_ERROR_WOULD_BLOCK_THREAD = -53,
  /**
   * This value indicates that the connection was closed. For TCP sockets, it
   * corresponds to a TCP FIN.
   */
  PP_ERROR_CONNECTION_CLOSED = -100,
  /**
   * This value indicates that the connection was reset. For TCP sockets, it
   * corresponds to a TCP RST.
   */
  PP_ERROR_CONNECTION_RESET = -101,
  /**
   * This value indicates that the connection attempt was refused.
   */
  PP_ERROR_CONNECTION_REFUSED = -102,
  /**
   * This value indicates that the connection was aborted. For TCP sockets, it
   * means the connection timed out as a result of not receiving an ACK for data
   * sent. This can include a FIN packet that did not get ACK'd.
   */
  PP_ERROR_CONNECTION_ABORTED = -103,
  /**
   * This value indicates that the connection attempt failed.
   */
  PP_ERROR_CONNECTION_FAILED = -104,
  /**
   * This value indicates that the connection attempt timed out.
   */
  PP_ERROR_CONNECTION_TIMEDOUT = -105,
  /**
   * This value indicates that the IP address or port number is invalid.
   */
  PP_ERROR_ADDRESS_INVALID = -106,
  /**
   * This value indicates that the IP address is unreachable. This usually means
   * that there is no route to the specified host or network.
   */
  PP_ERROR_ADDRESS_UNREACHABLE = -107,
  /**
   * This value is returned when attempting to bind an address that is already
   * in use.
   */
  PP_ERROR_ADDRESS_IN_USE = -108,
  /**
   * This value indicates that the message was too large for the transport.
   */
  PP_ERROR_MESSAGE_TOO_BIG = -109,
  /**
   * This value indicates that the host name could not be resolved.
   */
  PP_ERROR_NAME_NOT_RESOLVED = -110
};
/**
 * @}
 */

#endif  /* PPAPI_C_PP_ERRORS_H_ */

